#pragma once
#include <QListWidget>

namespace HuxApp
{
	namespace UI
	{
		class ScenarioBrowserWidget : public QListWidget
		{
			Q_OBJECT
		public:
			ScenarioBrowserWidget(QWidget* parent = nullptr);

		signals:
			void items_dropped();

		protected:
			void dropEvent(QDropEvent* event) override;
		};
	}
}