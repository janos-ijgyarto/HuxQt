#pragma once
#include "Scenario/Terminal.h"

namespace HuxApp
{
	struct DisplayData
	{
		Terminal::ScreenType m_screen_type = Terminal::ScreenType::NONE;
		QString m_text;
		int m_resource_id = 0;

		bool operator==(const DisplayData& other) const
		{
			return ((m_screen_type == other.m_screen_type) && (m_text == other.m_text) && (m_resource_id == other.m_resource_id));
		}

		bool operator!=(const DisplayData& other) const
		{
			return ((m_screen_type != other.m_screen_type) || (m_text != other.m_text) || (m_resource_id != other.m_resource_id));
		}
	};
}