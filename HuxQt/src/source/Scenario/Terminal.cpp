#include "stdafx.h"
#include "Scenario/Terminal.h"

namespace HuxApp
{
	void Terminal::Screen::reset()
	{
		m_type = ScreenType::NONE;
		m_alignment = ScreenAlignment::LEFT;
		m_resource_id = -1;
		m_display_text.clear();
		m_script.clear();
		m_comments.clear();
	}

	QString Terminal::get_screen_string(const Screen& screen_data)
	{
		switch (screen_data.m_type)
		{
		case Terminal::ScreenType::NONE:
			return QStringLiteral("NONE");
		case Terminal::ScreenType::LOGON:
			return QStringLiteral("LOGON %1").arg(screen_data.m_resource_id);
		case Terminal::ScreenType::INFORMATION:
			return QStringLiteral("INFORMATION");
		case Terminal::ScreenType::PICT:
			return QStringLiteral("PICT %1").arg(screen_data.m_resource_id);
		case Terminal::ScreenType::CHECKPOINT:
			return QStringLiteral("CHECKPOINT %1").arg(screen_data.m_resource_id);
		case Terminal::ScreenType::LOGOFF:
			return QStringLiteral("LOGOFF %1").arg(screen_data.m_resource_id);
		case Terminal::ScreenType::TAG:
			return QStringLiteral("TAG %1").arg(screen_data.m_resource_id);
		case Terminal::ScreenType::STATIC:
			return QStringLiteral("STATIC %1").arg(screen_data.m_resource_id);
		}

		return "";
	}
}