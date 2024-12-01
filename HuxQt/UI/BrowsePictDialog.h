#pragma once
#include <QDialog>
#include <ui_BrowsePictDialog.h>

namespace HuxApp
{
	class AppCore;
	class HuxQt;

	class BrowsePictDialog : public QDialog
	{
		Q_OBJECT
	public:
		BrowsePictDialog(AppCore& core, QWidget* parent);

	signals:
		void pict_selected(int pict_id);
	private:
		void connect_signals();

		void ok_clicked();

		void pict_clicked(QListWidgetItem* item);
		void pict_double_clicked(QListWidgetItem* item);

		Ui::BrowsePictDialog m_ui;
		QListWidgetItem* m_selected_pict = nullptr;
	};
}