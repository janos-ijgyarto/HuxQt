#pragma once
#include <QDialog>
#include <ui_AddLevelDialog.h>

namespace HuxApp
{
	class HuxQt;
	class Scenario;

	class AddLevelDialog : public QDialog
	{
		Q_OBJECT
	public:
		AddLevelDialog(HuxQt* main_window, const QStringList& available_levels);
	private:
		void connect_signals();

		void ok_clicked();
		void level_selected(QListWidgetItem* item);

		HuxQt* m_main_window;
		QListWidgetItem* m_selected_level;
		Ui::AddLevelDialog m_ui;
	};
}