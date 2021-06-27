#include "stdafx.h"
#include "UI/ExportScenarioDialog.h"

#include "Scenario/Scenario.h"

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
		m_ui.script_preview_tabs->setTabsClosable(true);

		for (int level_offset = 0; level_offset < level_output_list.length(); level_offset += 2)
		{
			QListWidgetItem* new_level_item = new QListWidgetItem(m_ui.scenario_level_list);
			new_level_item->setText(level_output_list.at(level_offset));
			new_level_item->setData(Qt::UserRole, level_output_list.at(level_offset + 1));
		}
	}

	void ExportScenarioDialog::connect_signals()
	{
		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &ExportScenarioDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &ExportScenarioDialog::reject);

		connect(m_ui.scenario_level_list, &QListWidget::itemDoubleClicked, this, &ExportScenarioDialog::level_item_double_clicked);

		connect(m_ui.script_preview_tabs, &QTabWidget::tabCloseRequested, this, &ExportScenarioDialog::level_script_tab_closed);
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

	void ExportScenarioDialog::level_item_double_clicked(QListWidgetItem* level_item)
	{
		for (int tab_index = 0; tab_index < m_ui.script_preview_tabs->count(); ++tab_index)
		{
			if (m_ui.script_preview_tabs->tabBar()->tabText(tab_index) == level_item->text())
			{
				// Tab is already opened, set as current
				m_ui.script_preview_tabs->setCurrentIndex(tab_index);
				return;
			}
		}

		// Create a new tab
		QPlainTextEdit* new_tab_text_edit = new QPlainTextEdit();
		new_tab_text_edit->setReadOnly(true);
		new_tab_text_edit->setPlainText(level_item->data(Qt::UserRole).toString());

		m_ui.script_preview_tabs->addTab(new_tab_text_edit, level_item->text());
	}

	void ExportScenarioDialog::level_script_tab_closed(int index)
	{
		QWidget* removed_tab = m_ui.script_preview_tabs->widget(index);
		m_ui.script_preview_tabs->removeTab(index);

		removed_tab->deleteLater();
	}
}