#include "stdafx.h"
#include "UI/AddLevelDialog.h"

#include "Scenario/Scenario.h"
#include "UI/HuxQt.h"

namespace HuxApp
{
	AddLevelDialog::AddLevelDialog(HuxQt* main_window, const QStringList& available_levels)
		: QDialog(main_window)
		, m_main_window(main_window)
		, m_selected_level(nullptr)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		m_ui.setupUi(this);

		for (int current_level_offset = 0; current_level_offset < available_levels.length(); current_level_offset += 2)
		{
			// The first string is the level name (taken from the .sceA filename), the second is the folder name
			QListWidgetItem* list_item = new QListWidgetItem(available_levels.at(current_level_offset), m_ui.level_list_widget);
			list_item->setData(Qt::UserRole, available_levels.at(current_level_offset + 1));
		}

		// Initially disable the controls, as the user must select a level first
		m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);
		connect_signals();
	}

	void AddLevelDialog::connect_signals()
	{
		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &AddLevelDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
		connect(m_ui.level_list_widget, &QListWidget::itemClicked, this, &AddLevelDialog::level_selected);
	}

	void AddLevelDialog::ok_clicked()
	{
		if (m_main_window->add_level(m_selected_level->text(), m_selected_level->data(Qt::UserRole).toString()))
		{
			QDialog::accept();
		}
	}

	void AddLevelDialog::level_selected(QListWidgetItem* item)
	{
		if (!m_selected_level)
		{
			// Once we have a level selected, we can enable the controls
			m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
		}
		m_selected_level = item;
	}
}