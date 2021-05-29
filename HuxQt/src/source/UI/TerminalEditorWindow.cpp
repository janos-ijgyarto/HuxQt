#include <stdafx.h>
#include <UI/TerminalEditorWindow.h>
#include <ui_TerminalEditorWindow.h>

#include <AppCore.h>
#include <Scenario/Terminal.h>
#include <Scenario/ScenarioManager.h>

#include <UI/DisplaySystem.h>
#include <UI/DisplayData.h>
#include <UI/ScreenEditTab.h>

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
    }

	struct TerminalEditorWindow::Internal
	{
		Internal(int level_index, const Terminal& terminal_data, const QString& title)
			: m_level_index(level_index)
            , m_terminal_data(terminal_data)
            , m_title(title)
		{}

        void update_screen_browser_buttons()
        {
            QTreeWidgetItem* selected_item = m_ui.screen_browser_tree->currentItem();
            if (selected_item)
            {
                m_ui.new_screen_button->setEnabled(true);
                m_ui.delete_screen_button->setEnabled(true);

                // Only enable movement buttons if it's a screen item
                if (is_screen_item(selected_item))
                {
                    const bool unfinished_group = (m_ui.screen_browser_tree->indexOfTopLevelItem(selected_item->parent()) == 0);
                    const int screen_index = selected_item->parent()->indexOfChild(selected_item);

                    m_ui.move_screen_up_button->setEnabled(screen_index > 0);
                    m_ui.move_screen_down_button->setEnabled(screen_index < (selected_item->parent()->childCount() - 1));
                }
                else
                {
                    m_ui.move_screen_up_button->setEnabled(false);
                    m_ui.move_screen_down_button->setEnabled(false);
                }
            }
            else
            {
                m_ui.new_screen_button->setEnabled(false);
                m_ui.move_screen_up_button->setEnabled(false);
                m_ui.move_screen_down_button->setEnabled(false);
                m_ui.delete_screen_button->setEnabled(false);
            }
        }

        void clear_clipboard() { m_screen_clipboard.clear(); }

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

        QTreeWidgetItem* get_current_item() const { return m_ui.screen_browser_tree->currentItem(); }

        bool is_screen_item(QTreeWidgetItem* item) const { return item && (m_ui.screen_browser_tree->indexOfTopLevelItem(item) == -1); }

        bool is_unfinished_group_item(QTreeWidgetItem* item) const 
        { 
            if (is_screen_item(item))
            {
                return (m_ui.screen_browser_tree->indexOfTopLevelItem(item->parent()) == 0);
            }
            else
            {
                return (m_ui.screen_browser_tree->indexOfTopLevelItem(item) == 0);
            }
        }

        QTreeWidgetItem* get_current_screen() const
        {
            QTreeWidgetItem* current_item = get_current_item();
            if (is_screen_item(current_item))
            {
                return current_item;
            }
            return nullptr;
        }

        void move_screen_item(QTreeWidgetItem* selected_item, bool move_up)
        {
            const bool unfinished = is_unfinished_group_item(selected_item);
            const int selected_index = selected_item->parent()->indexOfChild(selected_item);
            const int new_index = move_up ? selected_index - 1 : selected_index + 1;
            ScenarioManager::move_terminal_screen(m_terminal_data, selected_index, new_index, unfinished);

            // Move the tree item
            QTreeWidgetItem* screen_group_item = selected_item->parent();
            screen_group_item->takeChild(selected_index);
            screen_group_item->insertChild(new_index, selected_item);

            m_ui.screen_browser_tree->setCurrentItem(selected_item);
            screen_modified(selected_item);
            update_screen_browser_buttons();
        }

        void remove_screen_item(QTreeWidgetItem* screen_item)
        {
            const int tab_index = find_screen_tab_index(screen_item);
            if (tab_index >= 0)
            {
                // Delete the tab, ignore unsaved
                QWidget* screen_tab = m_ui.edited_screen_tabs->widget(tab_index);
                m_ui.edited_screen_tabs->removeTab(tab_index);
                screen_tab->deleteLater();
            }
            QTreeWidgetItem* group_item = screen_item->parent();
            group_item->removeChild(screen_item);
            screen_group_modified(group_item);
            delete screen_item;
        }

        int find_screen_tab_index(QTreeWidgetItem* item) const
        {
            for (int current_tab_index = 0; current_tab_index < m_ui.edited_screen_tabs->count(); ++current_tab_index)
            {
                QWidget* current_tab_widget = m_ui.edited_screen_tabs->widget(current_tab_index);
                ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);

                if (current_tab->get_screen_item() == item)
                {
                    return current_tab_index;
                }
            }

            return -1;
        }

        QString get_screen_title(QTreeWidgetItem* item)
        {
            const QString group_string = is_unfinished_group_item(item) ? QStringLiteral("UNFINISHED") : QStringLiteral("FINISHED");
            return QStringLiteral("%1 - %2").arg(group_string, item->text(0));
        }

        void reset_edit_notification()
        {
            m_edit_timer.stop();
            m_ui.screen_update_progress_bar->setValue(0);
            m_ui.screen_update_progress_bar->setVisible(false);
        }

        void screen_group_modified(QTreeWidgetItem* screen_group_item)
        {
            QFont font = screen_group_item->font(0);
            font.setBold(true);
            screen_group_item->setFont(0, font);
        }

        void screen_modified(QTreeWidgetItem* screen_item)
        {
            QFont font = screen_item->font(0);
            font.setBold(true);
            screen_item->setFont(0, font);
            screen_group_modified(screen_item->parent());
        }

        void save_screen_changes(ScreenEditTab* screen_tab)
        {
            if (screen_tab->is_modified())
            {
                QTreeWidgetItem* screen_item = screen_tab->get_screen_item();
                const bool unfinished = is_unfinished_group_item(screen_item);
                const int screen_index = screen_item->parent()->indexOfChild(screen_item);

                m_terminal_data.get_screen(screen_index, unfinished) = screen_tab->get_screen_data();
            }
        }

        void screen_data_modified(QTreeWidgetItem* screen_item, ScreenEditTab* screen_tab)
        {
            screen_item->setText(0, Terminal::get_screen_string(screen_tab->get_screen_data()));
            const int tab_index = m_ui.edited_screen_tabs->indexOf(screen_tab);

            // Set the modified tab's label to red
            QTabBar* tab_bar = m_ui.edited_screen_tabs->tabBar();
            tab_bar->setTabText(tab_index, get_screen_title(screen_item));
            tab_bar->setTabTextColor(tab_index, Qt::red);
        }

		Ui::TerminalEditorWindow m_ui;

        const int m_level_index; // Pointer to the tree widget item in the scenario browser
        Terminal m_terminal_data;
        QString m_title;

        DisplaySystem::ViewID m_view_id;

        // Clipboard (cut/copy & paste)
        std::vector<Terminal::Screen> m_screen_clipboard;

        // Flags
		bool m_modified = false;
        bool m_saved = false;
        bool m_screen_text_dirty = false;
        bool m_resource_browser_enabled = false;

        // Timer that updates the UI after an edit (delays the full update so we don't change the display immediately on every slight change)
        QTimer m_edit_timer;
	};

	TerminalEditorWindow::TerminalEditorWindow(AppCore& core, int level_index, const Terminal& terminal_data, const QString& title)
		: m_core(core)
		, m_internal(std::make_unique<Internal>(level_index, terminal_data, title))
	{
		m_internal->m_ui.setupUi(this);
        m_internal->m_edit_timer.stop();
        m_internal->reset_edit_notification();

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

    void TerminalEditorWindow::save_screens()
    {
        for (int tab_index = 0; tab_index < m_internal->m_ui.edited_screen_tabs->count(); ++tab_index)
        {
            ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(m_internal->m_ui.edited_screen_tabs->widget(tab_index));
            m_internal->save_screen_changes(current_tab);
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
                save_screens();
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
        m_internal->m_ui.screen_browser_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_internal->m_ui.screen_browser_tree, &QTreeWidget::customContextMenuRequested, this, &TerminalEditorWindow::screen_browser_context_menu);
        connect(m_internal->m_ui.screen_browser_tree, &QTreeWidget::itemClicked, this, &TerminalEditorWindow::screen_item_clicked);
        connect(m_internal->m_ui.screen_browser_tree, &QTreeWidget::itemDoubleClicked, this, &TerminalEditorWindow::open_screen);

        connect(m_internal->m_ui.new_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::add_screen_clicked);
        connect(m_internal->m_ui.move_screen_up_button, &QPushButton::clicked, this, &TerminalEditorWindow::move_screen_up_clicked);
        connect(m_internal->m_ui.move_screen_down_button, &QPushButton::clicked, this, &TerminalEditorWindow::move_screen_down_clicked);
        connect(m_internal->m_ui.delete_screen_button, &QPushButton::clicked, this, &TerminalEditorWindow::remove_screen_clicked);

        // Screen edit tabs
        connect(m_internal->m_ui.edited_screen_tabs, &QTabWidget::currentChanged, this, &TerminalEditorWindow::current_tab_changed);
        connect(m_internal->m_ui.edited_screen_tabs, &QTabWidget::tabCloseRequested, this, &TerminalEditorWindow::close_tab);
    }

    void TerminalEditorWindow::init_ui()
    {
        // Set the title
        update_window_title_internal();

        init_terminal_info();
        init_screen_editor();

        // Set appropriate sizing for the splitters
        const int window_height = height();
        m_internal->m_ui.screen_preview_splitter->setSizes({ window_height / 2, window_height / 2 });

        m_internal->m_ui.main_splitter->setStretchFactor(0, 1);
        m_internal->m_ui.main_splitter->setStretchFactor(1, 2);
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
        // Screen browser
        {
            QTreeWidgetItem* unfinished_screens_root = new QTreeWidgetItem(m_internal->m_ui.screen_browser_tree);
            m_internal->init_screen_group_item(unfinished_screens_root, true);

            const auto& unfinished_screens = m_internal->m_terminal_data.get_screens(true);
            QTreeWidgetItem* unfinished_group_item = m_internal->m_ui.screen_browser_tree->topLevelItem(0);
            for (const Terminal::Screen& current_screen : unfinished_screens)
            {
                QTreeWidgetItem* screen_item = new QTreeWidgetItem(unfinished_group_item);
                m_internal->init_screen_item(current_screen, screen_item);
            }
            unfinished_screens_root->setExpanded(true);
        }
        {
            QTreeWidgetItem* finished_screens_root = new QTreeWidgetItem(m_internal->m_ui.screen_browser_tree);
            m_internal->init_screen_group_item(finished_screens_root, false);

            const auto& finished_screens = m_internal->m_terminal_data.get_screens(false);
            QTreeWidgetItem* finished_group_item = m_internal->m_ui.screen_browser_tree->topLevelItem(1);
            for (const Terminal::Screen& current_screen : finished_screens)
            {
                QTreeWidgetItem* screen_item = new QTreeWidgetItem(finished_group_item);
                m_internal->init_screen_item(current_screen, screen_item);
            }
            finished_screens_root->setExpanded(true);
        }

        // Set the button icons
        m_internal->m_ui.new_screen_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileIcon));
        m_internal->m_ui.move_screen_up_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_ArrowUp));
        m_internal->m_ui.move_screen_down_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_ArrowDown));
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
        }
        update_window_title_internal();
    }

    void TerminalEditorWindow::update_preview()
    {
        QWidget* current_tab_widget = m_internal->m_ui.edited_screen_tabs->currentWidget();
        if (current_tab_widget)
        {
            ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);
            current_tab->update_display_text();
            const Terminal::Screen& current_screen_data = current_tab->get_screen_data();

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
        // Save changes and clear the modified flag
        save_screens();
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
        if (QTreeWidgetItem* selected_item = m_internal->m_ui.screen_browser_tree->itemAt(point))
        {
            if (m_internal->is_screen_item(selected_item))
            {
                QMenu context_menu;
                context_menu.addAction("Copy Screen", this, &TerminalEditorWindow::copy_screen_action);
                // Check if we can paste
                if (!m_internal->m_screen_clipboard.empty())
                {
                    context_menu.addAction("Paste Screen", this, &TerminalEditorWindow::paste_screen_action);
                }
                const QPoint global_pos = m_internal->m_ui.screen_browser_tree->mapToGlobal(point);
                context_menu.exec(global_pos);
            }
            else
            {
                // TODO: offer to copy/paste on entire group
            }
        }
    }

    void TerminalEditorWindow::copy_screen_action() 
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_screen())
        {
            m_internal->clear_clipboard();
            const bool unfinished = m_internal->is_unfinished_group_item(selected_item);
            const int screen_index = selected_item->parent()->indexOfChild(selected_item);

            // Check whether the editor was open for a screen
            const int edit_tab_index = m_internal->find_screen_tab_index(selected_item);
            if (edit_tab_index >= 0)
            {
                // Editor is open, copy data from the tab
                ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(m_internal->m_ui.edited_screen_tabs->widget(edit_tab_index));
                m_internal->m_screen_clipboard.push_back(selected_tab->get_screen_data());
            }
            else
            {
                // Copy the cached screen data
                m_internal->m_screen_clipboard.push_back(m_internal->m_terminal_data.get_screen(screen_index, unfinished));
            }
        }
    }
    
    void TerminalEditorWindow::paste_screen_action() 
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_screen())
        {
            // Add the new screen in the selected position
            const bool unfinished = m_internal->is_unfinished_group_item(selected_item);
            const int selected_index = selected_item->parent()->indexOfChild(selected_item);
            const int new_index = selected_index + 1;

            ScenarioManager::add_terminal_screen(m_internal->m_terminal_data, new_index, unfinished);

            // Copy the screen data
            Terminal::Screen& pasted_screen_data = m_internal->m_terminal_data.get_screen(new_index, unfinished);
            pasted_screen_data = m_internal->m_screen_clipboard.front();

            // Add the tree item
            QTreeWidgetItem* screen_group_item = unfinished ? m_internal->m_ui.screen_browser_tree->topLevelItem(0) : m_internal->m_ui.screen_browser_tree->topLevelItem(1);
            QTreeWidgetItem* new_screen_item = new QTreeWidgetItem(screen_group_item, selected_item);
            m_internal->init_screen_item(pasted_screen_data, new_screen_item);

            m_internal->screen_modified(new_screen_item);
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::screen_item_clicked(QTreeWidgetItem* item, int column)
    {
        // Update the buttons
        m_internal->update_screen_browser_buttons();
    }

    void TerminalEditorWindow::open_screen(QTreeWidgetItem* item, int column)
    {
        if (m_internal->is_screen_item(item))
        {
            const int screen_tab_index = m_internal->find_screen_tab_index(item);
            if (screen_tab_index != -1)
            {
                // Tab already open, set it as the current tab
                m_internal->m_ui.edited_screen_tabs->setCurrentIndex(screen_tab_index);
                return;
            }

            // Get the screen data
            const bool unfinished = m_internal->is_unfinished_group_item(item);
            const int screen_index = item->parent()->indexOfChild(item);
            const Terminal::Screen& screen_data = m_internal->m_terminal_data.get_screen(screen_index, unfinished);

            // Add new tab, connect to its signals
            ScreenEditTab* new_tab = new ScreenEditTab(m_core, item, screen_data);
            connect(new_tab, &ScreenEditTab::screen_edited, this, &TerminalEditorWindow::screen_edited);

            const int new_tab_index = m_internal->m_ui.edited_screen_tabs->addTab(new_tab, m_internal->get_screen_title(item));
            m_internal->m_ui.edited_screen_tabs->setCurrentIndex(new_tab_index);
        }
    }

    void TerminalEditorWindow::add_screen_clicked()
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_item())
        {
            const bool is_screen = m_internal->is_screen_item(selected_item);

            // Add the data item
            const bool unfinished = m_internal->is_unfinished_group_item(selected_item);
            const int new_index = is_screen ? selected_item->parent()->indexOfChild(selected_item) : 0;
            ScenarioManager::add_terminal_screen(m_internal->m_terminal_data, new_index, unfinished);

            // Add the tree item
            QTreeWidgetItem* screen_group_item = unfinished ? m_internal->m_ui.screen_browser_tree->topLevelItem(0) : m_internal->m_ui.screen_browser_tree->topLevelItem(1);
            QTreeWidgetItem* new_screen_item = new QTreeWidgetItem(screen_group_item, selected_item);
            m_internal->init_screen_item(m_internal->m_terminal_data.get_screen(new_index, unfinished), new_screen_item);

            m_internal->screen_modified(new_screen_item);
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::move_screen_up_clicked() 
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_screen())
        {
            m_internal->move_screen_item(selected_item, true);
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::move_screen_down_clicked()
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_screen())
        {
            m_internal->move_screen_item(selected_item, false);
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::remove_screen_clicked()
    {
        if (QTreeWidgetItem* selected_item = m_internal->get_current_screen())
        {
            const bool unfinished = m_internal->is_unfinished_group_item(selected_item);
            if (m_internal->is_screen_item(selected_item))
            {            
                const int selected_index = selected_item->parent()->indexOfChild(selected_item);
                ScenarioManager::remove_terminal_screen(m_internal->m_terminal_data, selected_index, unfinished);
                m_internal->remove_screen_item(selected_item);
            }
            else
            {
                // Clear the entire group
                while (selected_item->childCount() > 0)
                {
                    QTreeWidgetItem* current_child = selected_item->child(0);
                    m_internal->remove_screen_item(current_child);
                }
                m_internal->m_terminal_data.get_screens(unfinished).clear();
            }
            terminal_data_modified();
        }
    }

    void TerminalEditorWindow::current_tab_changed(int index)
    {
        m_internal->reset_edit_notification();
        
        // Select the screen corresponding to this tab
        if (ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(m_internal->m_ui.edited_screen_tabs->widget(index)))
        {
            QTreeWidgetItem* screen_item = selected_tab->get_screen_item();
            m_internal->m_ui.screen_browser_tree->setCurrentItem(screen_item);

            update_preview();
        }
    }

    void TerminalEditorWindow::screen_edited(QTreeWidgetItem* screen_item)
    {
        ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(sender());
        m_internal->screen_data_modified(screen_item, selected_tab);

        // Start/restart the timer for updating the preview
        m_internal->m_edit_timer.start(SCREEN_EDIT_TIMER_INTERVAL);
        m_internal->m_ui.screen_update_progress_bar->setValue(0);
        m_internal->m_ui.screen_update_progress_bar->setVisible(true);

        m_internal->screen_modified(screen_item);
        terminal_data_modified();
    }

    void TerminalEditorWindow::close_tab(int index)
    {
        // Save the screen data and delete the tab
        QWidget* selected_tab_widget = m_internal->m_ui.edited_screen_tabs->widget(index);
        ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(selected_tab_widget);
        m_internal->save_screen_changes(selected_tab);

        m_internal->m_ui.edited_screen_tabs->removeTab(index);
        selected_tab->deleteLater();
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