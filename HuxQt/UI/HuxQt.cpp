#include <HuxQt/UI/HuxQt.h>
#include "ui_HuxQt.h"

#include <HuxQt/AppCore.h>

#include <HuxQt/Scenario/ScenarioManager.h>
#include <HuxQt/Scenario/Scenario.h>
#include <HuxQt/Scenario/ScenarioBrowserModel.h>

#include <HuxQt/UI/DisplaySystem.h>
#include <HuxQt/UI/DisplayData.h>

#include <HuxQt/UI/EditLevelDialog.h>
#include <HuxQt/UI/ExportScenarioDialog.h>
#include <HuxQt/UI/TerminalEditorWindow.h>
#include <HuxQt/UI/PreviewConfigWindow.h>
#include <HuxQt/UI/EditTextColorDialog.h>

#include <HuxQt/Utils/Utilities.h>

#include <unordered_set>

#include <QMessageBox>
#include <QFileDialog>

namespace HuxApp
{
    namespace
    {
        constexpr int APP_VERSION_MAJOR = 0;
        constexpr int APP_VERSION_MINOR = 9;
        constexpr int APP_VERSION_PATCH = 5;

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

        QString get_app_version_string() { return QStringLiteral("%1.%2.%3").arg(APP_VERSION_MAJOR).arg(APP_VERSION_MINOR).arg(APP_VERSION_PATCH); }
    }

    struct HuxQt::Internal
    {
        Ui::HuxQtMainWindow m_ui;

        ScenarioBrowserModel m_scenario_browser_model;
        bool m_scenario_modified = false;

        TerminalID m_selected_terminal;

        EditLevelDialog* m_edit_level_dialog = nullptr;

        std::unordered_set<TerminalEditorWindow*> m_terminal_editors;

        PreviewConfigWindow* m_preview_config = nullptr;
        QPalette m_dark_theme;

        DisplaySystem::ViewID m_view_id;

        Internal()
        {
            prepare_dark_theme();
        }

        void prepare_dark_theme()
        {
            const QColor dark_gray(53, 53, 53);
            const QColor gray(128, 128, 128);
            const QColor black(25, 25, 25);
            const QColor blue(42, 130, 218);

            m_dark_theme.setColor(QPalette::Window, QColor(53, 53, 53));
            m_dark_theme.setColor(QPalette::WindowText, Qt::white);
            m_dark_theme.setColor(QPalette::Base, QColor(25, 25, 25));
            m_dark_theme.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
            m_dark_theme.setColor(QPalette::ToolTipBase, Qt::black);
            m_dark_theme.setColor(QPalette::ToolTipText, Qt::white);
            m_dark_theme.setColor(QPalette::Text, Qt::white);
            m_dark_theme.setColor(QPalette::Button, QColor(53, 53, 53));
            m_dark_theme.setColor(QPalette::ButtonText, Qt::white);
            m_dark_theme.setColor(QPalette::BrightText, Qt::red);
            m_dark_theme.setColor(QPalette::Link, QColor(42, 130, 218));
            m_dark_theme.setColor(QPalette::Highlight, QColor(42, 130, 218));
            m_dark_theme.setColor(QPalette::HighlightedText, Qt::black);

            m_dark_theme.setColor(QPalette::Active, QPalette::Button, gray.darker());
            m_dark_theme.setColor(QPalette::Disabled, QPalette::ButtonText, gray);
            m_dark_theme.setColor(QPalette::Disabled, QPalette::WindowText, gray);
            m_dark_theme.setColor(QPalette::Disabled, QPalette::Text, gray);
            m_dark_theme.setColor(QPalette::Disabled, QPalette::Light, dark_gray);
        }

        void set_app_theme()
        {
            const bool use_dark_theme = m_ui.action_use_dark_theme->isChecked();
            if (use_dark_theme)
            {
                qApp->setPalette(m_dark_theme);
            }
            else
            {
                qApp->setPalette(QPalette());
            }
        }

        TerminalEditorWindow* find_terminal_editor(const TerminalID& terminal_id) const
        {
            auto editor_it = std::find_if(m_terminal_editors.begin(), m_terminal_editors.end(),
                [terminal_id](const TerminalEditorWindow* editor)
                {
                    return (editor->get_terminal_id() == terminal_id);
                }
            );

            if (editor_it != m_terminal_editors.end())
            {
                return *editor_it;
            }
            return nullptr;
        }

        void clear_terminal_editors()
        {
            for (TerminalEditorWindow* current_terminal_editor : m_terminal_editors)
            {
                current_terminal_editor->deleteLater();
            }
            m_terminal_editors.clear();
        }

        bool save_terminal_editors()
        {
            // Go over all our editors and prompt to save changes
            for (TerminalEditorWindow* editor_window : m_terminal_editors)
            {
                const QMessageBox::StandardButton user_response = editor_window->prompt_save();

                switch (user_response)
                {
                case QMessageBox::YesToAll:
                {
                    // Save all changes
                    for (TerminalEditorWindow* current_window : m_terminal_editors)
                    {
                        current_window->force_save();
                    }
                    return true;
                }
                case QMessageBox::NoToAll:
                    return true;
                case QMessageBox::Cancel:
                    return false;
                }
            }
            return true;
        }

        void init_screen_group_item(QTreeWidgetItem* screen_group_item, Terminal::BranchType branch_type)
        {
            screen_group_item->setText(0, Terminal::get_branch_type_name(branch_type));
            screen_group_item->setIcon(0, QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_DirIcon));
        }

        void init_screen_item(const Terminal::Screen& screen_data, QTreeWidgetItem* screen_item)
        {
            screen_item->setText(0, Terminal::get_screen_string(screen_data));
            screen_item->setIcon(0, QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogDetailedView));
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
        }

        void reset_screen_browser()
        {
            // Clear the tree and re-add the screen group items
            m_ui.screen_browser_tree->clear();
            
            for (int current_branch_index = 0; current_branch_index < Utils::to_integral(Terminal::BranchType::TYPE_COUNT); ++current_branch_index)
            {
                const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
                QTreeWidgetItem* current_branch_screens_root = new QTreeWidgetItem(m_ui.screen_browser_tree);

                init_screen_group_item(current_branch_screens_root, current_branch_type);
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

        void terminal_selected(int terminal_id, const Terminal& selected_terminal)
        {
            m_ui.terminal_info_table->clearContents();
            reset_screen_browser();

            m_ui.terminal_name_label->setText(selected_terminal.get_name());

            // Set the terminal attributes
            m_ui.terminal_info_table->setItem(0, 0, new QTableWidgetItem(QString::number(terminal_id)));

            int current_branch_index = 0;
            for (const Terminal::Branch& current_branch : selected_terminal.get_branches())
            {
                const Terminal::Teleport& current_teleport = current_branch.m_teleport;
                if (current_teleport.m_type != Terminal::TeleportType::NONE)
                {
                    QTableWidgetItem* teleport_item = new QTableWidgetItem(QStringLiteral("%1 (%2)").arg(QString::number(current_teleport.m_index), TELEPORT_TYPE_LABELS[Utils::to_integral(current_teleport.m_type)]));
                    m_ui.terminal_info_table->setItem(current_branch_index + 1, 0, teleport_item);

                }

                // Fill the screen browser
                QTreeWidgetItem* screen_group_item = m_ui.screen_browser_tree->topLevelItem(current_branch_index);
                for (const Terminal::Screen& current_screen : current_branch.m_screens)
                {
                    QTreeWidgetItem* screen_item = new QTreeWidgetItem(screen_group_item);
                    init_screen_item(current_screen, screen_item);
                }

                if (!current_branch.m_screens.empty())
                {
                    screen_group_item->setExpanded(true);
                }

                ++current_branch_index;
            }
        }
    };

    HuxQt::HuxQt(QWidget* parent)
        : QMainWindow(parent)
        , m_internal(std::make_unique<Internal>())
    {
        m_internal->m_ui.setupUi(this);

        update_title();

        init_ui();
        connect_signals();

        // Create the core object after the UI is fully initialized
        m_core = std::make_unique<AppCore>(this);

        // Register the graphics view in the display system
        m_internal->m_view_id = m_core->get_display_system().register_graphics_view(m_internal->m_ui.terminal_preview);

        // TODO: "load" an empty scenario as our starting point
    }

    HuxQt::~HuxQt()
    {
        if (m_internal->m_preview_config)
        {
            delete m_internal->m_preview_config;
        }
    }

    QGraphicsView* HuxQt::get_graphics_view() { return m_internal->m_ui.terminal_preview; }

    void HuxQt::closeEvent(QCloseEvent* event)
    {
        if (!close_current_scenario())
        {
            event->ignore();
            return;
        }

        // Clear the editors
        m_internal->clear_terminal_editors();
        event->accept();
    }

    void HuxQt::init_ui()
    {
        m_internal->m_ui.action_save_scenario->setEnabled(false);
        m_internal->m_ui.action_save_scenario_as->setEnabled(false);
        m_internal->m_ui.action_export_scenario_scripts->setEnabled(false);
        
        m_internal->m_ui.terminal_info_table->setRowCount(4);
        m_internal->m_ui.terminal_info_table->setColumnCount(1);

        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("ID"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Unfinished teleport"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(2, new QTableWidgetItem("Finished teleport"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(3, new QTableWidgetItem("Failed teleport"));

        m_internal->m_ui.screen_info_table->setRowCount(2);
        m_internal->m_ui.screen_info_table->setColumnCount(1);

        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("Type"));
        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Resource ID"));

        // Set the button icons
        m_internal->m_ui.terminal_first_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSkipBackward));
        m_internal->m_ui.terminal_prev_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSeekBackward));
        m_internal->m_ui.terminal_next_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSeekForward));
        m_internal->m_ui.terminal_last_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_MediaSkipForward));

        // Screen browser
        m_internal->reset_screen_browser();
    }

    void HuxQt::connect_signals()
    {
        // Menu
        connect(m_internal->m_ui.action_open_scenario, &QAction::triggered, this, &HuxQt::open_scenario);

        connect(m_internal->m_ui.action_save_scenario, &QAction::triggered, this, &HuxQt::save_scenario_action);
        connect(m_internal->m_ui.action_save_scenario_as, &QAction::triggered, this, &HuxQt::save_scenario_as_action);

        connect(m_internal->m_ui.action_export_scenario_scripts, &QAction::triggered, this, &HuxQt::export_scenario_scripts);
        connect(m_internal->m_ui.action_import_scenario_scripts, &QAction::triggered, this, &HuxQt::import_scenario_scripts);
        connect(m_internal->m_ui.action_terminal_preview_config, &QAction::triggered, this, &HuxQt::open_preview_config);
        connect(m_internal->m_ui.action_override_text_colors, &QAction::triggered, this, &HuxQt::override_text_colors);
        connect(m_internal->m_ui.action_use_dark_theme, &QAction::triggered, this, &HuxQt::set_app_theme);

        // Scenario browser
        connect(m_internal->m_ui.scenario_browser, &ScenarioBrowserView::edit_level, this, &HuxQt::edit_level);
        connect(m_internal->m_ui.scenario_browser, &ScenarioBrowserView::terminal_selected, this, &HuxQt::terminal_selected);
        connect(m_internal->m_ui.scenario_browser, &ScenarioBrowserView::terminal_opened, this, &HuxQt::terminal_opened);
        
        connect(&m_internal->m_scenario_browser_model, &ScenarioBrowserModel::scenario_modified, this, &HuxQt::scenario_modified);
        connect(&m_internal->m_scenario_browser_model, &ScenarioBrowserModel::terminal_modified, this, &HuxQt::terminal_modified);
        connect(&m_internal->m_scenario_browser_model, &ScenarioBrowserModel::terminals_removed, this, &HuxQt::terminals_removed);

        // Screen browser
        connect(m_internal->m_ui.screen_browser_tree, &QTreeWidget::currentItemChanged, this, &HuxQt::screen_item_selected);

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
        clear_preview_display();
    }

    void HuxQt::update_title(const QString& text)
    {
        QString title = QStringLiteral("Hux (version %1)").arg(get_app_version_string());
        if (!text.isEmpty())
        {
            title = QStringLiteral("%1 - %2").arg(title).arg(text);
        }

        setWindowTitle(title);
    }

    void HuxQt::open_scenario()
    {
        const QString& current_scenario_path = m_internal->m_scenario_browser_model.get_path();
        const QString init_path = current_scenario_path.isEmpty() ? QStringLiteral("/home") : (current_scenario_path + "/Scenario.json");
        const QString scenario_file = QFileDialog::getOpenFileName(this, tr("Open Scenario File"), init_path, "Scenario File (*.json)");
        if (!scenario_file.isEmpty())
        {
            if (!close_current_scenario())
            {
                // Something went wrong, cancel this request
                return;
            }

            // Clear the UI
            m_internal->clear_terminal_editors();

            Scenario loaded_scenario;
            if (m_core->get_scenario_manager().load_scenario(scenario_file, loaded_scenario))
            {
                // Load successful, use file path for the load function
                QFileInfo scenario_file_info(scenario_file);
                scenario_loaded(loaded_scenario, scenario_file_info.absolutePath());

                // Cache the file name
                m_internal->m_scenario_browser_model.set_file_name(scenario_file_info.fileName());
            }
        }
    }

    void HuxQt::save_scenario_action()
    {
        // Save using the cached file name (equivalent to "Save As" if we haven't saved the file yet)
        save_scenario(m_internal->m_scenario_browser_model.get_file_name());
    }

    void HuxQt::save_scenario_as_action()
    {
        // Use empty string (will prompt user to select a file)
        save_scenario(QString());
    }

    void HuxQt::export_scenario_scripts()
    {
        // Prepare the export dialog (allows one last check to make sure the scripts we will output are correct, also helps with debugging)
        QStringList level_output_list;
        const ScenarioManager& scenario_manager = m_core->get_scenario_manager();
        const Scenario exported_scenario = m_internal->m_scenario_browser_model.export_scenario();
        for (const Level& current_level : exported_scenario.get_levels())
        {
            level_output_list << current_level.get_name();
            level_output_list << scenario_manager.print_level_script(current_level);
        }

        ExportScenarioDialog* export_dialog = new ExportScenarioDialog(this, m_internal->m_scenario_browser_model.get_path(), level_output_list);
        connect(export_dialog, &ExportScenarioDialog::export_path_selected, this, &HuxQt::export_scenario);
        export_dialog->open();
    }

    void HuxQt::import_scenario_scripts()
    {
        const QString scenario_dir = QFileDialog::getExistingDirectory(this, tr("Import Scenario Split Folder"), "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!scenario_dir.isEmpty())
        {
            if (!close_current_scenario())
            {
                // Something went wrong, cancel this request
                return;
            }

            // Clear the UI
            m_internal->clear_terminal_editors();

            Scenario loaded_scenario;
            if (m_core->get_scenario_manager().import_scenario(scenario_dir, loaded_scenario))
            {
                // Import successful, use the split folder path for the load function
                scenario_loaded(loaded_scenario, scenario_dir);

                // Clear file name so the user is prompted when saving
                m_internal->m_scenario_browser_model.set_file_name(QString());
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

    void HuxQt::override_text_colors()
    {
        EditTextColorDialog* edit_color_dialog = new EditTextColorDialog(*m_core);
        edit_color_dialog->open();
    }

    void HuxQt::set_app_theme()
    {
        m_internal->set_app_theme();
    }

    void HuxQt::edit_level(int level_id)
    {
        const LevelInfo level_info = m_internal->m_scenario_browser_model.get_level_info(level_id);
        m_internal->m_edit_level_dialog = new EditLevelDialog(this, level_info);

        connect(m_internal->m_edit_level_dialog, &EditLevelDialog::changes_accepted, this, &HuxQt::level_changes_accepted);
        m_internal->m_edit_level_dialog->open();
    }

    void HuxQt::level_changes_accepted()
    {
        // Validate the level info
        const LevelInfo edited_level_info = m_internal->m_edit_level_dialog->get_level_info();

        QString error_msg;
        if (m_internal->m_scenario_browser_model.update_level_data(edited_level_info, error_msg))
        {
            // Edit successful, close the dialog
            m_internal->m_edit_level_dialog->accept();
            m_internal->m_edit_level_dialog = nullptr;
        }
        else
        {
            // Place warning over the edit dialog
            QMessageBox::warning(m_internal->m_edit_level_dialog, QStringLiteral("Level Error"), QStringLiteral("Invalid level data: %1!").arg(error_msg));
        }
    }

    void HuxQt::terminal_selected(int level_id, int terminal_id)
    {
        TerminalID selected_terminal_id{ level_id, terminal_id };
        if (m_internal->m_selected_terminal != selected_terminal_id)
        {
            m_internal->m_selected_terminal = selected_terminal_id;

            const LevelModel* selected_level = m_internal->m_scenario_browser_model.get_level_model(level_id);
            const Terminal* selected_terminal = selected_level->get_terminal(selected_terminal_id);

            // Update the terminal UI
            m_internal->terminal_selected(terminal_id, *selected_terminal);

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

    void HuxQt::terminal_opened(int level_id, int terminal_id)
    {
        // First check if we already have an editor open for this item
        const TerminalID selected_terminal_id{ level_id, terminal_id };
        TerminalEditorWindow* editor_window = m_internal->find_terminal_editor(selected_terminal_id);

        if (!editor_window)
        {
            editor_window = new TerminalEditorWindow(*m_core, m_internal->m_scenario_browser_model, selected_terminal_id);

            // Connect signals between the editor and the main windows
            QObject::connect(editor_window, &QObject::destroyed, this, &HuxQt::terminal_editor_closed);
            m_internal->m_terminal_editors.insert(editor_window);
        }

        // Bring the editor window to the front
        editor_window->show();
        editor_window->activateWindow();
        editor_window->raise();
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

    void HuxQt::scenario_modified()
    {
        if (!m_internal->m_scenario_modified)
        {
            // Set the title
            update_title(QStringLiteral("%2 (Modified)").arg(m_internal->m_scenario_browser_model.get_name()));
            m_internal->m_scenario_modified = true;
        }
    }

    void HuxQt::display_current_screen()
    {
        if (m_internal->m_selected_terminal.is_valid())
        {
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

            const Terminal::BranchType branch_type = Utils::to_enum<Terminal::BranchType>(m_internal->m_ui.screen_browser_tree->indexOfTopLevelItem(current_screen->parent()));
            const int screen_index = current_screen->parent()->indexOfChild(current_screen);

            const LevelModel* selected_level = m_internal->m_scenario_browser_model.get_level_model(m_internal->m_selected_terminal.m_level_id);
            const Terminal* selected_terminal = selected_level->get_terminal(m_internal->m_selected_terminal);

            const Terminal::Branch& selected_branch = selected_terminal->get_branch(branch_type);
            const Terminal::Screen& selected_screen = selected_branch.m_screens[screen_index];

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

    void HuxQt::terminal_modified(int level_id, int terminal_id)
    {
        const TerminalID modified_terminal_id{ level_id, terminal_id };
        if (m_internal->m_selected_terminal == modified_terminal_id)
        {
            // Reset terminal preview by re-selecting it
            m_internal->m_selected_terminal.invalidate();
            terminal_selected(level_id, terminal_id);
        }
    }

    void HuxQt::terminals_removed(int level_id, const QList<int>& terminal_ids)
    {
        for (int current_terminal_id : terminal_ids)
        {
            const TerminalID removed_terminal_id{ level_id, current_terminal_id };
            if (m_internal->m_selected_terminal == removed_terminal_id)
            {
                // Reset terminal UI
                m_internal->reset_terminal_ui();
                m_internal->m_selected_terminal.invalidate();
            }
        }
    }

    void HuxQt::terminal_editor_closed(QObject* object)
    {
        // Remove the editor window from our lookup
        TerminalEditorWindow* editor_window = static_cast<TerminalEditorWindow*>(sender());
        assert(editor_window);

        m_internal->m_terminal_editors.erase(editor_window);
    }

    bool HuxQt::close_current_scenario()
    {
        if (!m_internal->save_terminal_editors())
        {
            return false;
        }

        if (m_internal->m_scenario_browser_model.is_modified())
        {
            const QMessageBox::StandardButton user_response = QMessageBox::question(this, "Unsaved Changes", "You have modified the scenario. Save changes?",
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel));

            switch (user_response)
            {
            case QMessageBox::Yes:
                return save_scenario(m_internal->m_scenario_browser_model.get_file_name());
            case QMessageBox::No:
                return true;
            case QMessageBox::Cancel:
                return false;
            }
        }

        return true;
    }

    bool HuxQt::save_scenario(const QString& file_name)
    {
        // Check if there is a selected save file (if not, prompt user)
        const QString& current_scenario_path = m_internal->m_scenario_browser_model.get_path();
        QFileInfo file_info(current_scenario_path + "/" + file_name);
        if (file_name.isEmpty())
        {
            // Use the scenario name to generate the file path
            const QString init_path = current_scenario_path + QStringLiteral("/%1.json").arg(m_internal->m_scenario_browser_model.get_name());
            const QString selected_file_path = QFileDialog::getSaveFileName(this, tr("Save Scenario As"), init_path, "Scenario File (*.json)");

            if (!selected_file_path.isEmpty())
            {
                file_info = QFileInfo(selected_file_path);
            }
            else
            {
                return false;
            }
        }

        ScenarioManager& scenario_manager = m_core->get_scenario_manager();
        const Scenario exported_scenario = m_internal->m_scenario_browser_model.export_scenario();
        if (!scenario_manager.save_scenario(file_info.absoluteFilePath(), exported_scenario))
        {
            return false;
        }

        // Save successful, cache the file location and overwrite the name
        m_internal->m_scenario_browser_model.set_name(file_info.baseName());
        m_internal->m_scenario_browser_model.set_path(file_info.absoluteDir().absolutePath());
        m_internal->m_scenario_browser_model.set_file_name(file_info.fileName());

        // Clear all the UI modifications
        m_internal->m_scenario_browser_model.clear_modified();
        m_internal->m_scenario_modified = false;
        
        update_title(m_internal->m_scenario_browser_model.get_name());
        return true;
    }

    bool HuxQt::export_scenario(const QString& export_path)
    {
        ScenarioManager& scenario_manager = m_core->get_scenario_manager();
        const Scenario exported_scenario = m_internal->m_scenario_browser_model.export_scenario();
        if (!scenario_manager.export_scenario(export_path, exported_scenario))
        {
            return false;
        }

        return true;
    }

    void HuxQt::scenario_loaded(const Scenario& scenario, const QString& path)
    {
        // Update the model and view
        reset_ui();

        m_internal->m_scenario_browser_model.load_scenario(scenario);
        m_internal->m_ui.scenario_browser->set_model(&m_internal->m_scenario_browser_model);

        // Update the UI (TODO: this can be deprecated, since we can start with an empty scenario)
        m_internal->m_ui.action_save_scenario->setEnabled(true);
        m_internal->m_ui.action_save_scenario_as->setEnabled(true);
        m_internal->m_ui.action_export_scenario_scripts->setEnabled(true);

        // Update the display system
        m_core->get_display_system().update_resources(path + "/Resources");

        // Cache the scenario path
        m_internal->m_scenario_browser_model.set_path(path);

        // Set the title
        update_title(scenario.get_name());
        m_internal->m_scenario_modified = false;
    }
}