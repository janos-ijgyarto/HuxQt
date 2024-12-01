#include <HuxQt/UI/ExportScenarioDialog.h>

#include <HuxQt/Scenario/Scenario.h>

#include <QFileDialog>

namespace HuxApp
{
	ExportScenarioDialog::ExportScenarioDialog(QWidget* parent, const QString& init_path, const QStringList& level_output_list)
		: QDialog(parent)
		, m_init_path(init_path)
	{
		m_ui.setupUi(this);
		setAttribute(Qt::WA_DeleteOnClose, true);

		init_ui(level_output_list);
		connect_signals();
	}

	void ExportScenarioDialog::init_ui(const QStringList& level_output_list)
	{
		for (int level_offset = 0; level_offset < level_output_list.length(); level_offset += 2)
		{
			QListWidgetItem* new_level_item = new QListWidgetItem(m_ui.scenario_level_list);
			new_level_item->setText(level_output_list.at(level_offset));
			new_level_item->setData(Qt::UserRole, level_output_list.at(level_offset + 1));
		}

		// Initialize the splitter
		const int window_width = width();
		m_ui.main_splitter->setSizes({ window_width / 2, window_width / 2 });
	}

	void ExportScenarioDialog::connect_signals()
	{
		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &ExportScenarioDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &ExportScenarioDialog::reject);

		connect(m_ui.scenario_level_list, &QListWidget::itemClicked, this, &ExportScenarioDialog::level_item_clicked);
	}

	void ExportScenarioDialog::ok_clicked()
	{
		QString export_dir_path = QFileDialog::getExistingDirectory(this, tr("Select Export Folder"), m_init_path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (!export_dir_path.isEmpty())
		{
			// Notify the main window of the selected export path
			emit(export_path_selected(export_dir_path));
			QDialog::accept();
		}
	}

	void ExportScenarioDialog::level_item_clicked(QListWidgetItem* level_item)
	{
		// Display the level name
		m_ui.script_name_label->setText(level_item->text());

		// Display the script contents
		m_ui.script_preview->setPlainText(level_item->data(Qt::UserRole).toString());
	}
}