#pragma once
#include <QWidget>

namespace HuxApp
{
	class AppCore;
	class ScenarioBrowserModel;
	struct TerminalID;

	class TerminalEditorWindow : public QWidget
	{
		Q_OBJECT
	public:
		TerminalEditorWindow(AppCore& core, ScenarioBrowserModel& model, const TerminalID& terminal_id);
		~TerminalEditorWindow();

		const TerminalID& get_terminal_id() const;

		QMessageBox::StandardButton prompt_save();
		void force_save();
	protected:
		void closeEvent(QCloseEvent* event) override;
	private:
		void connect_signals();
		void init_ui();
		void init_terminal_info();
		void init_screen_editor();

		void terminal_data_modified();
		void screen_edited(bool attributes);
		void update_preview();

		bool validate_terminal_info();
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

		// Misc.
		void update_edit_notification();
		void terminals_removed(int level_id, const QList<int>& terminal_ids);

		AppCore& m_core;

		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}