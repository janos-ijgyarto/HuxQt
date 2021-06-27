#pragma once
#include <QDialog>
#include <ui_ExportScenarioDialog.h>

namespace HuxApp
{
	class ExportScenarioDialog : public QDialog
	{
		Q_OBJECT
	public:
		ExportScenarioDialog(QWidget* parent, const QString& init_path, const QStringList& level_output_list);

	signals:
		void export_path_selected(const QString& path);
	private:
		void init_ui(const QStringList& level_output_list);
		void connect_signals();

		void ok_clicked();
		void level_item_double_clicked(QListWidgetItem* level_item);
		void level_script_tab_closed(int index);

		Ui::ExportScenarioDialog m_ui;
		QString m_init_path;
	};
}