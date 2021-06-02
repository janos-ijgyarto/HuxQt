#include <stdafx.h>
#include <UI/TerminalEditorWindow.h>
#include <ui_TerminalEditorWindow.h>

#include <AppCore.h>
#include <Scenario/Terminal.h>
#include <Scenario/ScenarioManager.h>

#include <UI/DisplaySystem.h>
#include <UI/DisplayData.h>
#include <UI/ScreenEditWidget.h>

#include <Utils/Utilities.h>

namespace HuxApp
{
    namespace
    {
        constexpr int SCREEN_EDIT_TIMEOUT_MS = 1000; // Wait this number of ms after the last user edit before updating the screen preview
        constexpr int SCREEN_EDIT_PROGRESS_STEP_COUNT = 100; // Have this many steps in the progress bar shown to the user
        constexpr int SCREEN_EDIT_TIMER_INTERVAL = SCREEN_EDIT_TIMEOUT_MS / SCREEN_EDIT_PROGRESS_STEP_COUNT;

        constexpr const char* TELEPORT_TYPE_LABELS[Utils::to_integral(Terminal::TeleportType::TYPE_COUNT)] =
        {
            "NONE",
            "INTERLEVEL",
            "INTRALEVEL"
        };

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
            std::vector<Screen> m_screens;
            bool m_modified = false;
        };

        struct TerminalScreenData
        {
            ScreenGroup m_unfinished_screens;
            ScreenGroup m_finished_screens;

            int m_screen_id_counter = 0;

            void init(const Terminal& terminal_data)
            {
                {
                    const std::vector<Terminal::Screen>& unfinished_screens = terminal_data.get_screens(true);
                    for (const Terminal::Screen& current_screen_data : unfinished_screens)
                    {
                        Screen& current_screen = m_unfinished_screens.m_screens.emplace_back();
                        current_screen.m_data = current_screen_data;
                        current_screen.m_id = m_screen_id_counter++;
                    }
                }
                {
                    const std::vector<Terminal::Screen>& finished_screens = terminal_data.get_screens(false);
                    for (const Terminal::Screen& current_screen_data : finished_screens)
                    {
                        Screen& current_screen = m_finished_screens.m_screens.emplace_back();
                        current_screen.m_data = current_screen_data;
                        current_screen.m_id = m_screen_id_counter++;
                    }
                }
            }

            void export_data(Terminal& terminal_data)
            {
                {
                    std::vector<Terminal::Screen>& unfinished_screens = terminal_data.get_screens(true);
                    unfinished_screens.clear();
                    for (const Screen& current_screen_data : m_unfinished_screens.m_screens)
                    {
                        unfinished_screens.push_back(current_screen_data.m_data);
                    }
                }
                {
                    std::vector<Terminal::Screen>& finished_screens = terminal_data.get_screens(false);
                    finished_screens.clear();
                    for (const Screen& current_screen_data : m_finished_screens.m_screens)
                    {
                        finished_screens.push_back(current_screen_data.m_data);
                    }
                }
            }

            void reorder_screens(const std::vector<int>& new_id_list, const std::unordered_set<int>& moved_ids, bool unfinished)
            {
                ScreenGroup& screen_group = unfinished ? m_unfinished_screens : m_finished_screens;
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

            void add_screen(bool unfinished, int index)
            {
                ScreenGroup& screen_group = unfinished ? m_unfinished_screens : m_finished_screens;
                const size_t screen_index = std::clamp(size_t(index), size_t(0), screen_group.m_screens.size());

                // Add the screen data object
                auto new_screen_it = screen_group.m_screens.emplace(screen_group.m_screens.begin() + screen_index);
                new_screen_it->m_id = m_screen_id_counter++;
                new_screen_it->m_modified = true;

                screen_group.m_modified = true;
            }

            void add_screens(const std::vector<Terminal::Screen>& new_screens, bool unfinished, int index)
            {
                ScreenGroup& screen_group = unfinished ? m_unfinished_screens : m_finished_screens;
                const size_t screen_index = std::clamp(size_t(index), size_t(0), screen_group.m_screens.size());
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

            void remove_screens(const std::vector<int>& screen_indices, bool unfinished)
            {
                ScreenGroup& screen_group = unfinished ? m_unfinished_screens : m_finished_screens;
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

            Screen& find_screen_data(int screen_id)
            {
                // First try the unfinished screens
                auto screen_it = std::find_if(m_unfinished_screens.m_screens.begin(), m_unfinished_screens.m_screens.end(),
                    [screen_id](const Screen& current_screen)
                    {
                        return (current_screen.m_id == screen_id);
                    }
                );

                if (screen_it == m_unfinished_screens.m_screens.end())
                {
                    // Try the finished screens
                    screen_it = std::find_if(m_finished_screens.m_screens.begin(), m_finished_screens.m_screens.end(),
                        [screen_id](const Screen& current_screen)
                        {
                            return (current_screen.m_id == screen_id);
                        }
                    );

                    // We should have found a screen by now
                    assert(screen_it != m_finished_screens.m_screens.end());
                }
                return *screen_it;
            }

            const Screen& find_screen_data(int screen_id, bool unfinished)
            {
                const ScreenGroup& screen_group = unfinished ? m_unfinished_screens : m_finished_screens;
                auto screen_it = std::find_if(screen_group.m_screens.begin(), screen_group.m_screens.end(), 
                    [screen_id](const Screen& current_screen)
                    {
                        return (current_screen.m_id == screen_id);
                    }
                );

                assert(screen_it != screen_group.m_screens.end());
                return *screen_it;
            }
        };

        QString get_screen_label(const Terminal::Screen& screen_data, int screen_id)
        {
            return QStringLiteral("%1 (%2)").arg(Terminal::get_screen_string(screen_data)).arg(screen_id);
        }

        QString get_screen_full_label(const Terminal::Screen& screen_data, int screen_id, bool unfinished)
        {
            const QString screen_label = get_screen_label(screen_data, screen_id);
            const QString prefix = unfinished ? QStringLiteral("UNFINISHED - ") : QStringLiteral("FINISHED - ");
            return prefix + screen_label;
        }
    }

	struct TerminalEditorWindow::Internal
	{
        Ui::TerminalEditorWindow m_ui;

        const int m_level_index; // Pointer to the tree widget item in the scenario browser
        Terminal m_terminal_data;
        QString m_title;

        DisplaySystem::ViewID m_view_id;

        // Screen browser
        ScreenBrowserState m_screen_browser_state = ScreenBrowserState::SCREEN_GROUPS;
        bool m_in_unfinished_group = true;
        TerminalScreenData m_screen_data;

        int m_selected_screen_id = -1;
        bool m_selected_group_unfinished = true;

        // Flags
        bool m_modified = false;
        bool m_saved = false;

        // Timer that updates the UI after an edit (delays the full update so we don't change the display immediately on every slight change)
        QTimer m_edit_timer;

		Internal(int level_index, const Terminal& terminal_data, const QString& title)
			: m_level_index(level_index)
            , m_terminal_data(terminal_data)
            , m_title(title)
		{}

        void init_screen_group_item(QListWidgetItem* screen_group_item, const ScreenGroup& group, bool unfinished)
        {
            if (unfinished)
            {
                screen_group_item->setText(QStringLiteral("UNFINISHED"));
            }
            else
            {
                screen_group_item->setText(QStringLiteral("FINISHED"));
            }
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
            const bool screen_list_state = (m_screen_browser_state == ScreenBrowserState::SCREEN_LIST);
            m_ui.screen_browser_up_button->setEnabled(screen_list_state);
            m_ui.new_screen_button->setEnabled(screen_list_state);
            m_ui.delete_screen_button->setEnabled(screen_list_state);
        }

        void reset_screen_browser()
        {
            // Reset view
            m_ui.screen_browser_view->clear();

            if (m_screen_browser_state == ScreenBrowserState::SCREEN_GROUPS)
            {
                QListWidgetItem* unfinished_group_item = new QListWidgetItem(m_ui.screen_browser_view);
                init_screen_group_item(unfinished_group_item, m_screen_data.m_unfinished_screens, true);

                QListWidgetItem* finished_group_item = new QListWidgetItem(m_ui.screen_browser_view);
                init_screen_group_item(finished_group_item, m_screen_data.m_finished_screens, false);
            }
            else
            {
                const ScreenGroup& screen_group = m_in_unfinished_group ? m_screen_data.m_unfinished_screens : m_screen_data.m_finished_screens;
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

        void open_screen_group(bool unfinished)
        {
            m_screen_browser_state = ScreenBrowserState::SCREEN_LIST;
            m_in_unfinished_group = unfinished;

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
                Screen& screen_data = m_screen_data.find_screen_data(m_selected_screen_id);
                screen_data.m_data = m_ui.screen_edit_widget->get_screen_data();
                screen_data.m_modified = true;
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
            m_screen_data.reorder_screens(new_id_list, moved_ids, m_in_unfinished_group);

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
	};

	TerminalEditorWindow::TerminalEditorWindow(AppCore& core, int level_index, const Terminal& terminal_data, const QString& title)
		: m_core(core)
		, m_internal(std::make_unique<Internal>(level_index, terminal_data, title))
	{
		m_internal->m_ui.setupUi(this);
        m_internal->m_edit_timer.stop();
        m_internal->reset_edit_notification();

        m_internal->m_screen_data.init(terminal_data);

        init_ui();
        connect_signals();
	}

    TerminalEditorWindow::~TerminalEditorWindow()
    {
        DisplaySystem& display_system = m_core.get_display_system();
        display_system.release_graphics_view(m_internal->m_view_id, m_internal->m_ui.screen_preview);
    }

    int TerminalEditorWindow::get_level_index() const { return m_internal->m_level_index; }
    const Terminal& TerminalEditorWindow::get_terminal_data() const { return m_internal->m_terminal_data; }

	bool TerminalEditorWindow::is_modified() const { return m_internal->m_modified; }
	void TerminalEditorWindow::clear_modified() 
	{
		m_internal->m_modified = false;
        m_internal->m_saved = false;
        update_window_title_internal();
	}

    bool TerminalEditorWindow::validate_terminal_info()
    {
        if (m_internal->m_modified)
        {
            // Gather and validate terminal info
            if (!gather_teleport_info(true) || !gather_teleport_info(false))
            {
                QMessageBox::warning(this, "Terminal Error", "Active teleport must have valid destination (field must not be empty)!");
                return false;
            }
        }
        return true;
    }

    void TerminalEditorWindow::update_window_title(const QString& title)
    {
        m_internal->m_title = title;
        update_window_title_internal();
    }

    void TerminalEditorWindow::save_changes()
    {
        if (m_internal->m_modified)
        {
            m_internal->save_screen_changes();

            // Export the terminal changes
            m_internal->m_screen_data.export_data(m_internal->m_terminal_data);
        }
    }

    void TerminalEditorWindow::closeEvent(QCloseEvent* event)
    {
        // Prompt in case we modified the terminal and did not click OK
        if (m_internal->m_modified && !m_internal->m_saved)
        {
            // Prompt user if they want to save changes
            const QMessageBox::StandardButton user_response = QMessageBox::question(this, "Terminal Modified",
                QStringLiteral("Save changes to this terminal?"),
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel));

            switch(user_response)
            {
            case QMessageBox::Yes:
            {
                save_changes();
            }
            break;
            case QMessageBox::No:
            {
                // Unset the flag so we don't read the data from here
                m_internal->m_modified = false;
                break;
            }
            case QMessageBox::Cancel:
            {
                // Stop the window from closing
                event->ignore();
                return;
            }
            }
        }

        emit(editor_closed());
        QWidget::closeEvent(event);
    }

    void TerminalEditorWindow::connect_signals()
    {
        // Terminal info controls
        connect(m_internal->m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &TerminalEditorWindow::ok_clicked);
        connect(m_internal->m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &TerminalEditorWindow::cancel_clicked);

        connect(m_internal->m_ui.unfinished_teleport_edit, &QLineEdit::textEdited, this, &TerminalEditorWindow::terminal_data_modified);
        connect(m_internal->m_ui.finished_teleport_edit, &QLineEdit::textEdited, this, &TerminalEditorWindow::terminal_data_modified);
        connect(m_internal->m_ui.unfinished_teleport_type, QOverload<int>::of(&QComboBox::activated), this, &TerminalEditorWindow::terminal_data_modified);
        connect(m_internal->m_ui.finished_teleport_type, QOverload<int>::of(&QComboBox::activated), this, &TerminalEditorWindow::terminal_data_modified);

        connect(&m_internal->m_edit_timer, &QTimer::timeout, this, &TerminalEditorWindow::update_edit_notification);

        // Screen browser
        m_internal->m_ui.screen_browser_view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::customContextMenuRequested, this, &TerminalEditorWindow::screen_browser_context_menu);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::itemClicked, this, &TerminalEditorWindow::screen_item_clicked);
        connect(m_internal->m_ui.screen_browser_view, &QListWidget::itemDoubleClicked, this, &TerminalEditorWindow::screen_item_double_clicked);
        connect(m_internal->m_ui.screen_browser_view, &Utils::ScenarioBrowserWidget::items_dropped, this, &TerminalEditorWindow::screen_items_moved, Qt::ConnectionType::QueuedConnection);

        connect(m_internal->m_ui.screen_browser_up_button, &QPushButton::clicked, this, &TerminalEditorWindow::screen_browser_up_clicked);
        connect(m_internal->m_ui.new_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::add_screen_clicked);
        connect(m_internal->m_ui.delete_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::remove_screen_clicked);

        // Screen editor
        connect(m_internal->m_ui.screen_edit_widget, &ScreenEditWidget::screen_edited, this, &TerminalEditorWindow::screen_edited);
    }

    void TerminalEditorWindow::init_ui()
    {
        // Set the title
        update_window_title_internal();

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
        // Set validators for the inputs
        m_internal->m_ui.unfinished_teleport_edit->setValidator(new QIntValidator(this));
        m_internal->m_ui.finished_teleport_edit->setValidator(new QIntValidator(this));

        for (int teleport_type_index = 0; teleport_type_index < Utils::to_integral(Terminal::TeleportType::TYPE_COUNT); ++teleport_type_index)
        {
            m_internal->m_ui.unfinished_teleport_type->addItem(TELEPORT_TYPE_LABELS[teleport_type_index]);
            m_internal->m_ui.finished_teleport_type->addItem(TELEPORT_TYPE_LABELS[teleport_type_index]);
        }

        m_internal->m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);

        // Teleport info
        {
            const Terminal::Teleport& unfinished_teleport = m_internal->m_terminal_data.get_teleport_info(true);
            m_internal->m_ui.unfinished_teleport_type->setCurrentIndex(Utils::to_integral(unfinished_teleport.m_type));
            if (unfinished_teleport.m_type != Terminal::TeleportType::NONE)
            {
                m_internal->m_ui.unfinished_teleport_edit->setText(QString::number(unfinished_teleport.m_index));
            }
        }

        {
            const Terminal::Teleport& finished_teleport = m_internal->m_terminal_data.get_teleport_info(false);
            m_internal->m_ui.finished_teleport_type->setCurrentIndex(Utils::to_integral(finished_teleport.m_type));

            m_internal->m_ui.finished_teleport_type->setCurrentIndex(Utils::to_integral(finished_teleport.m_type));
            if (finished_teleport.m_type != Terminal::TeleportType::NONE)
            {
                m_internal->m_ui.finished_teleport_edit->setText(QString::number(finished_teleport.m_index));
            }
        }
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

    void TerminalEditorWindow::update_window_title_internal()
    {
        if (m_internal->m_modified)
        {
            setWindowTitle(QStringLiteral("Terminal Editor - ") + m_internal->m_title + QStringLiteral(" (Modified)"));
        }
        else
        {
            setWindowTitle(QStringLiteral("Terminal Editor - ") + m_internal->m_title);
        }
    }

    void TerminalEditorWindow::terminal_data_modified()
    {
        if (!m_internal->m_modified)
        {
            m_internal->m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
            m_internal->m_modified = true;
            update_window_title_internal();
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
                m_internal->m_ui.current_screen_label->setText(get_screen_full_label(edited_screen_data, m_internal->m_selected_screen_id, m_internal->m_selected_group_unfinished));
            }

            Screen& screen_data = m_internal->m_screen_data.find_screen_data(m_internal->m_selected_screen_id);
            if (!screen_data.m_modified)
            {
                QFont font = screen_item->font();
                font.setBold(true);
                screen_item->setFont(font);
                screen_data.m_modified = true;
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

    bool TerminalEditorWindow::gather_teleport_info(bool unfinished)
    {
        Terminal::Teleport& teleport_info = m_internal->m_terminal_data.get_teleport_info(unfinished);
        QLineEdit* teleport_index_edit = unfinished ? m_internal->m_ui.unfinished_teleport_edit : m_internal->m_ui.finished_teleport_edit;
        QComboBox* teleport_type_combo = unfinished ? m_internal->m_ui.unfinished_teleport_type : m_internal->m_ui.finished_teleport_type;

        // First set the type
        teleport_info.m_type = static_cast<Terminal::TeleportType>(teleport_type_combo->currentIndex());

        if (teleport_info.m_type != Terminal::TeleportType::NONE)
        {
            // Active teleport, so the user must provide a valid input
            const QString teleport_index_text = teleport_index_edit->text();
            if (teleport_index_text.isEmpty())
            {
                return false;
            }
            teleport_info.m_index = teleport_index_text.toInt();
        }
        else
        {
            teleport_info.m_index = -1;
        }
        return true;
    }

    void TerminalEditorWindow::ok_clicked()
    {
        // Save changes and set the relevant flag
        save_changes();
        m_internal->m_saved = true;
        close();
    }

    void TerminalEditorWindow::cancel_clicked() 
    { 
        // Close without saving
        clear_modified();
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

        std::vector<Terminal::Screen>& clipboard_screen_group = clipboard_terminal.get_screens(true); // Always use the UNFINISHED group, we're just using the terminal object as a container
        const ScreenGroup& edited_screen_group = m_internal->m_in_unfinished_group ? m_internal->m_screen_data.m_unfinished_screens : m_internal->m_screen_data.m_finished_screens;

        QList<QListWidgetItem*> selected_screens = m_internal->get_selected_screens();
        for (QListWidgetItem* current_screen_item : selected_screens)
        {
            const int screen_index = m_internal->get_screen_item_index(current_screen_item);
            clipboard_screen_group.push_back(edited_screen_group.m_screens[screen_index].m_data);
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
        const std::vector<Terminal::Screen>& clipboard_screen_group = m_core.get_scenario_manager().get_screen_clipboard()->get_screens(true);
        m_internal->m_screen_data.add_screens(clipboard_screen_group, m_internal->m_in_unfinished_group, paste_index);

        // Reset view to include the new items
        m_internal->reset_screen_browser();

        // Select the newly inserted screens
        for (int new_screen_index = paste_index; new_screen_index < (paste_index + clipboard_screen_group.size()); ++new_screen_index)
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
            screen_selected(item);
        }
    }

    void TerminalEditorWindow::screen_item_double_clicked(QListWidgetItem* item)
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_GROUPS)
        {
            const bool unfinished_group = (m_internal->m_ui.screen_browser_view->row(item) == 0);
            m_internal->open_screen_group(unfinished_group);
        }
    }

    void TerminalEditorWindow::screen_items_moved()
    {
        if (m_internal->m_screen_browser_state == ScreenBrowserState::SCREEN_LIST)
        {
            m_internal->screen_items_moved();
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
                m_internal->m_selected_group_unfinished = m_internal->m_in_unfinished_group;

                // Get the new screen data
                const Screen& screen_data = m_internal->m_screen_data.find_screen_data(screen_id, m_internal->m_in_unfinished_group);

                // Update the label
                m_internal->m_ui.current_screen_label->setText(get_screen_full_label(screen_data.m_data, screen_id, m_internal->m_in_unfinished_group));

                // Reset the editor
                m_internal->m_ui.screen_edit_widget->reset_editor(screen_data.m_data);
                m_internal->m_ui.screen_edit_widget->setEnabled(true);

                // Update the preview
                update_preview();
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
        // Add a new screen
        QList<QListWidgetItem*> selected_screens = m_internal->get_selected_screens();
        const int selected_index = selected_screens.isEmpty() ? m_internal->m_ui.screen_browser_view->count() : m_internal->get_screen_item_index(selected_screens.front());
        m_internal->m_screen_data.add_screen(m_internal->m_in_unfinished_group, selected_index);

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
            m_internal->m_screen_data.remove_screens(selected_screen_indices, m_internal->m_in_unfinished_group);

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
}