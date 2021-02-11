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
}