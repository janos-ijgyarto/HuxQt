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
		TerminalEditorWindow(AppCore& core, int level_index, const Terminal& terminal_data, const QString& title);
		~TerminalEditorWindow();

		int get_level_index() const;
		const Terminal& get_terminal_data() const;

		bool is_modified() const;
		void clear_modified();

		bool validate_terminal_info();
		void update_window_title(const QString& title);
		void save_changes();
	signals:
		void editor_closed();
	protected:
		void closeEvent(QCloseEvent* event) override;
	private:
		void connect_signals();
		void init_ui();
		void init_terminal_info();
		void init_screen_editor();
		void update_window_title_internal();

		void terminal_data_modified();
		void screen_edited(bool attributes);
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
		void screen_item_clicked(QListWidgetItem* item);
		void screen_item_double_clicked(QListWidgetItem* item);
		void screen_items_moved();
		void screen_selected(QListWidgetItem* item);

		// Screen browser buttons
		void screen_browser_up_clicked();
		void add_screen_clicked();
		void remove_screen_clicked();

		void update_edit_notification();

		AppCore& m_core;

		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}