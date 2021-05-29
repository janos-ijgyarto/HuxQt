#include <stdafx.h>
#include <Scenario/Level.h>

namespace HuxApp
{
	int Level::find_terminal(int terminal_id) const
	{
		auto terminal_it = std::find_if(m_terminals.begin(), m_terminals.end(),
			[terminal_id](const Terminal& current_terminal)
			{
				return (current_terminal.get_id() == terminal_id);
			}
		);

		if (terminal_it != m_terminals.end())
		{
			return std::distance(m_terminals.begin(), terminal_it);
		}

		return -1;
	}
}