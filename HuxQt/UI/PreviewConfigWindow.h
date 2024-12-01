#pragma once
#include <QWidget>
#include "ui_PreviewConfigWindow.h"

namespace HuxApp
{
	class AppCore;

	class PreviewConfigWindow : public QWidget
	{
		Q_OBJECT
	public:
		PreviewConfigWindow(AppCore& core);
		~PreviewConfigWindow();
	signals:
		void config_update();
		void window_closed();
	private:
		void config_changed();

		AppCore& m_core;

		Ui::PreviewConfigWindow m_ui;
	};
}