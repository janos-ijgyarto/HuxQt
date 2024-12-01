#include <HuxQt/UI/BrowsePictDialog.h>

#include <HuxQt/AppCore.h>
#include <HuxQt/UI/HuxQt.h>
#include <HuxQt/UI/DisplaySystem.h>

namespace HuxApp
{
	BrowsePictDialog::BrowsePictDialog(AppCore& core, QWidget* parent)
		: QDialog(parent)
	{
		m_ui.setupUi(this);
		setAttribute(Qt::WA_DeleteOnClose, true);

		// Use list widget to display previews of picts
		m_ui.pict_list_widget->setViewMode(QListWidget::IconMode);
		m_ui.pict_list_widget->setIconSize(QSize(200, 200));
		m_ui.pict_list_widget->setResizeMode(QListWidget::Adjust);

		QMapIterator<int, QString> pict_cache_it(core.get_display_system().get_pict_cache());
		while (pict_cache_it.hasNext())
		{
			pict_cache_it.next();
			QListWidgetItem* pict_item = new QListWidgetItem(QIcon(pict_cache_it.value()), QString::number(pict_cache_it.key()));
			pict_item->setData(Qt::UserRole, pict_cache_it.key());
			m_ui.pict_list_widget->addItem(pict_item);
		}

		connect_signals();
	}

	void BrowsePictDialog::connect_signals()
	{
		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &BrowsePictDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

		connect(m_ui.pict_list_widget, &QListWidget::itemClicked, this, &BrowsePictDialog::pict_clicked);
		connect(m_ui.pict_list_widget, &QListWidget::itemDoubleClicked, this, &BrowsePictDialog::pict_double_clicked);
	}

	void BrowsePictDialog::ok_clicked()
	{
		if (m_selected_pict)
		{
			emit(pict_selected(m_selected_pict->data(Qt::UserRole).toInt()));
		}

		QDialog::accept();
	}

	void BrowsePictDialog::pict_clicked(QListWidgetItem* item)
	{
		m_selected_pict = item;
	}

	void BrowsePictDialog::pict_double_clicked(QListWidgetItem* item)
	{
		// Interpret as selecting a PICT and exiting
		ok_clicked();
	}
}