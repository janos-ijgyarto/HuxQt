#pragma once
#include <ui_ScenarioBrowserView.h>

namespace HuxApp
{
	class LevelModel;
	class ScenarioBrowserModel;

	class ScenarioBrowserView : public QWidget
	{
		Q_OBJECT
	public:
		ScenarioBrowserView(QWidget* parent = nullptr);
		~ScenarioBrowserView();

		void set_model(ScenarioBrowserModel* model);
	signals:
		void edit_level(int level_id);
		void terminal_selected(int level_id, int terminal_id);
		void terminal_opened(int level_id, int terminal_id);
		void level_removed(int level_id);
		void terminals_removed(const QList<int>& level_ids, const QList<int>& terminal_ids);
	private:
		void init_ui();
		void connect_signals();

		bool in_level() const;

		// Mouse events
		void scenario_item_clicked(const QModelIndex& index);
		void scenario_item_double_clicked(const QModelIndex& index);
		void scenario_view_context_menu(const QPoint& point);

		// Button handlers
		void up_button_clicked();
		void add_button_clicked();
		void remove_button_clicked();

		void terminal_item_selected(const QModelIndex& index);
		void terminal_item_double_clicked(const QModelIndex& index);

		// Scenario edit functions
		void add_level();
		void add_terminal();
		void remove_level();
		void remove_terminals();

		void level_removed();
		void terminals_removed();

		// Context menu actions
		void edit_level_action();
		void copy_terminals_action();
		void paste_terminals_action();

		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}