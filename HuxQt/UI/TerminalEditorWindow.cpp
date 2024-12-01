#include <HuxQt/UI/TerminalEditorWindow.h>
#include <ui_TerminalEditorWindow.h>

#include <HuxQt/AppCore.h>
#include <HuxQt/Scenario/ScenarioBrowserModel.h>

#include <HuxQt/UI/DisplaySystem.h>
#include <HuxQt/UI/DisplayData.h>
#include <HuxQt/UI/TeleportEditWidget.h>
#include <HuxQt/UI/ScreenEditWidget.h>

#include <HuxQt/Utils/Utilities.h>

#include <unordered_set>

#include <QMenu>
#include <QTimer>

namespace HuxApp
{
    namespace
    {
        constexpr int SCREEN_EDIT_TIMEOUT_MS = 1000; // Wait this number of ms after the last user edit before updating the screen preview
        constexpr int SCREEN_EDIT_PROGRESS_STEP_COUNT = 100; // Have this many steps in the progress bar shown to the user
        constexpr int SCREEN_EDIT_TIMER_INTERVAL = SCREEN_EDIT_TIMEOUT_MS / SCREEN_EDIT_PROGRESS_STEP_COUNT;

        template<typename T>
        T clamp(T value, T min, T max)
        {
            if(value < min)
            {
                return min;
            }

            if(value > max)
            {
                return max;
            }

            return value;
        }

        enum class ScreenBrowserState
        {
            SCREEN_GROUPS,
            SCREEN_LIST
        };

        struct Screen
        {
            Terminal::Screen m_data;
            int m_id = -1;
            bool m_modified = false;
        };

        struct ScreenGroup
        {
            TeleportEditWidget* m_teleport_widget = nullptr;
            std::vector<Screen> m_screens;
            bool m_modified = false;
        };

        struct TerminalScreenData
        {
            std::array<ScreenGroup, Utils::to_integral(Terminal::BranchType::TYPE_COUNT)> m_screen_groups;

            int m_screen_id_counter = 0;

            ScreenGroup& get_screen_group(Terminal::BranchType branch) { return m_screen_groups[Utils::to_integral(branch)]; }

            void init(const Terminal& terminal_data)
            {
                auto screen_group_it = m_screen_groups.begin();
                for (const Terminal::Branch& current_branch : terminal_data.get_branches())
                {
                    ScreenGroup& current_screen_group = *screen_group_it;
                    for (const Terminal::Screen& current_screen_data : current_branch.m_screens)
                    {
                        current_screen_group.m_screens.emplace_back();
                        Screen& current_screen = current_screen_group.m_screens.back();
                        current_screen.m_data = current_screen_data;
                        current_screen.m_id = m_screen_id_counter++;
                    }

                    ++screen_group_it;
                }
            }

            void export_data(Terminal& terminal_data)
            {
                for (int current_branch_index = 0; current_branch_index < Utils::to_integral(Terminal::BranchType::TYPE_COUNT); ++current_branch_index)
                {
                    const ScreenGroup& current_screen_group = m_screen_groups[current_branch_index];

                    const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
                    Terminal::Branch& current_branch = terminal_data.get_branch(current_branch_type);

                    current_branch.m_screens.clear();
                    for (const Screen& current_screen_data : current_screen_group.m_screens)
                    {
                        current_branch.m_screens.push_back(current_screen_data.m_data);
                    }
                }
            }

            void reorder_screens(const std::vector<int>& new_id_list, const std::unordered_set<int>& moved_ids, Terminal::BranchType branch)
            {
                ScreenGroup& screen_group = get_screen_group(branch);
                assert(new_id_list.size() == screen_group.m_screens.size());

                const std::vector<Screen> screen_group_copy = screen_group.m_screens;
                auto screen_it = screen_group.m_screens.begin();
                for (int current_id : new_id_list)
                {
                    auto moving_screen_it = std::find_if(screen_group_copy.begin(), screen_group_copy.end(),
                        [current_id](const Screen& current_screen)
                        {
                            return (current_screen.m_id == current_id);
                        }
                    );
                    assert(moving_screen_it != screen_group_copy.end());

                    *screen_it = *moving_screen_it;
                    // Indicate which terminals were moved
                    if (moved_ids.find(current_id) != moved_ids.end())
                    {
                        screen_it->m_modified = true;
                    }
                    ++screen_it;
                }
                screen_group.m_modified = true;
            }

            void add_screen(Terminal::BranchType branch, int index)
            {
                ScreenGroup& screen_group = get_screen_group(branch);
                const size_t screen_index = clamp(size_t(index), size_t(0), screen_group.m_screens.size());

                // Add the screen data object
                auto new_screen_it = screen_group.m_screens.emplace(screen_group.m_screens.begin() + screen_index);
                new_screen_it->m_id = m_screen_id_counter++;
                new_screen_it->m_modified = true;

                screen_group.m_modified = true;
            }

            void add_screens(const std::vector<Terminal::Screen>& new_screens, Terminal::BranchType branch, int index)
            {
                ScreenGroup& screen_group = get_screen_group(branch);
                const size_t screen_index = clamp(size_t(index), size_t(0), screen_group.m_screens.size());
                auto new_screen_it = screen_group.m_screens.begin() + screen_index;

                for (const Terminal::Screen& new_screen_data : new_screens)
                {
                    // Add the screen data object
                    new_screen_it = screen_group.m_screens.emplace(new_screen_it);
                    new_screen_it->m_data = new_screen_data;
                    new_screen_it->m_id = m_screen_id_counter++;
                    new_screen_it->m_modified = true;

                    // Step the iterator so we insert the next object after it
                    ++new_screen_it;
                }
                screen_group.m_modified = true;
            }

            void remove_screens(const std::vector<int>& screen_indices, Terminal::BranchType branch)
            {
                ScreenGroup& screen_group = get_screen_group(branch);
                const std::vector<Screen> prev_screens = screen_group.m_screens;
                screen_group.m_screens.clear();

                int current_index = 0;
                for (const Screen& current_screen : prev_screens)
                {
                    if (std::find(screen_indices.begin(), screen_indices.end(), current_index) == screen_indices.end())
                    {
                        // Index not among those to be removed
                        screen_group.m_screens.push_back(current_screen);
                    }
                    ++current_index;
                }
                screen_group.m_modified = true;
            }

            Screen* find_screen_data(int screen_id)
            {
                for (ScreenGroup& current_screen_group : m_screen_groups)
                {
                    auto screen_it = std::find_if(current_screen_group.m_screens.begin(), current_screen_group.m_screens.end(),
                        [screen_id](const Screen& current_screen)
                        {
                            return (current_screen.m_id == screen_id);
                        }
                    );

                    if (screen_it != current_screen_group.m_screens.end())
                    {
                        return &(*screen_it);
                    }
                }

                return nullptr;
            }

            const Screen* find_screen_data(int screen_id, Terminal::BranchType branch)
            {
                const ScreenGroup& screen_group = get_screen_group(branch);
                auto screen_it = std::find_if(screen_group.m_screens.begin(), screen_group.m_screens.end(), 
                    [screen_id](const Screen& current_screen)
                    {
                        return (current_screen.m_id == screen_id);
                    }
                );

                if (screen_it != screen_group.m_screens.end())
                {
                    return &(*screen_it);
                }

                return nullptr;
            }
        };

        QString get_screen_label(const Terminal::Screen& screen_data, int screen_id)
        {
            return QStringLiteral("%1 (%2)").arg(Terminal::get_screen_string(screen_data)).arg(screen_id);
        }

        QString get_screen_full_label(const Terminal::Screen& screen_data, int screen_id, Terminal::BranchType branch)
        {
            return QStringLiteral("%1 - %2").arg(Terminal::get_branch_type_name(branch)).arg(get_screen_label(screen_data, screen_id));
        }
    }

	struct TerminalEditorWindow::Internal
	{
        Ui::TerminalEditorWindow m_ui;

        ScenarioBrowserModel& m_model;
        const TerminalID m_terminal_id;
        Terminal m_terminal_data;

        DisplaySystem::ViewID m_view_id;

        // Screen browser
        ScreenBrowserState m_screen_browser_state = ScreenBrowserState::SCREEN_GROUPS;
        Terminal::BranchType m_current_branch = Terminal::BranchType::UNFINISHED;
        TerminalScreenData m_screen_data;

        int m_selected_screen_id = -1;
        Terminal::BranchType m_selected_screen_branch = Terminal::BranchType::UNFINISHED;

        // Flags
        bool m_modified = false;

        // Timer that updates the UI after an edit (delays the full update so we don't change the display immediately on every slight change)
        QTimer m_edit_timer;

		Internal(const TerminalID& terminal_id, ScenarioBrowserModel& model)
			: m_model(model)
            , m_terminal_id(terminal_id)
		{
            LevelModel* selected_level = model.get_level_model(terminal_id.m_level_id);
            if (const Terminal* selected_terminal = selected_level->get_terminal(terminal_id))
            {
                m_terminal_data = *selected_terminal;
            }
        }

        void init_screen_group_item(QListWidgetItem* screen_group_item, const ScreenGroup& group, Terminal::BranchType branch)
        {
            screen_group_item->setText(Terminal::get_branch_type_name(branch));
            screen_group_item->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_DirIcon));
            
            if (group.m_modified)
            {
                QFont font = screen_group_item->font();
                font.setBold(true);
                screen_group_item->setFont(font);
            }
        }

        void init_screen_item(QListWidgetItem* screen_item, const Screen& screen_data)
        {
            screen_item->setText(QStringLiteral("%1 (%2)").arg(Terminal::get_screen_string(screen_data.m_data)).arg(screen_data.m_id));
            screen_item->setData(Qt::UserRole, screen_data.m_id);
            screen_item->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogDetailedView));
            if (screen_data.m_modified)
            {
                QFont font = screen_item->font();
                font.setBold(true);
                screen_item->setFont(font);
            }
        }

        void update_screen_browser_buttons()
        {
            if (m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
            {
                m_ui.screen_browser_up_button->setEnabled(true);
                m_ui.new_screen_button->setEnabled(true);
                m_ui.delete_screen_button->setEnabled(!get_selected_screens().isEmpty());
            }
            else
            {
                m_ui.screen_browser_up_button->setEnabled(false);
                m_ui.new_screen_button->setEnabled(false);
                m_ui.delete_screen_button->setEnabled(false);
            }
        }

        void reset_screen_browser()
        {
            // Reset view
            m_ui.screen_browser_view->clear();

            if (m_screen_browser_state == ScreenBrowserState::SCREEN_GROUPS)
            {
                for (int current_branch_index = 0; current_branch_index < Utils::to_integral(Terminal::BranchType::TYPE_COUNT); ++current_branch_index)
                {
                    const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
                    const ScreenGroup& screen_group = m_screen_data.get_screen_group(current_branch_type);

                    QListWidgetItem* current_group_item = new QListWidgetItem(m_ui.screen_browser_view);

                    init_screen_group_item(current_group_item, screen_group, current_branch_type);
                }
            }
            else
            {
                const ScreenGroup& screen_group = m_screen_data.get_screen_group(m_current_branch);
                for (const Screen& current_screen : screen_group.m_screens)
                {
                    QListWidgetItem* screen_item = new QListWidgetItem(m_ui.screen_browser_view);
                    init_screen_item(screen_item, current_screen);
                }
            }
            update_screen_browser_buttons();
        }

        void show_screen_groups()
        {
            m_screen_browser_state = ScreenBrowserState::SCREEN_GROUPS;

            // Change selection and drag & drop mode
            m_ui.screen_browser_view->setDragDropMode(QAbstractItemView::DragDropMode::NoDragDrop);
            m_ui.screen_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

            reset_screen_browser();
        }

        void open_screen_group(Terminal::BranchType branch)
        {
            m_screen_browser_state = ScreenBrowserState::SCREEN_LIST;
            m_current_branch = branch;

            // Change selection and drag & drop mode
            m_ui.screen_browser_view->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);
            m_ui.screen_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);

            reset_screen_browser();
        }

        QList<QListWidgetItem*> get_selected_screens()
        {
            assert(m_screen_browser_state == ScreenBrowserState::SCREEN_LIST);
            return m_ui.screen_browser_view->selectedItems();
        }

        int get_screen_item_index(QListWidgetItem* screen_item)
        {
            assert(m_screen_browser_state == ScreenBrowserState::SCREEN_LIST);
            return m_ui.screen_browser_view->row(screen_item);
        }

        int get_screen_item_index(int screen_id)
        {
            assert(m_screen_browser_state == ScreenBrowserState::SCREEN_LIST);
            for (int current_row = 0; current_row < m_ui.screen_browser_view->count(); ++current_row)
            {
                QListWidgetItem* current_item = m_ui.screen_browser_view->item(current_row);
                if (current_item->data(Qt::UserRole).toInt() == screen_id)
                {
                    return current_row;
                }
            }
            return -1;
        }

        int get_screen_item_id(QListWidgetItem* screen_item)
        {
            assert(m_screen_browser_state == ScreenBrowserState::SCREEN_LIST);
            return screen_item->data(Qt::UserRole).toInt();
        }

        void save_screen_changes()
        {
            if ((m_selected_screen_id >= 0) && m_ui.screen_edit_widget->is_modified())
            {
                if (Screen* screen_data = m_screen_data.find_screen_data(m_selected_screen_id))
                {
                    screen_data->m_data = m_ui.screen_edit_widget->get_screen_data();
                    screen_data->m_modified = true;
                }
            }
        }

        void save_teleport_info()
        {
            for (int current_branch_index = 0; current_branch_index < Utils::to_integral(Terminal::BranchType::TYPE_COUNT); ++current_branch_index)
            {
                const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
                Terminal::Branch& current_branch = m_terminal_data.get_branch(current_branch_type);
    
                const TeleportEditWidget* teleport_edit_widget = m_screen_data.m_screen_groups[current_branch_index].m_teleport_widget;
                current_branch.m_teleport = teleport_edit_widget->get_teleport_info();
            }
        }

        void clear_screen_editor()
        {
            m_ui.screen_edit_widget->setEnabled(false);
            m_ui.screen_edit_widget->clear_editor();
            m_ui.current_screen_label->setText(QStringLiteral("N/A"));
        }

        void screen_items_moved()
        {
            // Cache the new ID order and the items that moved
            std::vector<int> new_id_list;
            std::unordered_set<int> moved_ids;
            for (int current_row = 0; current_row < m_ui.screen_browser_view->count(); ++current_row)
            {
                QListWidgetItem* current_screen_item = m_ui.screen_browser_view->item(current_row);
                const int screen_id = get_screen_item_id(current_screen_item);

                // Assume selected items were those that we moved (via drag & drop)
                if (current_screen_item->isSelected())
                {
                    moved_ids.insert(screen_id);
                }
                new_id_list.push_back(screen_id);
            }

            // Reorder the data
            m_screen_data.reorder_screens(new_id_list, moved_ids, m_current_branch);

            // Reset the view to show the new order
            reset_screen_browser();

            // Re-select the items that were moved
            for (int current_row = 0; current_row < m_ui.screen_browser_view->count(); ++current_row)
            {
                QListWidgetItem* current_screen_item = m_ui.screen_browser_view->item(current_row);
                const int screen_id = get_screen_item_id(current_screen_item);
                if (moved_ids.find(screen_id) != moved_ids.end())
                {
                    current_screen_item->setSelected(true);
                }
            }
        }

        void reset_edit_notification()
        {
            m_edit_timer.stop();
            m_ui.screen_update_progress_bar->setValue(0);
            m_ui.screen_update_progress_bar->setVisible(false);
        }

        void save_changes()
        {
            if (m_modified)
            {
                save_screen_changes();

                // Export the terminal changes
                m_terminal_data.set_name(m_ui.name_edit->text());
                m_screen_data.export_data(m_terminal_data);

                // Save the teleport info
                save_teleport_info();

                m_model.update_terminal_data(m_terminal_id, m_terminal_data);

                m_modified = false;
            }
        }
	};

	TerminalEditorWindow::TerminalEditorWindow(AppCore& core, ScenarioBrowserModel& model, const TerminalID& terminal_id)
		: m_core(core)
		, m_internal(std::make_unique<Internal>(terminal_id, model))
	{
		m_internal->m_ui.setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose);

        m_internal->m_ui.screen_edit_widget->initialize(core); // Initialize the screen editor

        m_internal->m_edit_timer.stop();
        m_internal->reset_edit_notification();

        m_internal->m_screen_data.init(m_internal->m_terminal_data);

        init_ui();
        connect_signals();
	}

    TerminalEditorWindow::~TerminalEditorWindow()
    {
        DisplaySystem& display_system = m_core.get_display_system();
        display_system.release_graphics_view(m_internal->m_view_id, m_internal->m_ui.screen_preview);
    }

    const TerminalID& TerminalEditorWindow::get_terminal_id() const { return m_internal->m_terminal_id; }

    QMessageBox::StandardButton TerminalEditorWindow::prompt_save()
    {
        if (m_internal->m_modified)
        {
            // Prompt user if they want to save changes
            const QMessageBox::StandardButton user_response = QMessageBox::question(this, "Terminal Modified",
                QStringLiteral("Save changes to this terminal?"),
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel));

            if ((user_response == QMessageBox::Yes) || (user_response == QMessageBox::YesToAll))
            {
                force_save();
            }

            return user_response;
        }

        return QMessageBox::StandardButton::Yes;
    }

    void TerminalEditorWindow::force_save() { m_internal->save_changes(); }

    void TerminalEditorWindow::closeEvent(QCloseEvent* event)
    {
        // Prompt in case we modified the terminal and did not click OK
        const QMessageBox::StandardButton user_response = prompt_save();
        if (user_response == QMessageBox::Cancel)
        {
            // Stop the window from closing
            event->ignore();
            return;
        }

        QWidget::closeEvent(event);
    }

    void TerminalEditorWindow::connect_signals()
    {
        // Scenario browser model
        connect(&m_internal->m_model, &QObject::destroyed, this, &QObject::deleteLater); // Delete window if the model is being destroyed (i.e we are closing the app)
        
        LevelModel* selected_level = m_internal->m_model.get_level_model(m_internal->m_terminal_id.m_level_id);
        connect(selected_level, &QObject::destroyed, this, &QObject::deleteLater);
        connect(selected_level, &LevelModel::terminals_removed, this, &TerminalEditorWindow::terminals_removed);

        // Terminal info controls
        connect(m_internal->m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &TerminalEditorWindow::ok_clicked);
        connect(m_internal->m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &TerminalEditorWindow::cancel_clicked);

        connect(m_internal->m_ui.name_edit, &QLineEdit::textEdited, this, &TerminalEditorWindow::terminal_data_modified);

        connect(&m_internal->m_edit_timer, &QTimer::timeout, this, &TerminalEditorWindow::update_edit_notification);

        // Screen browser
        m_internal->m_ui.screen_browser_view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::customContextMenuRequested, this, &TerminalEditorWindow::screen_browser_context_menu);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::itemClicked, this, &TerminalEditorWindow::screen_item_clicked);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::itemDoubleClicked, this, &TerminalEditorWindow::screen_item_double_clicked);
        connect(m_internal->m_ui.screen_browser_view, &UI::ScenarioBrowserWidget::items_dropped, this, &TerminalEditorWindow::screen_items_moved, Qt::ConnectionType::QueuedConnection);

        connect(m_internal->m_ui.screen_browser_up_button, &QPushButton::clicked, this, &TerminalEditorWindow::screen_browser_up_clicked);
        connect(m_internal->m_ui.new_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::add_screen_clicked);
        connect(m_internal->m_ui.delete_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::remove_screen_clicked);

        // Screen editor
        connect(m_internal->m_ui.screen_edit_widget, &ScreenEditWidget::screen_edited, this, &TerminalEditorWindow::screen_edited);
    }

    void TerminalEditorWindow::init_ui()
    {
        // Set the title
        const QString terminal_name = m_internal->m_terminal_data.get_name().isEmpty() ? QStringLiteral("TERMINAL (%1)").arg(m_internal->m_terminal_id.m_terminal_id) : m_internal->m_terminal_data.get_name();

        const LevelInfo level_info = m_internal->m_model.get_level_info(m_internal->m_terminal_id.m_level_id);
        setWindowTitle(QStringLiteral("%1 / %2").arg(level_info.m_name).arg(terminal_name));

        init_terminal_info();
        init_screen_editor();

        // Set appropriate sizing for the splitters
        const int window_height = height();
        m_internal->m_ui.screen_edit_splitter->setSizes({ window_height / 2, window_height / 2 });

        m_internal->m_ui.main_splitter->setStretchFactor(0, 1);
        m_internal->m_ui.main_splitter->setStretchFactor(1, 2);

        m_internal->show_screen_groups();
    }

    void TerminalEditorWindow::init_terminal_info()
    {
        // Create the teleport editors
        for (int current_branch_index = 0; current_branch_index < Utils::to_integral(Terminal::BranchType::TYPE_COUNT); ++current_branch_index)
        {
            const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
            ScreenGroup& current_screen_group = m_internal->m_screen_data.m_screen_groups[current_branch_index];

            current_screen_group.m_teleport_widget = new TeleportEditWidget(QString(Terminal::get_branch_type_name(current_branch_type)), m_internal->m_terminal_data.get_branch(current_branch_type).m_teleport);

            connect(current_screen_group.m_teleport_widget, &TeleportEditWidget::teleport_info_modified, this, &TerminalEditorWindow::terminal_data_modified);

            m_internal->m_ui.teleport_edit_vbox->addWidget(current_screen_group.m_teleport_widget);
        }

        m_internal->m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);

        m_internal->m_ui.name_edit->setText(m_internal->m_terminal_data.get_name());
    }

    void TerminalEditorWindow::init_screen_editor()
    {
        m_internal->m_ui.screen_edit_widget->setEnabled(false);

        // Set the button icons
        m_internal->m_ui.screen_browser_up_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogToParent));
        m_internal->m_ui.new_screen_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileIcon));
        m_internal->m_ui.delete_screen_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_TrashIcon));
        
        // Register view with the display system
        DisplaySystem& display_system = m_core.get_display_system();
        m_internal->m_view_id = display_system.register_graphics_view(m_internal->m_ui.screen_preview);

        m_internal->update_screen_browser_buttons();
    }

    void TerminalEditorWindow::terminal_data_modified()
    {
        if (!m_internal->m_modified)
        {
            m_internal->m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
            m_internal->m_modified = true;

            // Adjust title
            setWindowTitle(QStringLiteral("%1 (Modified)").arg(windowTitle()));
        }
    }

    void TerminalEditorWindow::screen_edited(bool attributes)
    {
        if (m_internal->m_selected_screen_id >= 0)
        {
            const int screen_item_index = m_internal->get_screen_item_index(m_internal->m_selected_screen_id);
            QListWidgetItem* screen_item = m_internal->m_ui.screen_browser_view->item(screen_item_index);

            if (attributes)
            {
                // Update the label and the item text
                const Terminal::Screen& edited_screen_data = m_internal->m_ui.screen_edit_widget->get_screen_data();
                screen_item->setText(get_screen_label(edited_screen_data, m_internal->m_selected_screen_id));
                m_internal->m_ui.current_screen_label->setText(get_screen_full_label(edited_screen_data, m_internal->m_selected_screen_id, m_internal->m_selected_screen_branch));
            }

            Screen* screen_data = m_internal->m_screen_data.find_screen_data(m_internal->m_selected_screen_id);
            if (screen_data && !screen_data->m_modified)
            {
                QFont font = screen_item->font();
                font.setBold(true);
                screen_item->setFont(font);
                screen_data->m_modified = true;
            }

            // Start/restart the timer for updating the preview
            m_internal->m_edit_timer.start(SCREEN_EDIT_TIMER_INTERVAL);
            m_internal->m_ui.screen_update_progress_bar->setValue(0);
            m_internal->m_ui.screen_update_progress_bar->setVisible(true);

            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::update_preview()
    {
        if (m_internal->m_selected_screen_id >= 0)
        {
            // Update the HTML text based on the AO script
            m_internal->m_ui.screen_edit_widget->update_display_text();

            // Update the preview based on the screen data
            const Terminal::Screen& current_screen_data = m_internal->m_ui.screen_edit_widget->get_screen_data();
            if (current_screen_data.m_type != Terminal::ScreenType::NONE)
            {
                // Update the terminal preview display
                DisplayData display_data;
                display_data.m_resource_id = current_screen_data.m_resource_id;
                display_data.m_text = current_screen_data.m_display_text;
                display_data.m_screen_type = current_screen_data.m_type;
                display_data.m_alignment = current_screen_data.m_alignment;

                const int line_count = m_core.get_display_system().update_display(m_internal->m_view_id, display_data);
                const int page_count = DisplaySystem::get_page_count(line_count);
                if (page_count > 1)
                {
                    // Line count will exceed a single page, display how many pages it spans
                    m_internal->m_ui.screen_page_label->setText(QStringLiteral("Page count: %1").arg(page_count));
                }
                else
                {
                    // Line count within bounds, clear the label
                    m_internal->m_ui.screen_page_label->clear();
                }
            }
        }
    }

    bool TerminalEditorWindow::validate_terminal_info()
    {
        if (m_internal->m_modified)
        {
            // Gather and validate terminal info
            if (!gather_teleport_info())
            {
                QMessageBox::warning(this, "Terminal Error", "Active teleport must have valid destination (field must not be empty)!");
                return false;
            }
        }
        return true;
    }

    bool TerminalEditorWindow::gather_teleport_info()
    {
        bool valid = true;
        int current_branch_index = 0;
        for (const ScreenGroup& current_screen_group : m_internal->m_screen_data.m_screen_groups)
        {
            const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);

            Terminal::Branch& current_branch = m_internal->m_terminal_data.get_branch(current_branch_type);
            current_branch.m_teleport = current_screen_group.m_teleport_widget->get_teleport_info();

            if (!current_screen_group.m_teleport_widget->is_valid())
            {
                valid = false;
            }

            ++current_branch_index;
        }

        return valid;
    }

    void TerminalEditorWindow::ok_clicked()
    {
        // Save changes and set the relevant flag
        force_save();
        close();
    }

    void TerminalEditorWindow::cancel_clicked() 
    { 
        // Close without saving
        m_internal->m_modified = false;
        close(); 
    }

    void TerminalEditorWindow::screen_browser_context_menu(const QPoint& point)
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
        {
            QMenu context_menu;
            QListWidgetItem* selected_item = m_internal->m_ui.screen_browser_view->itemAt(point);
            if (selected_item)
            {
                // TODO: shortcut!
                context_menu.addAction("Copy", this, &TerminalEditorWindow::copy_screen_action);
            }

            // Check if we can paste
            const Terminal* screen_clipboard = m_core.get_scenario_manager().get_screen_clipboard();
            if (screen_clipboard)
            {
                QAction* paste_action = context_menu.addAction("Paste", this, &TerminalEditorWindow::paste_screen_action);
                const int selected_item_index = selected_item ? m_internal->get_screen_item_index(selected_item) : -1;
                paste_action->setData(selected_item_index);
            }
            const QPoint global_pos = m_internal->m_ui.screen_browser_view->mapToGlobal(point);
            context_menu.exec(global_pos);
        }
    }

    void TerminalEditorWindow::copy_screen_action() 
    {
        // Cache the changes of the most recently edited screen
        m_internal->save_screen_changes();

        // Store copies of the selected screens in the clipboard
        Terminal clipboard_terminal;

        Terminal::Branch& clipboard_screen_group = clipboard_terminal.get_branch(Terminal::BranchType::UNFINISHED); // Always use the UNFINISHED group, we're just using the terminal object as a container
        const ScreenGroup& edited_screen_group = m_internal->m_screen_data.get_screen_group(m_internal->m_current_branch);

        QList<QListWidgetItem*> selected_screens = m_internal->get_selected_screens();
        for (QListWidgetItem* current_screen_item : selected_screens)
        {
            const int screen_index = m_internal->get_screen_item_index(current_screen_item);
            clipboard_screen_group.m_screens.push_back(edited_screen_group.m_screens[screen_index].m_data);
        }

        m_core.get_scenario_manager().set_screen_clipboard(clipboard_terminal);
    }
    
    void TerminalEditorWindow::paste_screen_action() 
    {
        // Paste at selection (or at the end if nothing is selected)
        QAction* paste_action = qobject_cast<QAction*>(sender());
        const int selected_index = paste_action->data().toInt();
        const int paste_index = (selected_index >= 0) ? selected_index : m_internal->m_ui.screen_browser_view->count();

        // Always read screens from unfinished group (we're only using the terminal object as a container)
        const Terminal::Branch& clipboard_screen_group = m_core.get_scenario_manager().get_screen_clipboard()->get_branch(Terminal::BranchType::UNFINISHED);
        m_internal->m_screen_data.add_screens(clipboard_screen_group.m_screens, m_internal->m_current_branch, paste_index);
 
        // Reset view to include the new items
        m_internal->reset_screen_browser();

        // Select the newly inserted screens
        for (int new_screen_index = paste_index; new_screen_index < (paste_index + clipboard_screen_group.m_screens.size()); ++new_screen_index)
        {
            m_internal->m_ui.screen_browser_view->item(new_screen_index)->setSelected(true);
        }

        // Update flags
        terminal_data_modified();
    }

    void TerminalEditorWindow::screen_item_clicked(QListWidgetItem* item)
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
        {
            m_internal->update_screen_browser_buttons();
            screen_selected(item);
        }
    }

    void TerminalEditorWindow::screen_item_double_clicked(QListWidgetItem* item)
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_GROUPS)
        {
            const Terminal::BranchType selected_branch = Utils::to_enum<Terminal::BranchType>(m_internal->m_ui.screen_browser_view->row(item));
            m_internal->open_screen_group(selected_branch);
        }
    }

    void TerminalEditorWindow::screen_items_moved()
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
        {
            m_internal->screen_items_moved();

            // Make sure we register this as a change
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::screen_selected(QListWidgetItem* item)
    {
        if (item && item->isSelected())
        {
            const int screen_id = m_internal->get_screen_item_id(item);
            if (screen_id != m_internal->m_selected_screen_id)
            {
                // Save the previous screen's data
                m_internal->save_screen_changes();

                m_internal->m_selected_screen_id = screen_id;
                m_internal->m_selected_screen_branch = m_internal->m_current_branch;

                // Get the new screen data
                const Screen* screen_data = m_internal->m_screen_data.find_screen_data(screen_id, m_internal->m_current_branch);
                if (screen_data)
                {
                    // Update the label
                    m_internal->m_ui.current_screen_label->setText(get_screen_full_label(screen_data->m_data, screen_id, m_internal->m_current_branch));

                    // Reset the editor
                    m_internal->m_ui.screen_edit_widget->reset_editor(screen_data->m_data);
                    m_internal->m_ui.screen_edit_widget->setEnabled(true);

                    // Update the preview
                    update_preview();
                }
            }
        }
    }

    void TerminalEditorWindow::screen_browser_up_clicked()
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
        {
            m_internal->show_screen_groups();
        }
    }

    void TerminalEditorWindow::add_screen_clicked()
    {
        // Save the previous screen's data (if applicable)
        m_internal->save_screen_changes();

        // Add a new screen
        QList<QListWidgetItem*> selected_screens = m_internal->get_selected_screens();
        const int selected_index = selected_screens.isEmpty() ? m_internal->m_ui.screen_browser_view->count() : m_internal->get_screen_item_index(selected_screens.front());
        m_internal->m_screen_data.add_screen(m_internal->m_current_branch, selected_index);

        // Reset view to include the new item
        m_internal->reset_screen_browser();

        // Set the new item to be selected
        QListWidgetItem* new_item = m_internal->m_ui.screen_browser_view->item(selected_index);
        new_item->setSelected(true);
        screen_selected(new_item);

        // Update flags
        terminal_data_modified();
    }

    void TerminalEditorWindow::remove_screen_clicked()
    {
        QList<QListWidgetItem*> selected_screens = m_internal->get_selected_screens();
        if (!selected_screens.isEmpty())
        {
            // Save the previous screen's data (if applicable)
            m_internal->save_screen_changes();

            std::vector<int> selected_screen_indices;

            for (QListWidgetItem* current_screen_item : selected_screens)
            {
                const int screen_index = m_internal->get_screen_item_index(current_screen_item);
                const int screen_id = m_internal->get_screen_item_id(current_screen_item);
                if (screen_id == m_internal->m_selected_screen_id)
                {
                    // One of the deleted screens was selected for editing, clear the UI
                    m_internal->clear_screen_editor();
                    m_internal->m_selected_screen_id = -1;
                }

                selected_screen_indices.push_back(screen_index);
            }

            // Remove the screens
            m_internal->m_screen_data.remove_screens(selected_screen_indices, m_internal->m_current_branch);

            // Reset view to show changes
            m_internal->reset_screen_browser();

            // Update flags
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::update_edit_notification()
    {
        const int current_progress = m_internal->m_ui.screen_update_progress_bar->value();
        if (current_progress < SCREEN_EDIT_PROGRESS_STEP_COUNT)
        {
            m_internal->m_ui.screen_update_progress_bar->setValue(current_progress + 1);
        }

        if (m_internal->m_ui.screen_update_progress_bar->value() >= SCREEN_EDIT_PROGRESS_STEP_COUNT)
        {
            m_internal->reset_edit_notification();
            update_preview();
        }
    }

    void TerminalEditorWindow::terminals_removed(int level_id, const QList<int>& terminal_ids)
    {
        if (terminal_ids.indexOf(m_internal->m_terminal_id.m_terminal_id) >= 0)
        {
            // The terminal this window was editing has been deleted, remove the window as well
            deleteLater();
        }
    }
}
