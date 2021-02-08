#include "stdafx.h"
#include "UI/HuxQt.h"
#include "ui_HuxQt.h"

#include "AppCore.h"

#include "Scenario/ScenarioManager.h"
#include "Scenario/Scenario.h"

#include "UI/DisplaySystem.h"
#include "UI/DisplayData.h"
#include "UI/TerminalEditDialog.h"
#include "UI/ScreenEditorWindow.h"
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
            "LOGOFF"
        };

        enum class ScenarioNodeType
        {
            LEVEL,
            TERMINAL,
            SCREEN_GROUP,
            SCREEN
        };

        enum class ScenarioNodeUserRoles
        {
            NODE_TYPE = Qt::UserRole,
            NODE_ID,
            NODE_MODIFIED,
            NODE_OPENED
        };

        ScenarioNodeType get_scenario_node_type(const QTreeWidgetItem* item)
        {
            return static_cast<ScenarioNodeType>(item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE)).toInt());
        }
    }

    struct HuxQt::Internal
    {
        Ui::HuxQtMainWindow m_ui;

        Scenario m_scenario;
        bool m_scenario_edited = false;

        QTreeWidgetItem* m_selected_terminal_item = nullptr;
        QTreeWidgetItem* m_selected_screen_item = nullptr;

        ScreenEditorWindow* m_screen_editor = nullptr;
        PreviewConfigWindow* m_preview_config = nullptr;

        QString get_screen_string(const Terminal::Screen& current_screen) const
        {
            switch (current_screen.m_type)
            {
            case Terminal::ScreenType::LOGON:
                return QStringLiteral("LOGON %1").arg(current_screen.m_resource_id);
            case Terminal::ScreenType::INFORMATION:
                return QStringLiteral("INFORMATION");
            case Terminal::ScreenType::PICT:
                return QStringLiteral("PICT %1").arg(current_screen.m_resource_id);
            case Terminal::ScreenType::CHECKPOINT:
                return QStringLiteral("CHECKPOINT %1").arg(current_screen.m_resource_id);
            case Terminal::ScreenType::LOGOFF:
                return QStringLiteral("LOGOFF %1").arg(current_screen.m_resource_id);
            }

            return "";
        }

        void add_terminal_tree_items(const Terminal& terminal_data, QTreeWidgetItem* terminal_root_item)
        {           
            {
                QTreeWidgetItem* unfinished_group_item = new QTreeWidgetItem(terminal_root_item);
                unfinished_group_item->setText(0, QStringLiteral("UNFINISHED"));
                unfinished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::SCREEN_GROUP));
                unfinished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID), true);
                unfinished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), false);

                const std::vector<Terminal::Screen>&unfinished_screens = terminal_data.get_screens(true);
                for (const Terminal::Screen& current_screen : unfinished_screens)
                {
                    QTreeWidgetItem* screen_item = new QTreeWidgetItem(unfinished_group_item);
                    screen_item->setText(0, get_screen_string(current_screen));
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::SCREEN));
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), false);
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_OPENED), false);
                }
            }

            {
                QTreeWidgetItem* finished_group_item = new QTreeWidgetItem(terminal_root_item);
                finished_group_item->setText(0, QStringLiteral("FINISHED"));
                finished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::SCREEN_GROUP));
                finished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID), false);
                finished_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), false);

                const std::vector<Terminal::Screen>& finished_screens = terminal_data.get_screens(false);
                for (const Terminal::Screen& current_screen : finished_screens)
                {
                    QTreeWidgetItem* screen_item = new QTreeWidgetItem(finished_group_item);
                    screen_item->setText(0, get_screen_string(current_screen));
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::SCREEN));
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), false);
                    screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_OPENED), false);
                }
            }
        }

        void get_terminal_index_path(QTreeWidgetItem* terminal_item, int& level_index, int& terminal_index)
        {
            QTreeWidgetItem* level_item = terminal_item->parent();
            level_index = m_ui.scenario_contents_tree->indexOfTopLevelItem(level_item);
            terminal_index = level_item->indexOfChild(terminal_item);
        }

        void get_screen_index_path(QTreeWidgetItem* screen_item, int& level_index, int& terminal_index, bool& unfinished, int& screen_index)
        {
            // Reverse-search the corresponding screen object using the tree
            QTreeWidgetItem* screen_group_item = screen_item->parent();
            QTreeWidgetItem* terminal_item = screen_group_item->parent();

            get_terminal_index_path(terminal_item, level_index, terminal_index);
            unfinished = (terminal_item->indexOfChild(screen_group_item) == 0);
            screen_index = screen_group_item->indexOfChild(screen_item);
        }

        void display_terminal_info()
        {
            m_ui.terminal_info_table->clearContents();

            int level_index = -1;
            int terminal_index = -1;
            get_terminal_index_path(m_selected_terminal_item, level_index, terminal_index);

            const Level& selected_level = m_scenario.get_level(level_index);
            const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);

            m_ui.terminal_name_label->setText(m_selected_terminal_item->text(0));

            // Set the terminal attributes
            m_ui.terminal_info_table->setItem(0, 0, new QTableWidgetItem(QString::number(selected_terminal.get_id())));  
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

                core.get_display_system().update_display(DisplaySystem::View::MAIN_WINDOW, display_data);
            }

            m_ui.screen_info_table->clearContents();

            m_ui.screen_info_table->setItem(0, 0, new QTableWidgetItem(SCREEN_TYPE_LABELS[Utils::to_integral(screen_data.m_type)]));
            m_ui.screen_info_table->setItem(1, 0, new QTableWidgetItem(QString::number(screen_data.m_resource_id)));
        }

        QTreeWidgetItem* find_terminal_node(const QString& terminal_path)
        {
            QTreeWidgetItemIterator scenario_tree_it(m_ui.scenario_contents_tree);
            while (QTreeWidgetItem* current_item = *scenario_tree_it)
            {
                if (get_scenario_node_type(current_item) == ScenarioNodeType::TERMINAL)
                {
                    const QString current_terminal_path = current_item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID)).toString();
                    if (current_terminal_path == terminal_path)
                    {
                        return current_item;
                    }
                }
                ++scenario_tree_it;
            }
            return nullptr;
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

        // Set the graphics view in the display system
        m_core->get_display_system().set_graphics_view(DisplaySystem::View::MAIN_WINDOW, m_internal->m_ui.terminal_preview);
    }

    HuxQt::~HuxQt()
    {
        if (m_internal->m_preview_config)
        {
            delete m_internal->m_preview_config;
        }
    }

    void HuxQt::reset_ui()
    {
        reset_terminal_ui();
        reset_scenario_ui();
    }

    void HuxQt::reset_scenario_ui()
    {
        m_internal->m_ui.scenario_contents_tree->clear();
        m_internal->m_ui.scenario_name_label->setText(m_internal->m_scenario.get_name());

        // Reset the font
        QFont default_font = m_internal->m_ui.scenario_name_label->font();
        default_font.setBold(false);
        m_internal->m_ui.scenario_name_label->setFont(default_font);

        // Fill the scenario browser tree
        const std::vector<Level>& level_vec = m_internal->m_scenario.get_levels();
        for (const Level& current_level : level_vec)
        {
            QTreeWidgetItem* level_tree_item = new QTreeWidgetItem(m_internal->m_ui.scenario_contents_tree);
            level_tree_item->setText(0, current_level.get_name());
            level_tree_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::LEVEL));
            level_tree_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID), current_level.get_name());

            const std::vector<Terminal>& terminal_vec = current_level.get_terminals();
            for (const Terminal& current_terminal : terminal_vec)
            {
                QTreeWidgetItem* terminal_item = new QTreeWidgetItem(level_tree_item);
                const QString current_terminal_name = QStringLiteral("TERMINAL %1").arg(current_terminal.get_id());

                terminal_item->setText(0, current_terminal_name);
                terminal_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_TYPE), Utils::to_integral(ScenarioNodeType::TERMINAL));

                // Use the "terminal path" as its UID
                const QString terminal_path = QStringLiteral("%1/%2").arg(current_level.get_name(), current_terminal_name);
                terminal_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID), terminal_path);

                m_internal->add_terminal_tree_items(current_terminal, terminal_item);
            }
        }
    }

    void HuxQt::reset_terminal_ui()
    {
        // Terminal info
        m_internal->m_ui.terminal_info_table->clearContents();
        m_internal->m_ui.screen_info_table->clearContents();
        m_internal->m_ui.terminal_name_label->setText("N/A");

        // Terminal preview
        m_internal->m_ui.terminal_first_button->setEnabled(false);
        m_internal->m_ui.terminal_last_button->setEnabled(false);
        m_internal->m_ui.terminal_next_button->setEnabled(false);
        m_internal->m_ui.terminal_prev_button->setEnabled(false);

        m_internal->m_selected_terminal_item = nullptr;
        m_internal->m_selected_screen_item = nullptr;
    }

    QGraphicsView* HuxQt::get_graphics_view() { return m_internal->m_ui.terminal_preview; }

    bool HuxQt::save_terminal_info(const QString& terminal_path, const Terminal& terminal_info)
    {
        // Find the appropriate node using its path
        QTreeWidgetItem* terminal_item = m_internal->find_terminal_node(terminal_path);

        // Must be able to successfully reverse-search the terminal, otherwise something is very wrong!
        assert(terminal_item);

        // Find the relevant level and terminal objects
        int modified_level_index = -1;
        int modified_terminal_index = -1;
        m_internal->get_terminal_index_path(terminal_item, modified_level_index, modified_terminal_index);

        Level& modified_level = m_internal->m_scenario.get_level(modified_level_index);
        Terminal& modified_terminal = modified_level.get_terminal(modified_terminal_index);

        // If we changed the terminal ID, make sure it's unique
        if (modified_terminal.get_id() != terminal_info.get_id())
        {
            int current_terminal_index = 0;
            for (const Terminal& current_terminal : modified_level.get_terminals())
            {
                if ((current_terminal_index != modified_terminal_index) && (current_terminal.get_id() == terminal_info.get_id()))
                {
                    // The IDs conflict, so we cannot accept this change
                    return false;
                }
                ++current_terminal_index;
            }
        }

        // All the data is valid, set the relevant values
        modified_level.set_modified(true);
        modified_terminal.set_id(terminal_info.get_id());
        modified_terminal.get_teleport_info(true) = terminal_info.get_teleport_info(true);
        modified_terminal.get_teleport_info(false) = terminal_info.get_teleport_info(false);

        // Set the new name and path
        const QString new_terminal_name = QStringLiteral("TERMINAL %1").arg(modified_terminal.get_id());
        terminal_item->setText(0, new_terminal_name);

        QTreeWidgetItem* level_item = terminal_item->parent();
        const QString new_terminal_path = QStringLiteral("%1/%2").arg(level_item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID)).toString(), new_terminal_name);
        terminal_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID), new_terminal_path);

        // Set font to bold to indicate which items got modified
        QFont bold_font = level_item->font(0);
        bold_font.setBold(true);
        level_item->setFont(0, bold_font);
        terminal_item->setFont(0, bold_font);

        // Set the flags in the items as well to make sure we can find them later
        level_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        terminal_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        
        // Update the terminal info display
        m_internal->display_terminal_info();

        // Update the scenario state
        scenario_edited();
        return true;
    }

    void HuxQt::screen_edit_tab_closed(QTreeWidgetItem* screen_item)
    {
        screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_OPENED), false);
    }

    void HuxQt::save_screen(QTreeWidgetItem* screen_item, const Terminal::Screen& screen_data)
    {
        screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);

        int modified_level_index = -1;
        int modified_terminal_index = -1;
        bool unfinished = false;
        int modified_screen_index = -1;
        m_internal->get_screen_index_path(screen_item, modified_level_index, modified_terminal_index, unfinished, modified_screen_index);

        Level& modified_level = m_internal->m_scenario.get_level(modified_level_index);
        Terminal& modified_terminal = modified_level.get_terminal(modified_terminal_index);
        Terminal::Screen& modified_screen = modified_terminal.get_screen(modified_screen_index, unfinished);

        modified_screen = screen_data;

        QTreeWidgetItem* screen_group_item = screen_item->parent();
        QTreeWidgetItem* terminal_item = screen_group_item->parent();
        QTreeWidgetItem* level_item = terminal_item->parent();

        // Set font to bold to indicate which items got modified
        QFont bold_font = level_item->font(0);
        bold_font.setBold(true);
        level_item->setFont(0, bold_font);
        terminal_item->setFont(0, bold_font);
        screen_group_item->setFont(0, bold_font);
        screen_item->setFont(0, bold_font);

        // Set the flags in the items as well to make sure we can find them later
        level_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        terminal_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        screen_group_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        screen_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), true);
        
        if (screen_item == m_internal->m_selected_screen_item)
        {
            display_current_screen();
        }
        scenario_edited();
    }

    QString HuxQt::get_screen_path(QTreeWidgetItem* screen_item)
    {
        QTreeWidgetItem* screen_group_item = screen_item->parent();
        QTreeWidgetItem* terminal_item = screen_group_item->parent();
        QTreeWidgetItem* level_item = terminal_item->parent();

        QString screen_path = QStringLiteral("%1/%2/%3/%4").arg(level_item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID)).toString(),
            terminal_item->text(0), screen_group_item->text(0), screen_item->text(0));

        return screen_path;
    }

    void HuxQt::closeEvent(QCloseEvent* event)
    {
        if (!close_current_scenario())
        {
            event->ignore();
            return;
        }

        event->accept();
    }

    void HuxQt::init_ui()
    {
        m_internal->m_ui.action_save_scenario->setEnabled(false);
        m_internal->m_ui.action_export_scenario_scripts->setEnabled(false);

        m_internal->m_ui.scenario_contents_tree->setColumnCount(1);
        
        m_internal->m_ui.terminal_info_table->setRowCount(3);
        m_internal->m_ui.terminal_info_table->setColumnCount(1);

        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("ID"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Unfinished teleport"));
        m_internal->m_ui.terminal_info_table->setVerticalHeaderItem(2, new QTableWidgetItem("Finished teleport"));

        m_internal->m_ui.screen_info_table->setRowCount(2);
        m_internal->m_ui.screen_info_table->setColumnCount(1);

        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(0, new QTableWidgetItem("Type"));
        m_internal->m_ui.screen_info_table->setVerticalHeaderItem(1, new QTableWidgetItem("Resource ID"));
    }

    void HuxQt::connect_signals()
    {
        // Menu
        connect(m_internal->m_ui.action_open_scenario, &QAction::triggered, this, &HuxQt::open_scenario);
        connect(m_internal->m_ui.action_save_scenario, &QAction::triggered, this, &HuxQt::save_scenario);
        connect(m_internal->m_ui.action_export_scenario_scripts, &QAction::triggered, this, &HuxQt::export_scenario_scripts);
        connect(m_internal->m_ui.action_terminal_preview_config, &QAction::triggered, this, &HuxQt::open_preview_config);

        // Tree
        m_internal->m_ui.scenario_contents_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_internal->m_ui.scenario_contents_tree, &QTreeWidget::itemClicked, this, &HuxQt::scenario_tree_clicked);
        connect(m_internal->m_ui.scenario_contents_tree, &QTreeWidget::itemDoubleClicked, this, &HuxQt::scenario_tree_double_clicked);
        connect(m_internal->m_ui.scenario_contents_tree, &QTreeWidget::customContextMenuRequested, this, &HuxQt::scenario_tree_context_menu);

        // Buttons
        connect(m_internal->m_ui.terminal_first_button, &QPushButton::clicked, this, &HuxQt::terminal_first_clicked);
        connect(m_internal->m_ui.terminal_prev_button, &QPushButton::clicked, this, &HuxQt::terminal_prev_clicked);
        connect(m_internal->m_ui.terminal_next_button, &QPushButton::clicked, this, &HuxQt::terminal_next_clicked);
        connect(m_internal->m_ui.terminal_last_button, &QPushButton::clicked, this, &HuxQt::terminal_last_clicked);
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

            if (m_internal->m_screen_editor)
            {
                m_internal->m_screen_editor->clear_editor();
            }

            if(m_core->get_scenario_manager().load_scenario(scenario_dir, m_internal->m_scenario))
            {
                m_internal->m_ui.action_save_scenario->setEnabled(true);
                m_internal->m_ui.action_export_scenario_scripts->setEnabled(true);
                reset_ui();
                
                // Update the display system
                m_core->get_display_system().update_resources(scenario_dir + "/Resources");
            }
            else
            {
                // TODO: error messages!
            }
        }
    }

    bool HuxQt::save_scenario()
    {
        /*if (m_core->get_scenario_manager().save_scenario(m_internal->m_scenario.get_merge_folder_path(), m_internal->m_scenario))
        {
        }*/

        // Clear all the UI modifications
        clear_scenario_edited();
        return true;
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

    void HuxQt::scenario_tree_clicked(QTreeWidgetItem* item, int column)
    {
        // Check what type of node we clicked, change display accordingly
        const ScenarioNodeType node_type = get_scenario_node_type(item);
        switch (node_type)
        {
        case ScenarioNodeType::SCREEN:
            screen_node_selected(item);
            break;
        default:
        {
            // Jump to the first screen child (if possible)
            std::vector<QTreeWidgetItem*> child_queue;
            child_queue.push_back(item);

            while(!child_queue.empty())
            {
                QTreeWidgetItem* current_item = child_queue.back();
                child_queue.pop_back();

                if (current_item->childCount() > 0)
                {
                    // Check the first child, if it's a screen then we are done
                    QTreeWidgetItem* first_child_item = current_item->child(0);
                    if (get_scenario_node_type(first_child_item) == ScenarioNodeType::SCREEN)
                    {
                        screen_node_selected(first_child_item);
                        return;
                    }
                }

                // Queue the children in reverse order (this ensures that we visit the nodes in the correct order)
                for (int child_index = current_item->childCount() - 1; child_index >= 0; --child_index)
                {
                    child_queue.push_back(current_item->child(child_index));
                }
            }
        }
        }
    }

    void HuxQt::scenario_tree_double_clicked(QTreeWidgetItem* item, int column)
    {
        // Check what type of node we clicked, change display accordingly
        const ScenarioNodeType node_type = get_scenario_node_type(item);
        switch (node_type)
        {
        case ScenarioNodeType::TERMINAL:
            terminal_node_double_clicked(item);
            break;
        case ScenarioNodeType::SCREEN:
            screen_node_double_clicked(item);
            break;
        }
    }

    void HuxQt::terminal_node_double_clicked(QTreeWidgetItem* item)
    {
        int level_index = -1;
        int terminal_index = -1;
        m_internal->get_terminal_index_path(item, level_index, terminal_index);

        const Level& selected_level = m_internal->m_scenario.get_level(level_index);
        const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);

        // Open the terminal for editing
        TerminalEditDialog* terminal_edit_dialog = new TerminalEditDialog(this, item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_ID)).toString(), selected_terminal);
        terminal_edit_dialog->open();
    }

    void HuxQt::screen_node_selected(QTreeWidgetItem* item)
    {
        if (item != m_internal->m_selected_screen_item)
        {
            m_internal->m_selected_screen_item = item;
            display_current_screen();
        }
    }

    void HuxQt::screen_node_double_clicked(QTreeWidgetItem* item)
    {
        if (!m_internal->m_screen_editor)
        {
            // Create screen editor
            m_internal->m_screen_editor = new ScreenEditorWindow(*m_core);
            m_internal->m_screen_editor->setWindowIcon(QIcon(":/HuxQt/fat_hux.ico"));
        }

        // Bring up the screen editor
        m_internal->m_screen_editor->show();
        m_internal->m_screen_editor->activateWindow();
        m_internal->m_screen_editor->raise();

        int level_index = -1;
        int terminal_index = -1;
        bool unfinished = false;
        int screen_index = -1;
        m_internal->get_screen_index_path(item, level_index, terminal_index, unfinished, screen_index);

        const Level& selected_level = m_internal->m_scenario.get_level(level_index);
        const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);
        const Terminal::Screen& selected_screen = selected_terminal.get_screen(screen_index, unfinished);

        // Open the screen for editing
        item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_OPENED), true);
        m_internal->m_screen_editor->open_screen(item, selected_screen);
    }

    void HuxQt::scenario_tree_context_menu(const QPoint& point)
    {
        // Check if we clicked on an item
        QTreeWidgetItem* selected_item = m_internal->m_ui.scenario_contents_tree->itemAt(point);
        if (selected_item)
        {
            const ScenarioNodeType node_type = get_scenario_node_type(selected_item);
            QMenu context_menu;
            switch (node_type)
            {
            case ScenarioNodeType::LEVEL:
                context_menu.addAction("Add Terminal", this, &HuxQt::add_terminal);
                break;
            case ScenarioNodeType::TERMINAL:
            {
                QAction* move_up_action = context_menu.addAction("Move Terminal Up", this, &HuxQt::move_terminal);
                move_up_action->setProperty("MoveUp", true);
                QAction* move_down_action = context_menu.addAction("Move Terminal Down", this, &HuxQt::move_terminal);
                move_down_action->setProperty("MoveUp", false);
                context_menu.addAction("Remove Terminal", this, &HuxQt::remove_terminal);
            }
            break;
            case ScenarioNodeType::SCREEN_GROUP:
            {
                context_menu.addAction("Add Screen", this, &HuxQt::add_screen);
                context_menu.addAction("Clear Group", this, &HuxQt::clear_group);
            }
            case ScenarioNodeType::SCREEN:
            {
                QAction* add_above_action = context_menu.addAction("Add Screen Above", this, &HuxQt::add_screen);
                add_above_action->setProperty("AddAbove", true);
                QAction* add_below_action = context_menu.addAction("Add Screen Below", this, &HuxQt::add_screen);
                add_below_action->setProperty("AddAbove", false);

                QAction* move_up_action = context_menu.addAction("Move Screen Up", this, &HuxQt::move_screen);
                move_up_action->setProperty("MoveUp", true);
                QAction* move_down_action = context_menu.addAction("Move Screen Down", this, &HuxQt::move_screen);
                move_down_action->setProperty("MoveUp", false);

                context_menu.addAction("Remove Screen", this, &HuxQt::remove_screen);
            }
                break;
            }

            const QPoint global_pos = m_internal->m_ui.scenario_contents_tree->mapToGlobal(point);
            context_menu.exec(global_pos);
        }
        else
        {
            QMenu context_menu;
            context_menu.addAction("Add Level", this, &HuxQt::add_level);
            const QPoint global_pos = m_internal->m_ui.scenario_contents_tree->mapToGlobal(point);
            context_menu.exec(global_pos);
        }
    }

    void HuxQt::add_level() 
    {

    }

    void HuxQt::add_terminal() 
    {

    }

    void HuxQt::move_terminal()
    {

    }

    void HuxQt::remove_terminal()
    {

    }

    void HuxQt::add_screen()
    {

    }

    void HuxQt::move_screen()
    {

    }

    void HuxQt::remove_screen()
    {

    }

    void HuxQt::clear_group()
    {

    }

    void HuxQt::display_current_screen()
    {
        if (m_internal->m_selected_screen_item)
        {
            int level_index = -1;
            int terminal_index = -1;
            bool unfinished = false;
            int screen_index = -1;
            m_internal->get_screen_index_path(m_internal->m_selected_screen_item, level_index, terminal_index, unfinished, screen_index);

            const Level& selected_level = m_internal->m_scenario.get_level(level_index);
            const Terminal& selected_terminal = selected_level.get_terminal(terminal_index);
            const Terminal::Screen& selected_screen = selected_terminal.get_screen(screen_index, unfinished);

            m_internal->display_screen(*m_core, selected_screen);

            // Additional UI updates
            QTreeWidgetItem* screen_group_item = m_internal->m_selected_screen_item->parent();            
            update_navigation_buttons(screen_index, screen_group_item->childCount());

            QTreeWidgetItem* terminal_item = screen_group_item->parent();
            if (terminal_item != m_internal->m_selected_terminal_item)
            {
                m_internal->m_selected_terminal_item = terminal_item;
                m_internal->display_terminal_info();
            }
        }
    }

    void HuxQt::terminal_first_clicked()
    {
        if (!m_internal->m_selected_screen_item)
        {
            return;
        }

        // Simply jump to the first child
        screen_node_selected(m_internal->m_selected_screen_item->parent()->child(0));
    }
    void HuxQt::terminal_prev_clicked()
    {
        if (!m_internal->m_selected_screen_item)
        {
            return;
        }

        QTreeWidgetItem* parent_item = m_internal->m_selected_screen_item->parent();
        const int screen_index = parent_item->indexOfChild(m_internal->m_selected_screen_item);
        screen_node_selected(parent_item->child(screen_index - 1));
    }
    void HuxQt::terminal_next_clicked()
    {
        if (!m_internal->m_selected_screen_item)
        {
            return;
        }

        QTreeWidgetItem* parent_item = m_internal->m_selected_screen_item->parent();
        const int screen_index = parent_item->indexOfChild(m_internal->m_selected_screen_item);
        screen_node_selected(parent_item->child(screen_index + 1));
    }
    void HuxQt::terminal_last_clicked()
    {
        if (!m_internal->m_selected_screen_item)
        {
            return;
        }

        // Jump to the last child
        QTreeWidgetItem* parent_item = m_internal->m_selected_screen_item->parent();
        screen_node_selected(parent_item->child(parent_item->childCount() - 1));
    }

    void HuxQt::update_navigation_buttons(int current_index, int screen_count)
    {
        const bool after_start = (current_index > 0);
        const bool before_end = (current_index < (screen_count - 1));

        m_internal->m_ui.terminal_first_button->setEnabled(after_start);
        m_internal->m_ui.terminal_prev_button->setEnabled(after_start);
        m_internal->m_ui.terminal_next_button->setEnabled(before_end);
        m_internal->m_ui.terminal_last_button->setEnabled(before_end);
    }

    void HuxQt::scenario_edited()
    {
        if (!m_internal->m_scenario_edited)
        {
            // Modify the label so the user can see that the scenario has changed
            m_internal->m_ui.scenario_name_label->setText(m_internal->m_ui.scenario_name_label->text() + " - Modified");
            
            QFont bold_font = m_internal->m_ui.scenario_name_label->font();
            bold_font.setBold(true);            
            m_internal->m_ui.scenario_name_label->setFont(bold_font);

            // Set the flag
            m_internal->m_scenario_edited = true;
        }
    }

    void HuxQt::clear_scenario_edited()
    {
        // Reset the terminal/screen displays
        reset_terminal_ui();

        // Go over the scenario tree and reset any relevant data/formatting
        QTreeWidgetItemIterator scenario_tree_it(m_internal->m_ui.scenario_contents_tree);
        while (QTreeWidgetItem* current_item = *scenario_tree_it)
        {
            const bool node_modified = current_item->data(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED)).toBool();
            if (node_modified)
            {
                // Reset font
                QFont default_font = current_item->font(0);
                default_font.setBold(false);

                current_item->setFont(0, default_font);

                // Reset flag
                current_item->setData(0, Utils::to_integral(ScenarioNodeUserRoles::NODE_MODIFIED), false);
            }
            ++scenario_tree_it;
        }

        // Reset the scenario label
        m_internal->m_ui.scenario_name_label->setText(m_internal->m_scenario.get_name());
        QFont default_font = m_internal->m_ui.scenario_name_label->font();
        default_font.setBold(false);
        m_internal->m_ui.scenario_name_label->setFont(default_font);

        // Reset the main flag
        m_internal->m_scenario_edited = false;
    }

    bool HuxQt::close_current_scenario()
    {
        if (m_internal->m_screen_editor)
        {
            if (!m_internal->m_screen_editor->check_unsaved())
            {
                // Cancelled close through screen editor
                return false;
            }
        }

        if (m_internal->m_scenario_edited)
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
}