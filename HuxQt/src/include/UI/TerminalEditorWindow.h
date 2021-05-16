#pragma once
#include <QWidget>

namespace HuxApp
{
	class AppCore;
	class Terminal;

	class TerminalEditorWindow : public QWidget
	{
		Q_OBJECT
	public:
		TerminalEditorWindow(AppCore& core, QTreeWidgetItem* terminal_item, const Terminal& terminal_data);
		~TerminalEditorWindow();
		
		QTreeWidgetItem* get_terminal_item() const;
		const Terminal& get_terminal_data() const;

		bool is_modified() const;
		void clear_modified();

		bool validate_terminal_info();
		void update_window_title();
		void save_screens();
	signals:
		void editor_closed();
	protected:
		void closeEvent(QCloseEvent* event) override;
	private:
		void connect_signals();
		void init_ui();
		void init_terminal_info();
		void init_screen_editor();

		void terminal_data_modified();
		void update_preview();

		bool gather_teleport_info(bool unfinished);

		// Dialog buttons
		void ok_clicked();
		void cancel_clicked();

		// Context menus
		void screen_browser_context_menu(const QPoint& point);
		void copy_screen_action();
		void paste_screen_action();

		// Screen browser
		void screen_item_clicked(QTreeWidgetItem* item, int column);
		void open_screen(QTreeWidgetItem* item, int column);

		// Screen browser buttons
		void add_screen_clicked();
		void move_screen_up_clicked();
		void move_screen_down_clicked();
		void remove_screen_clicked();

		// Screen tab slots
		void current_tab_changed(int index);
		void screen_edited(QTreeWidgetItem* screen_item);
		void close_tab(int index);

		void update_edit_notification();

		AppCore& m_core;

		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}