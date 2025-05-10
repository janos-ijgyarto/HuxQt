#pragma once
#include <QDialog>
#include <ui_EditTextColorDialog.h>

namespace HuxApp
{
	class AppCore;

	class EditTextColorDialog : public QDialog
	{
	public:
		EditTextColorDialog(AppCore& core);
	private:
		void init_ui();
		void connect_signals();

		QListWidgetItem* get_selected_item() const;

		void color_item_selected(QListWidgetItem* item);

		void update_color_select_button(const QColor& color);

		void select_color();
		void ok_clicked();

		AppCore& m_core;
		Ui::EditTextColorDialog m_ui;
	};
}