#include <HuxQt/UI/ScenarioBrowserWidget.h>

namespace HuxApp
{
	namespace UI
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