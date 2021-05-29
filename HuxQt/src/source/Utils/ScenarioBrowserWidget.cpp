#include <stdafx.h>
#include <Utils/ScenarioBrowserWidget.h>

namespace HuxApp
{
	namespace Utils
	{
		ScenarioBrowserWidget::ScenarioBrowserWidget(QWidget* parent)
			: QListWidget(parent)
		{
		}

		void ScenarioBrowserWidget::dropEvent(QDropEvent* event)
		{
			emit(items_dropped());
			QListWidget::dropEvent(event);
		}
	}
}