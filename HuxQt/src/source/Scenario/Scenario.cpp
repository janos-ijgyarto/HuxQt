#include "stdafx.h"
#include "Scenario/Scenario.h"

namespace HuxApp
{
	void Scenario::clear_modified()
	{
		for (Level& current_level : m_levels)
		{
			current_level.clear_modified();
		}
		m_modified = false;
	}

	void Scenario::reset()
	{
		m_name = "";
		m_levels.clear();
		m_terminal_id_counter = 0; 
		m_modified = false;
	}
}