#pragma once
#include <ui_TeleportEditWidget.h>

#include <HuxQt/Scenario/Terminal.h>

namespace HuxApp
{
	class TeleportEditWidget : public QWidget
	{
		Q_OBJECT
	public:
		TeleportEditWidget(const QString& label, const Terminal::Teleport& teleport_info);

		Terminal::Teleport get_teleport_info() const;
		bool is_valid() const;
	signals:
		void teleport_info_modified();
	private:
		void connect_signals();

		Ui::TeleportEditWidget m_ui;
	};
}