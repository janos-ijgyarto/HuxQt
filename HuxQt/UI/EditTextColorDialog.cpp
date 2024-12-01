#include <HuxQt/UI/EditTextColorDialog.h>

#include <HuxQt/AppCore.h>
#include <HuxQt/UI/HuxQt.h>

#include <HuxQt/Scenario/ScenarioManager.h>

#include <HuxQt/Utils/Color.h>

#include <QColorDialog>

namespace HuxApp
{
	EditTextColorDialog::EditTextColorDialog(AppCore& core)
		: QDialog(core.get_main_window())
		, m_core(core)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		m_ui.setupUi(this);

		init_ui();
		connect_signals();
	}

	void EditTextColorDialog::init_ui()
	{
		// Disable buttons initially
		m_ui.select_color_button->setEnabled(false);
		m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);

		const ScenarioManager& scenario_manager = m_core.get_scenario_manager();
		const ScenarioManager::TextColorArray& text_colors = scenario_manager.get_text_colors();

		int current_color_index = 0;
		for (const QColor& current_text_color : text_colors)
		{
			QListWidgetItem* current_color_item = new QListWidgetItem(m_ui.color_list);
			current_color_item->setText(QStringLiteral("C%1").arg(current_color_index));
			current_color_item->setData(Qt::BackgroundRole, current_text_color);
			current_color_item->setData(Qt::ForegroundRole, Utils::get_contrast_color(current_text_color));
			++current_color_index;
		}
	}

	void EditTextColorDialog::connect_signals()
	{
		connect(m_ui.color_list, &QListWidget::itemClicked, this, &EditTextColorDialog::color_item_selected);

		connect(m_ui.select_color_button, &QPushButton::clicked, this, &EditTextColorDialog::select_color);
			
		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &EditTextColorDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	}

	QListWidgetItem* EditTextColorDialog::get_selected_item() const
	{
		QList<QListWidgetItem*> selected_items = m_ui.color_list->selectedItems();
		if (!selected_items.isEmpty())
		{
			return selected_items.front();
		}

		return nullptr;
	}

	void EditTextColorDialog::color_item_selected(QListWidgetItem* item)
	{
		// Enable the color select button
		m_ui.select_color_button->setEnabled(true);

		QListWidgetItem* selected_item = get_selected_item();
		const QColor selected_color = selected_item->data(Qt::BackgroundRole).value<QColor>();

		// Update button
		update_color_select_button(selected_color);

		// Update the label
		m_ui.selected_color_index->setText(QStringLiteral("C%1").arg(m_ui.color_list->row(selected_item)));
	}

	void EditTextColorDialog::update_color_select_button(const QColor& color)
	{
		// Create colored icon for the color select button
		QPixmap button_icon_pixmap(100, 100);
		button_icon_pixmap.fill(color);

		QIcon button_icon(button_icon_pixmap);
		m_ui.select_color_button->setIcon(button_icon);
	}

	void EditTextColorDialog::select_color()
	{
		// Open a color dialog
		QListWidgetItem* selected_item = get_selected_item();
		const QColor previous_color = selected_item->data(Qt::BackgroundRole).value<QColor>();

		const QColor new_color = QColorDialog::getColor(previous_color, this, QStringLiteral("Select Override Color for C%1").arg(m_ui.color_list->row(selected_item)));
		if (!new_color.isValid() || (new_color == previous_color))
		{
			return;
		}

		// Update the colors, enable OK button since we made a change
		m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
		selected_item->setData(Qt::BackgroundRole, new_color);
		selected_item->setData(Qt::ForegroundRole, Utils::get_contrast_color(new_color));

		update_color_select_button(new_color);
	}

	void EditTextColorDialog::ok_clicked()
	{
		// Update the scenario manager with the new colors
		ScenarioManager::TextColorArray new_text_colors;

		for (int current_row = 0; current_row < m_ui.color_list->count(); ++current_row)
		{
			const QListWidgetItem* current_item = m_ui.color_list->item(current_row);
			new_text_colors[current_row] = current_item->data(Qt::BackgroundRole).value<QColor>();

		}

		ScenarioManager& scenario_manager = m_core.get_scenario_manager();
		scenario_manager.set_text_colors(new_text_colors);
		accept();
	}
}