#include "stdafx.h"
#include "UI/HuxQt.h"
#include "ui_HuxQt.h"

#include "AppCore.h"

#include "Scenario/ScenarioManager.h"
#include "Scenario/Scenario.h"

#include "UI/DisplaySystem.h"
#include "UI/DisplayData.h"

#include "UI/AddLevelDialog.h"
#include "UI/TerminalEditorWindow.h"
#include "UI/ExportScenarioDialog.h"
#include "UI/PreviewConfigWindow.h"

#include "Utils/Utilities.h"

#include <QMessageBox>

namespace HuxApp
{
    namespace
    {
        constexpr const char* TELEPORT_TYPE_LABELS[Utils::to_integral(Terminal::TeleportType::TYPE_COUNT)] =
        {
            "NONE",
            "INTERLEVEL",
            "INTRALEVEL"
        };

        constexpr const char* SCREEN_TYPE_LABELS[Utils::to_integral(Terminal::ScreenType::TYPE_COUNT)] = {
            "NONE",
            "LOGON",
            "INFORMATION",
            "PICT",
            "CHECKPOINT",
            "LOGOFF",
            "TAG",
            "STATIC"
        };

        enum class ScenarioBrowserState
        {
            LEVELS,
            TERMINALS
        };

        QString generate_terminal_editor_title(const Level& level_data, const Terminal& terminal_data, int terminal_index)
        {
            return QStringLiteral("%1 / TERMINAL %2 (%3)").arg(level_data.get_name()).arg(terminal_index).arg(terminal_data.get_id());
        }
    }

    struct HuxQt::Internal
    {
        Ui::HuxQtMainWindow m_ui;

        Scenario m_scenario;

        ScenarioBrowserState m_scenario_browser_state = ScenarioBrowserState::LEVELS;
        int m_current_level_index = -1;

        std::vector<TerminalEditorWindow*> m_terminal_editors;
        PreviewConfigWindow* m_preview_config = nullptr;

        std::vector<Terminal> m_terminal_clipboard;

        DisplaySystem::ViewID m_view_id;

        void clear_scenario_browser()
        {
            m_ui.scenario_browser_view->clear();
        }

        void init_level_item(const Level& level_data, QListWidgetItem* level_item)
        {
            level_item->setText(level_data.get_name());
            level_item->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogStart));

            if (level_data.is_modified())
            {
                QFont font = level_item->font();
                font.setBold(true);
                level_item->setFont(font);
            }
        }

        void init_terminal_item(const Terminal& terminal, int terminal_index, QListWidgetItem* terminal_item)
        {
            terminal_item->setText(QStringLiteral("TERMINAL %1 (%2)").arg(terminal_index).arg(terminal.get_id()));
            terminal_item->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_ComputerIcon));
            terminal_item->setData(Qt::UserRole, terminal.get_id()); // Set ID so we can map between the item and the underlying data

            QFont font = terminal_item->font();
            font.setBold(terminal.is_modified());
            terminal_item->setFont(font);
        }

        int get_terminal_index(QListWidgetItem* terminal_item)
        {
            assert(m_scenario_browser_state == ScenarioBrowserState::TERMINALS);
            return m_ui.scenario_browser_view->row(terminal_item);
        }

        int find_terminal_data_index(int terminal_id) const
        {
            assert(m_scenario_browser_state == ScenarioBrowserState::TERMINALS);
            const Level& current_level = m_scenario.get_level(m_current_level_index);
            return current_level.find_terminal(terminal_id);
        }

        void reset_scenario_browser_view()
        {
            // Reset view
            clear_scenario_browser();
            reset_terminal_ui();

            if (m_scenario_browser_state == ScenarioBrowserState::LEVELS)
            {
                // Change selection and drag & drop mode
                m_ui.scenario_browser_view->setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
                m_ui.scenario_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

                // List the levels
                for (const Level& current_level : m_scenario.get_levels())
                {
                    QListWidgetItem* level_item = new QListWidgetItem(m_ui.scenario_browser_view);
                    init_level_item(current_level, level_item);
                }
                m_ui.scenario_up_button->setEnabled(false);
            }
            else
            {
                // Change selection and drag & drop mode
                m_ui.scenario_browser_view->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);
                m_ui.scenario_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);

                // List the terminals
                const Level& level_data = m_scenario.get_level(m_current_level_index);
                int current_terminal_index = 0;
                for (const Terminal& current_terminal : level_data.get_terminals())
                {
                    QListWidgetItem* terminal_item = new QListWidgetItem(m_ui.scenario_browser_view);
                    init_terminal_item(current_terminal, current_terminal_index, terminal_item);

                    // Update the terminal editor (if applicable)
                    const int editor_index = find_terminal_editor(current_terminal);
                    if (editor_index >= 0)
                    {
                        // We can get the terminal ID from the scenario item index in its parent
                        TerminalEditorWindow* editor_window = m_terminal_editors[editor_index];
                        editor_window->update_window_title(generate_terminal_editor_title(level_data, current_terminal, current_terminal_index));
                    }
                    ++current_terminal_index;
                }
                m_ui.scenario_up_button->setEnabled(true);
            }
        }

        void reorder_terminal_items()
        {
            // Cache the new ID order and the items that moved
            std::vector<int> new_id_list;
            std::unordered_set<int> moved_ids;
            for (int current_row = 0; current_row < m_ui.scenario_browser_view->count(); ++current_row)
            {
                QListWidgetItem* current_terminal_item = m_ui.scenario_browser_view->item(current_row);
                const int terminal_id = current_terminal_item->data(Qt::UserRole).toInt();

                // Assume selected items were those that we moved (via drag & drop)
                if (current_terminal_item->isSelected())
                {
                    moved_ids.insert(terminal_id);
                }
                new_id_list.push_back(terminal_id);
            }

            // Reorder the data
            Level& current_level = m_scenario.get_level(m_current_level_index);
            ScenarioManager::reorder_level_terminals(current_level, new_id_list, moved_ids);

            // Reset the view to show the new order
            reset_scenario_browser_view();

            // Re-select the items that were moved
            for (int current_row = 0; current_row < m_ui.scenario_browser_view->count(); ++current_row)
            {
                QListWidgetItem* current_terminal_item = m_ui.scenario_browser_view->item(current_row);
                const int terminal_id = current_terminal_item->data(Qt::UserRole).toInt();
                if (moved_ids.find(terminal_id) != moved_ids.end())
                {
                    current_terminal_item->setSelected(true);
                }
            }

            scenario_edited();
        }

        void display_scenario_levels()
        {
            m_scenario_browser_state = ScenarioBrowserState::LEVELS;
            m_current_level_index = -1;
            m_ui.current_level_label->clear();
            reset_scenario_browser_view();
        }

        void open_level(QListWidgetItem* level_item)
        {
            // Store the current level index
            m_current_level_index = m_ui.scenario_browser_view->row(level_item);           
            m_scenario_browser_state = ScenarioBrowserState::TERMINALS;
            m_ui.current_level_label->setText(level_item->text());
            reset_scenario_browser_view();
        }
        
        void init_screen_group_item(QTreeWidgetItem* screen_group_item, bool unfinished)
        {
            if (unfinished)
            {
                screen_group_item->setText(0, QStringLiteral("UNFINISHED"));
            }
            else
            {
                screen_group_item->setText(0, QStringLiteral("FINISHED"));
            }
            screen_group_item->setIcon(0, QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_DirIcon));
        }

        void init_screen_item(const Terminal::Screen& screen_data, QTreeWidgetItem* screen_item)
        {
            screen_item->setText(0, Terminal::get_screen_string(screen_data));
            screen_item->setIcon(0, QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogDetailedView));
        }

        QListWidgetItem* get_selected_level() const
        {
            assert(m_scenario_browser_state == ScenarioBrowserState::LEVELS);
            QList<QListWidgetItem*> selected_items = m_ui.scenario_browser_view->selectedItems();
            if (!selected_items.isEmpty())
            {
                return selected_items.front();
            }
            return nullptr;
        }

        QList<QListWidgetItem*> get_selected_terminals() const
        {
            assert(m_scenario_browser_state == ScenarioBrowserState::TERMINALS);
            return m_ui.scenario_browser_view->selectedItems();
        }

        bool is_screen_item(QTreeWidgetItem* item) const
        {
            if (item)
            {
                assert(item->treeWidget() == m_ui.screen_browser_tree);
                return (m_ui.screen_browser_tree->indexOfTopLevelItem(item) == -1);
            }
            return false;
        }

        void set_current_screen_browser_item(QTreeWidgetItem* item) { m_ui.screen_browser_tree->setCurrentItem(item); }

        QTreeWidgetItem* get_current_screen() const
        {
            QTreeWidgetItem* current_item = m_ui.screen_browser_tree->currentItem();
            if (is_screen_item(current_item))
            {
                return current_item;
            }
            return nullptr;
        }

        void set_current_screen(QTreeWidgetItem* item)
        {
            if (is_screen_item(item))
            {
                set_current_screen_browser_item(item);
            }
        }

        void display_screen(AppCore& core, const Terminal::Screen& screen_data)
        {
            if (screen_data.m_type != Terminal::ScreenType::NONE)
            {
                // Update the terminal preview display
                DisplayData display_data;
                display_data.m_resource_id = screen_data.m_resource_id;
                display_data.m_text = screen_data.m_display_text;
                display_data.m_screen_type = screen_data.m_type;
                display_data.m_alignment = screen_data.m_alignment;

                core.get_display_system().update_display(m_view_id, display_data);
            }

            m_ui.screen_info_table->clearContents();

            m_ui.screen_info_table->setItem(0, 0, new QTableWidgetItem(SCREEN_TYPE_LABELS[Utils::to_integral(screen_data.m_type)]));
            m_ui.screen_info_table->setItem(1, 0, new QTableWidgetItem(QString::number(screen_data.m_resource_id)));
        }

        void clear_clipboard() { m_terminal_clipboard.clear(); }

        int find_terminal_editor(const Terminal& terminal) const
        {
            auto editor_it = std::find_if(m_terminal_editors.begin(), m_terminal_editors.end(),
                [terminal](const TerminalEditorWindow* editor)
                {
                    return (editor->get_terminal_data().get_id() == terminal.get_id());
                }
            );

            if (editor_it != m_terminal_editors.end())
            {
                return std::distance(m_terminal_editors.begin(), editor_it);
            }
            return -1;
        }

        void remove_terminal_editor(const Terminal& terminal)
        {
            const int editor_index = find_terminal_editor(terminal);
            if (editor_index >= 0)
            {
                TerminalEditorWindow* editor_window = m_terminal_editors[editor_index];
                editor_window->clear_modified();
                delete editor_window;

                // "Swap and pop" the deleted terminal editor
                if (editor_index < (m_terminal_editors.size() - 1))
                {
                    std::swap(m_terminal_editors[editor_index], m_terminal_editors.back());
                }
                m_terminal_editors.pop_back();
            }
        }

        void remove_terminal_editor(TerminalEditorWindow* editor_window)
        {
            assert(editor_window);
            const int editor_index = std::distance(m_terminal_editors.begin(), std::find(m_terminal_editors.begin(), m_terminal_editors.end(), editor_window));
            editor_window->clear_modified();
            delete editor_window;

            // "Swap and pop" the deleted terminal editor
            if (editor_index < (m_terminal_editors.size() - 1))
            {
                std::swap(m_terminal_editors[editor_index], m_terminal_editors.back());
            }
            m_terminal_editors.pop_back();
        }

        void reset_scenario_ui()
        {
            clear_scenario_browser();
            m_ui.scenario_name_label->setText(m_scenario.get_name());

            // Reset the font
            QFont default_font = m_ui.scenario_name_label->font();
            default_font.setBold(false);
            m_ui.scenario_name_label->setFont(default_font);

            display_scenario_levels();

            clear_clipboard();
        }

        void reset_terminal_ui()
        {
            // Terminal info
            m_ui.terminal_info_table->clearContents();
            m_ui.screen_info_table->clearContents();
            m_ui.terminal_name_label->setText("N/A");

            // Terminal preview
            m_ui.terminal_first_button->setEnabled(false);
            m_ui.terminal_last_button->setEnabled(false);
            m_ui.terminal_next_button->setEnabled(false);
            m_ui.terminal_prev_button->setEnabled(false);

            // Browsers
            reset_screen_browser();
            update_scenario_buttons();
        }

        void clear_terminal_editors()
        {
            for (TerminalEditorWindow* current_terminal_editor : m_terminal_editors)
            {
                current_terminal_editor->deleteLater();
            }
            m_terminal_editors.clear();
        }

        void reset_screen_browser()
        {
            // Clear the tree and re-add the screen group items
            m_ui.screen_browser_tree->clear();
            {
                QTreeWidgetItem* unfinished_screens_root = new QTreeWidgetItem(m_ui.screen_browser_tree);
                init_screen_group_item(unfinished_screens_root, true);
            }
            {
                QTreeWidgetItem* finished_screens_root = new QTreeWidgetItem(m_ui.screen_browser_tree);
                init_screen_group_item(finished_screens_root, false);
            }
        }

        void update_scenario_buttons()
        {
            if (m_scenario_browser_state == ScenarioBrowserState::TERMINALS)
            {
                m_ui.add_terminal_button->setEnabled(true);
                m_ui.remove_terminal_button->setEnabled(!get_selected_terminals().isEmpty());
            }
            else
            {
                m_ui.add_terminal_button->setEnabled(false);
                m_ui.remove_terminal_button->setEnabled(false);
            }
        }

        void update_screen_navigation_buttons(int current_index, int screen_count)
        {
            const bool after_start = (current_index > 0);
            const bool before_end = (current_index < (screen_count - 1));

            m_ui.terminal_first_button->setEnabled(after_start);
            m_ui.terminal_prev_button->setEnabled(after_start);
            m_ui.terminal_next_button->setEnabled(before_end);
            m_ui.terminal_last_button->setEnabled(before_end);
        }

        void disable_screen_navigation_buttons()
        {
            m_ui.terminal_first_button->setEnabled(false);
            m_ui.terminal_prev_button->setEnabled(false);
            m_ui.terminal_next_button->setEnabled(false);
            m_ui.terminal_last_button->setEnabled(false);
        }

        void terminal_node_selected(QListWidgetItem* terminal_item, int terminal_index)
        {
            m_ui.terminal_info_table->clearContents();
            reset_screen_browser();

            const Level& selected_level = m_scenario.get_level(m_current_level_index);
            const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);

            m_ui.terminal_name_label->setText(terminal_item->text());

            // Set the terminal attributes
            m_ui.terminal_info_table->setItem(0, 0, new QTableWidgetItem(QString::number(terminal_index)));
            {
                const Terminal::Teleport& unfinished_teleport = selected_terminal.get_teleport_info(true);
                if (unfinished_teleport.m_type != Terminal::TeleportType::NONE)
                {
                    QTableWidgetItem* unfinished_teleport_item = new QTableWidgetItem(QStringLiteral("%1 (%2)").arg(QString::number(unfinished_teleport.m_index), TELEPORT_TYPE_LABELS[Utils::to_integral(unfinished_teleport.m_type)]));
                    m_ui.terminal_info_table->setItem(1, 0, unfinished_teleport_item);
                }
            }

            {
                const Terminal::Teleport& finished_teleport = selected_terminal.get_teleport_info(false);
                if (finished_teleport.m_type != Terminal::TeleportType::NONE)
                {
                    QTableWidgetItem* finished_teleport_item = new QTableWidgetItem(QStringLiteral("%1 (%2)").arg(QString::number(finished_teleport.m_index), TELEPORT_TYPE_LABELS[Utils::to_integral(finished_teleport.m_type)]));
                    m_ui.terminal_info_table->setItem(2, 0, finished_teleport_item);
                }
            }

            // Fill the screen browser
            {
                const auto& unfinished_screens = selected_terminal.get_screens(true);
                QTreeWidgetItem* unfinished_group_item = m_ui.screen_browser_tree->topLevelItem(0);
                for (const Terminal::Screen& current_screen : unfinished_screens)
                {
                    QTreeWidgetItem* screen_item = new QTreeWidgetItem(unfinished_group_item);
                    init_screen_item(current_screen, screen_item);
                }
                unfinished_group_item->setExpanded(true);
            }
            {
                const auto& finished_screens = selected_terminal.get_screens(false);
                QTreeWidgetItem* finished_group_item = m_ui.screen_browser_tree->topLevelItem(1);
                for (const Terminal::Screen& current_screen : finished_screens)
                {
                    QTreeWidgetItem* screen_item = new QTreeWidgetItem(finished_group_item);
                    init_screen_item(current_screen, screen_item);
                }
                finished_group_item->setExpanded(true);
            }

            update_scenario_buttons();
        }

        void terminal_node_double_clicked(AppCore& core, QListWidgetItem* item)
        {
            // First check if we already have an editor open for this item
            TerminalEditorWindow* editor_window = nullptr;
            const int terminal_index = get_terminal_index(item);
            const Level& selected_level = m_scenario.get_level(m_current_level_index);
            const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);

            const int editor_window_index = find_terminal_editor(selected_terminal);
            if (editor_window_index == -1)
            {
                editor_window = new TerminalEditorWindow(core, m_current_level_index, selected_terminal, generate_terminal_editor_title(selected_level, selected_terminal, terminal_index));

                // Connect signals between the editor and the main windows
                HuxQt* main_window = core.get_main_window();
                QObject::connect(editor_window, &TerminalEditorWindow::editor_closed, main_window, &HuxQt::terminal_editor_closed);
                m_terminal_editors.push_back(editor_window);
            }
            else
            {
                editor_window = m_terminal_editors[editor_window_index];
            }

            // Bring the editor window to the front
            editor_window->show();
            editor_window->activateWindow();
            editor_window->raise();
        }

        bool save_terminal_editors()
        {
            for (TerminalEditorWindow* editor_window : m_terminal_editors)
            {
                if (editor_window->is_modified())
                {
                    const QMessageBox::StandardButton user_response = QMessageBox::question(editor_window, "Terminal Modified", "You have modified this terminal. Save changes?",
                        QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel));

                    switch (user_response)
                    {
                    case QMessageBox::Yes:
                        if (!save_terminal_changes(editor_window))
                        {
                            return false;
                        }
                        break;
                    case QMessageBox::YesToAll:
                    {
                        // Save all changes
                        for (TerminalEditorWindow* current_window : m_terminal_editors)
                        {
                            if (editor_window->is_modified())
                            {
                                if (!save_terminal_changes(editor_window))
                                {
                                    return false;
                                }
                            }
                        }
                        return true;
                    }
                    case QMessageBox::No:
                    {
                        editor_window->clear_modified(); // Ignore the changes
                        continue;
                    }
                    case QMessageBox::NoToAll:
                    {
                        // Ignore all changes
                        for (TerminalEditorWindow* current_window : m_terminal_editors)
                        {
                            editor_window->clear_modified();
                        }
                        return true;
                    }
                    case QMessageBox::Cancel:
                        return false;
                    }
                }
            }
            return true;
        }

        bool save_terminal_changes(TerminalEditorWindow* editor_window)
        {
            if (editor_window->validate_terminal_info())
            {
                // Make sure all the latest changes got saved
                editor_window->save_changes();

                Level& selected_level = m_scenario.get_level(editor_window->get_level_index());
                const Terminal& edited_terminal_data = editor_window->get_terminal_data();

                const int edited_terminal_index = selected_level.find_terminal(edited_terminal_data.get_id());
                assert(edited_terminal_index >= 0);

                Terminal& selected_terminal = selected_level.get_terminal(edited_terminal_index);
                selected_terminal = edited_terminal_data;
                selected_terminal.set_modified(true);
                selected_level.set_modified();

                // Reset browser just to be safe
                reset_scenario_browser_view();

                m_scenario.set_modified();
                return true;
            }
            return false;
        }

        void remove_level_item(QListWidgetItem* level_item)
        {
            const int level_index = m_ui.scenario_browser_view->row(level_item);
            const Level& removed_level = m_scenario.get_level(level_index);
            for (const Terminal& current_terminal : removed_level.get_terminals())
            {
                remove_terminal_editor(current_terminal);
            }

            delete level_item;
            scenario_edited();
        }

        void scenario_edited()
        {
            if (!m_scenario.is_modified())
            {
                // Modify the label so the user can see that the scenario has changed
                m_ui.scenario_name_label->setText(m_ui.scenario_name_label->text() + " - Modified");

                QFont bold_font = m_ui.scenario_name_label->font();
                bold_font.setBold(true);
                m_ui.scenario_name_label->setFont(bold_font);

                // Set the flag
                m_scenario.set_modified();
            }
        }

        void clear_scenario_edited()
        {
            m_scenario.clear_modified();

            // Reset the terminal/screen displays
            reset_scenario_browser_view();

            // Reset the scenario label
            m_ui.scenario_name_label->setText(m_scenario.get_name());
            QFont default_font = m_ui.scenario_name_label->font();
            default_font.setBold(false);
            m_ui.scenario_name_label->setFont(default_font);
        }
    };

    HuxQt::HuxQt(QWidget* parent)
        : QMainWindow(parent)
        , m_internal(std::make_unique<Internal>())
    {
        m_internal->m_ui.setupUi(this);

        init_ui();
        connect_signals();

        // Create the core object after the UI is fully initialized
        m_core = std::make_unique<AppCore>(this);

        // Register the graphics view in the display system
        m_internal->m_view_id = m_core->get_display_system().register_graphics_view(m_internal->m_ui.terminal_preview);
    }

    HuxQt::~HuxQt()
    {
        if (m_internal->m_preview_config)
        {
            delete m_internal->m_preview_config;
        }
    }

    QGraphicsView* HuxQt::get_graphics_view() { return m_internal->m_ui.terminal_preview; }

    bool HuxQt::add_level(const QString& level_name, const QString& level_dir_name)
    {
        if (ScenarioManager::add_scenario_level(m_internal->m_scenario, level_dir_name))
        {
            if (m_internal->m_scenario_browser_state == ScenarioBrowserState::LEVELS)
            {
                QListWidgetItem* level_item = new QListWidgetItem(m_internal->m_ui.scenario_browser_view);
                m_internal->init_level_item(m_internal->m_scenario.get_levels().back(), level_item);
            }
            return true;
        }
        else
        {
            QMessageBox::warning(this, "Scenario Editor Error", QStringLiteral("Error loading level \"%1\"!").arg(level_name));
            return false;
        }
    }

    void HuxQt::closeEvent(QCloseEvent* event)
    {
        if (!close_current_scenario())
        {
            event->ignore();
            return;
        }

        m_internal->clear_terminal_editors();
        event->accept();
    }

    void HuxQt::init_ui()
    {
        m_internal->m_ui.action_save_scenario->setEnabled(false);
        m_internal->m_ui.action_export_scenario_scripts->setEnabled(false);
        m_internal->m_ui.scenario_up_button->setEnabled(false);
        
        m_internal->m_ui.terminal_info_table->setRowCount(3);
        m_internal->m_ui.terminal_info_table->setColumnCount(1);

        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("ID"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Unfinished teleport"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(2, new QTableWidgetItem("Finished teleport"));

        m_internal->m_ui.screen_info_table->setRowCount(2);
        m_internal->m_ui.screen_info_table->setColumnCount(1);

        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("Type"));
        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Resource ID"));

        // Set the button icons
        m_internal->m_ui.scenario_up_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogToParent));

        m_internal->m_ui.add_terminal_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileIcon));
        m_internal->m_ui.remove_terminal_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_TrashIcon));

        m_internal->m_ui.terminal_first_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSkipBackward));
        m_internal->m_ui.terminal_prev_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSeekBackward));
        m_internal->m_ui.terminal_next_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSeekForward));
        m_internal->m_ui.terminal_last_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSkipForward));

        m_internal->update_scenario_buttons();

        // Screen browser
        m_internal->reset_screen_browser();
    }

    void HuxQt::connect_signals()
    {
        // Menu
        connect(m_internal->m_ui.action_open_scenario, &QAction::triggered, this, &HuxQt::open_scenario);
        connect(m_internal->m_ui.action_save_scenario, &QAction::triggered, this, &HuxQt::save_scenario_action);
        connect(m_internal->m_ui.action_export_scenario_scripts, &QAction::triggered, this, &HuxQt::export_scenario_scripts);
        connect(m_internal->m_ui.action_terminal_preview_config, &QAction::triggered, this, &HuxQt::open_preview_config);

        // Scenario browser
        connect(m_internal->m_ui.scenario_up_button, &QToolButton::clicked, this, &HuxQt::scenario_up_clicked);
        connect(m_internal->m_ui.scenario_browser_view, &Utils::ScenarioBrowserWidget::items_dropped, this, &HuxQt::terminal_items_moved, Qt::ConnectionType::QueuedConnection);

        m_internal->m_ui.scenario_browser_view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_internal->m_ui.scenario_browser_view, &QListWidget::itemClicked, this, &HuxQt::scenario_item_clicked);
        connect(m_internal->m_ui.scenario_browser_view, &QListWidget::itemDoubleClicked, this, &HuxQt::scenario_item_double_clicked);
        connect(m_internal->m_ui.scenario_browser_view, &QListWidget::customContextMenuRequested, this, &HuxQt::scenario_view_context_menu);

        // Screen browser
        connect(m_internal->m_ui.screen_browser_tree, &QTreeWidget::currentItemChanged, this, &HuxQt::screen_item_selected);

        // Browser buttons
        connect(m_internal->m_ui.add_terminal_button, &QPushButton::clicked, this, &HuxQt::add_terminal_clicked);
        connect(m_internal->m_ui.remove_terminal_button, &QPushButton::clicked, this, &HuxQt::remove_terminal_clicked);

        // Preview buttons
        connect(m_internal->m_ui.terminal_first_button, &QPushButton::clicked, this, &HuxQt::terminal_first_clicked);
        connect(m_internal->m_ui.terminal_prev_button, &QPushButton::clicked, this, &HuxQt::terminal_prev_clicked);
        connect(m_internal->m_ui.terminal_next_button, &QPushButton::clicked, this, &HuxQt::terminal_next_clicked);
        connect(m_internal->m_ui.terminal_last_button, &QPushButton::clicked, this, &HuxQt::terminal_last_clicked);
    }

    void HuxQt::clear_preview_display()
    {
        m_core->get_display_system().clear_display(m_internal->m_view_id);
    }

    void HuxQt::reset_ui()
    {
        m_internal->reset_terminal_ui();
        m_internal->reset_scenario_ui();
        clear_preview_display();
    }

    void HuxQt::open_scenario()
    {
        QString scenario_dir = QFileDialog::getExistingDirectory(this, tr("Open Scenario Split Folder"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!scenario_dir.isEmpty())
        {
            if (!close_current_scenario())
            {
                // Something went wrong, cancel this request
                return;
            }

            m_internal->clear_terminal_editors();

            if(m_core->get_scenario_manager().load_scenario(scenario_dir, m_internal->m_scenario))
            {
                m_internal->m_ui.action_save_scenario->setEnabled(true);
                m_internal->m_ui.action_export_scenario_scripts->setEnabled(true);
                reset_ui();
                
                // Update the display system
                m_core->get_display_system().update_resources(scenario_dir + "/Resources");
            }
        }
    }

    void HuxQt::save_scenario_action()
    {
        // Prepare the export dialog (allows one last check to make sure the scripts we will output are correct, also helps with debugging)
        QStringList level_output_list;
        const ScenarioManager& scenario_manager = m_core->get_scenario_manager();
        for (const Level& current_level : m_internal->m_scenario.get_levels())
        {
            level_output_list << current_level.get_name();
            level_output_list << scenario_manager.print_level_script(current_level);
        }

        ExportScenarioDialog* export_dialog = new ExportScenarioDialog(this, level_output_list);
        connect(export_dialog, &QDialog::accepted, this, &HuxQt::save_scenario);
        export_dialog->open();
    }

    void HuxQt::export_scenario_scripts()
    {
        QString export_dir_path = QFileDialog::getExistingDirectory(this, tr("Select Export Folder"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if(!export_dir_path.isEmpty())
        {
            if (QDir(export_dir_path).isEmpty())
            {
                m_core->get_scenario_manager().save_scenario(export_dir_path, m_internal->m_scenario, false);
            }
            else
            {
                QMessageBox::warning(this, "Scenario Export Error", "Export folder must be empty!");
            }
        }
    }

    void HuxQt::open_preview_config()
    {
        if (!m_internal->m_preview_config)
        {
            m_internal->m_preview_config = new PreviewConfigWindow(*m_core);
            connect(m_internal->m_preview_config, &PreviewConfigWindow::window_closed, this, &HuxQt::preview_config_closed);
            connect(m_internal->m_preview_config, &PreviewConfigWindow::config_update, this, &HuxQt::display_current_screen);
        }

        m_internal->m_preview_config->show();
        m_internal->m_preview_config->activateWindow();
        m_internal->m_preview_config->raise();
    }

    void HuxQt::preview_config_closed()
    {
        m_internal->m_preview_config = nullptr;
    }

    void HuxQt::scenario_item_clicked(QListWidgetItem* item)
    {
        m_internal->update_scenario_buttons();
        if (m_internal->m_scenario_browser_state == ScenarioBrowserState::TERMINALS)
        {
            if (item->isSelected())
            {
                const int terminal_index = m_internal->get_terminal_index(item);
                // Update the terminal UI
                m_internal->terminal_node_selected(item, terminal_index);

                // Jump to the first screen child (if possible)
                QTreeWidgetItemIterator screen_iterator(m_internal->m_ui.screen_browser_tree);
                while (QTreeWidgetItem* current_screen = *screen_iterator)
                {
                    if (m_internal->is_screen_item(current_screen))
                    {
                        m_internal->set_current_screen(current_screen);
                        return;
                    }
                    ++screen_iterator;
                }
            }
        }
        // TODO: anything else?
    }

    void HuxQt::scenario_item_double_clicked(QListWidgetItem* item)
    {
        if (m_internal->m_scenario_browser_state == ScenarioBrowserState::LEVELS)
        {
            m_internal->open_level(item);
        }
        else
        {
            m_internal->terminal_node_double_clicked(*m_core, item);
        }
    }

    void HuxQt::scenario_view_context_menu(const QPoint& point)
    {
        // Check if we clicked on an item
        QListWidgetItem* selected_item = m_internal->m_ui.scenario_browser_view->itemAt(point);
        if (m_internal->m_scenario_browser_state == ScenarioBrowserState::TERMINALS)
        {
            QMenu context_menu;
            if (selected_item)
            {
                // TODO: shortcut!
                context_menu.addAction("Copy", this, &HuxQt::copy_terminal_action);
            }
            // Check if we can paste
            if (!m_internal->m_terminal_clipboard.empty())
            {
                // TODO: shortcut!
                context_menu.addAction("Paste", this, &HuxQt::paste_terminal_action);
            }
            const QPoint global_pos = m_internal->m_ui.scenario_browser_view->mapToGlobal(point);
            context_menu.exec(global_pos);
        }
        else
        {
            if (selected_item)
            {
                // TODO: copy level contents?
                // TODO2: remove level
            }
            else
            {
                QMenu context_menu;
                context_menu.addAction("Add Level", this, &HuxQt::add_level_action);
                const QPoint global_pos = m_internal->m_ui.scenario_browser_view->mapToGlobal(point);
                context_menu.exec(global_pos);
            }
        }
    }

    void HuxQt::screen_item_selected(QTreeWidgetItem* current, QTreeWidgetItem* previous)
    {
        // Additional UI updates
        if (m_internal->is_screen_item(current))
        {
            QTreeWidgetItem* screen_group_item = current->parent();
            const int screen_index = screen_group_item->indexOfChild(current);
            m_internal->update_screen_navigation_buttons(screen_index, screen_group_item->childCount());
            display_current_screen();
        }
        else
        {
            m_internal->disable_screen_navigation_buttons();
        }
    }

    void HuxQt::add_level_action()
    {
        QStringList available_levels = ScenarioManager::gather_additional_levels(m_internal->m_scenario);
        if (!available_levels.isEmpty())
        {
            AddLevelDialog* add_level_dialog = new AddLevelDialog(this, available_levels);
            add_level_dialog->open();
        }
        else
        {
            QMessageBox::warning(this, "Scenario Editor Error", "The current scenario contains no additional levels!");
        }
    }

    void HuxQt::remove_level_action()
    {
        QListWidgetItem* level_item = m_internal->get_selected_level();
        assert(m_internal->m_scenario_browser_state == ScenarioBrowserState::LEVELS);

        // NOTE: deleting a level from the scenario view only simply means we won't export any updates for it. The user can opt to delete the file as well
        if (QMessageBox::question(this, "Remove Level", QStringLiteral("Are you sure you want to remove the level \"%1\"?").arg(level_item->text())) == QMessageBox::StandardButton::Yes)
        {
            const int level_index = m_internal->m_ui.scenario_browser_view->row(level_item);
            if (QMessageBox::question(this, "Remove Level", "Delete level script file?", QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No) == QMessageBox::StandardButton::Yes)
            {
                if (!ScenarioManager::delete_scenario_level_script(m_internal->m_scenario, level_index))
                {
                    QMessageBox::warning(this, "Scenario Manager Error", QStringLiteral("Unable to remove level \"%1\"!").arg(level_item->text()));
                }
            }
            ScenarioManager::remove_scenario_level(m_internal->m_scenario, level_index);

            // Remove the level from the tree
            m_internal->remove_level_item(level_item);
        }
    }

    void HuxQt::terminal_items_moved()
    {
        if (m_internal->m_scenario_browser_state == ScenarioBrowserState::TERMINALS)
        {
            m_internal->reorder_terminal_items();
        }
    }

    void HuxQt::copy_terminal_action()
    {
        m_internal->clear_clipboard();

        // Store copies of the selected terminals in the clipboard
        QList<QListWidgetItem*> selected_terminals = m_internal->get_selected_terminals();
        Level& selected_level = m_internal->m_scenario.get_level(m_internal->m_current_level_index);
        for (QListWidgetItem* current_terminal_item : selected_terminals)
        {
            const int terminal_index = m_internal->get_terminal_index(current_terminal_item);
            m_internal->m_terminal_clipboard.push_back(selected_level.get_terminal(terminal_index));
        }
    }

    void HuxQt::paste_terminal_action()
    {
        // Paste at selection (or at the end if nothing is selected)
        QList<QListWidgetItem*> selected_terminals = m_internal->get_selected_terminals();
        const int selected_index = selected_terminals.isEmpty() ? m_internal->m_ui.scenario_browser_view->count() : m_internal->get_terminal_index(selected_terminals.front());

        Level& selected_level = m_internal->m_scenario.get_level(m_internal->m_current_level_index);
        ScenarioManager::add_level_terminals(m_internal->m_scenario, selected_level, m_internal->m_terminal_clipboard, selected_index);

        // Reset view to include the new items
        m_internal->reset_scenario_browser_view();

        // Select the newly inserted terminals
        for (int pasted_index = selected_index; pasted_index < (selected_index + m_internal->m_terminal_clipboard.size()); ++pasted_index)
        {
            m_internal->m_ui.scenario_browser_view->item(pasted_index)->setSelected(true);
        }

        m_internal->scenario_edited();
    }

    void HuxQt::scenario_up_clicked()
    {
        m_internal->display_scenario_levels();
    }

    void HuxQt::add_terminal_clicked()
    {
        Level& selected_level = m_internal->m_scenario.get_level(m_internal->m_current_level_index);
        ScenarioManager::add_level_terminal(m_internal->m_scenario, selected_level);

        // Add the new terminal to the tree
        const Terminal& new_terminal = selected_level.get_terminals().back();
        QListWidgetItem* new_terminal_item = new QListWidgetItem(m_internal->m_ui.scenario_browser_view);

        // Reset view to include the new item
        m_internal->reset_scenario_browser_view();
        m_internal->m_ui.scenario_browser_view->setCurrentRow(m_internal->m_ui.scenario_browser_view->count() - 1);

        m_internal->scenario_edited();
    }

    void HuxQt::remove_terminal_clicked()
    {
        QList<QListWidgetItem*> selected_terminals = m_internal->get_selected_terminals();
        if (!selected_terminals.isEmpty())
        {
            Level& selected_level = m_internal->m_scenario.get_level(m_internal->m_current_level_index);
            std::vector<size_t> selected_terminal_indices;

            for (QListWidgetItem* current_terminal_item : selected_terminals)
            {
                const int terminal_index = m_internal->get_terminal_index(current_terminal_item);
                selected_terminal_indices.push_back(terminal_index);

                // Close any editors associated with this terminal
                const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);
                m_internal->remove_terminal_editor(selected_terminal);
            }

            // Remove the terminals
            ScenarioManager::remove_level_terminals(selected_level, selected_terminal_indices);

            // Reset view to show changes
            m_internal->reset_scenario_browser_view();
            clear_preview_display();

            m_internal->scenario_edited();
        }
    }

    void HuxQt::display_current_screen()
    {
        if (m_internal->m_scenario_browser_state == ScenarioBrowserState::TERMINALS)
        {
            QListWidgetItem* current_terminal = m_internal->m_ui.scenario_browser_view->currentItem();
            if (!current_terminal || !current_terminal->isSelected())
            {
                // TODO: clear the display?
                return;
            }

            QTreeWidgetItem* current_screen = m_internal->get_current_screen();
            if (!current_screen)
            {
                // Check if we selected a group item
                QTreeWidgetItem* group_item = m_internal->m_ui.screen_browser_tree->currentItem();
                if (!group_item || (group_item->childCount() == 0))
                {
                    // No screen was selected
                    // TODO: clear the display?
                    return;
                }
                // Use the first screen from the screen group (if applicable)
                current_screen = group_item->child(0);
            }

            const bool unfinished = (m_internal->m_ui.screen_browser_tree->indexOfTopLevelItem(current_screen->parent()) == 0);
            const int screen_index = current_screen->parent()->indexOfChild(current_screen);

            const Level& selected_level = m_internal->m_scenario.get_level(m_internal->m_current_level_index);
            const Terminal& selected_terminal = selected_level.get_terminal(m_internal->get_terminal_index(current_terminal));
            const Terminal::Screen& selected_screen = selected_terminal.get_screen(screen_index, unfinished);

            m_internal->display_screen(*m_core, selected_screen);
        }
    }

    void HuxQt::terminal_first_clicked()
    {       
        if (QTreeWidgetItem * current_screen = m_internal->get_current_screen())
        {
            // Simply jump to the first child
            m_internal->set_current_screen(current_screen->parent()->child(0));
        }
    }

    void HuxQt::terminal_prev_clicked()
    {
        if (QTreeWidgetItem* current_screen = m_internal->get_current_screen())
        {
            QTreeWidgetItem* parent_item = current_screen->parent();
            const int screen_index = parent_item->indexOfChild(current_screen);
            m_internal->set_current_screen(parent_item->child(screen_index - 1));
        }
    }

    void HuxQt::terminal_next_clicked()
    {
        if (QTreeWidgetItem* current_screen = m_internal->get_current_screen())
        {
            QTreeWidgetItem* parent_item = current_screen->parent();
            const int screen_index = parent_item->indexOfChild(current_screen);
            m_internal->set_current_screen(parent_item->child(screen_index + 1));
        }
    }

    void HuxQt::terminal_last_clicked()
    {
        if (QTreeWidgetItem* current_screen = m_internal->get_current_screen())
        {
            // Jump to the last child
            QTreeWidgetItem* parent_item = current_screen->parent();
            m_internal->m_ui.screen_browser_tree->setCurrentItem(parent_item->child(parent_item->childCount() - 1));
        }
    }

    void HuxQt::terminal_editor_closed()
    {
        TerminalEditorWindow* editor_window = qobject_cast<TerminalEditorWindow*>(sender());
        assert(editor_window);

        if (editor_window->is_modified())
        {
            if (editor_window->validate_terminal_info())
            {
                // Overwrite the terminal data
                Level& selected_level = m_internal->m_scenario.get_level(editor_window->get_level_index());
                const Terminal& edited_terminal_data = editor_window->get_terminal_data();
                for (Terminal& current_terminal : selected_level.get_terminals())
                {
                    if (current_terminal.get_id() == edited_terminal_data.get_id())
                    {
                        current_terminal = edited_terminal_data;
                        current_terminal.set_modified(true);
                        break;
                    }
                }

                selected_level.set_modified();
                m_internal->reset_scenario_browser_view();
                m_internal->scenario_edited();
            }
        }

        m_internal->remove_terminal_editor(editor_window);
    }

    bool HuxQt::close_current_scenario()
    {
        if (!m_internal->save_terminal_editors())
        {
            return false;
        }

        if (m_internal->m_scenario.is_modified())
        {
            const QMessageBox::StandardButton user_response = QMessageBox::question(this, "Unsaved Changes", "You have modified the scenario. Save changes?",
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel));

            switch (user_response)
            {
            case QMessageBox::Yes:
                return save_scenario();
            case QMessageBox::No:
                return true;
            case QMessageBox::Cancel:
                return false;
            }
        }

        return true;
    }

    bool HuxQt::save_scenario()
    {
        ScenarioManager& scenario_manager = m_core->get_scenario_manager();
        if (!scenario_manager.save_scenario(m_internal->m_scenario.get_merge_folder_path(), m_internal->m_scenario))
        {
            return false;
        }

        // Clear all the UI modifications
        m_internal->clear_scenario_edited();
        return true;
    }
}