#pragma once
#include <QWidget>

#include "Scenario/Terminal.h"

namespace HuxApp
{
	class AppCore;
	class ScreenEditTab;

	class ScreenEditorWindow : public QWidget
	{
		Q_OBJECT
	public:
		ScreenEditorWindow(AppCore& core);
		
		void open_screen(QTreeWidgetItem* screen_item, const Terminal::Screen& screen_data);
		void update_screen(QTreeWidgetItem* screen_item, const QString& screen_path);
		void close_screen(QTreeWidgetItem* screen_item);
		void remove_screen(QTreeWidgetItem* screen_item);

		bool check_unsaved();
		void clear_editor();
	private:
		void connect_signals();

		int find_screen_index(QTreeWidgetItem* screen_itex);
		void current_tab_changed(int index);
		void close_tab(int index);

		void screen_edited(QTreeWidgetItem* screen_item);
		bool prompt_save(ScreenEditTab* screen_tab);
		void save_screen(ScreenEditTab* screen_tab);
		void save_all();

		void update_preview();
		void update_edit_notification();
		void reset_edit_notification();

		AppCore& m_core;

		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}