#pragma once
#include <QDialog>
#include <ui_ExportScenarioDialog.h>

namespace HuxApp
{
	class ExportScenarioDialog : public QDialog
	{
		Q_OBJECT
	public:
		ExportScenarioDialog(QWidget* parent, const QStringList& level_output_list);
	private:
		void init_ui(const QStringList& level_output_list);
		void connect_signals();

		void ok_clicked();
		void level_item_double_clicked(QListWidgetItem* level_item);
		void level_script_tab_closed(int index);

		Ui::ExportScenarioDialog m_ui;
	};
}