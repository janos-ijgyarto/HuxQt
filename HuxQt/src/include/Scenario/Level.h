#pragma once
#include "Scenario/Terminal.h"

namespace HuxApp
{
	class Level
	{
	public:
		const QString& get_name() const { return m_name; }
		void set_name(const QString& name) { m_name = name; }

		const QString& get_level_dir_name() const { return m_level_dir_name; }
		void set_level_dir_name(const QString& level_dir_name) { m_level_dir_name = level_dir_name; }

		const QString& get_level_script_name() const { return m_level_script_name; }
		
		bool is_modified() const { return m_modified; }
		void set_modified() { m_modified = true; }
		void clear_modified()
		{
			for (Terminal& terminal : m_terminals)
			{
				terminal.set_modified(false);
			}
			m_modified = false;
		}

		Terminal& get_terminal(int index) { return m_terminals[index]; }
		std::vector<Terminal>& get_terminals() { return m_terminals; }

		const Terminal& get_terminal(int index) const { return m_terminals[index]; }
		const std::vector<Terminal>& get_terminals() const { return m_terminals; }

		int find_terminal(int terminal_id) const;
	private:
		QString m_name;
		QString m_level_dir_name;
		QString m_level_script_name;
		bool m_modified = false;

		std::vector<Terminal> m_terminals;
		QString m_comments;

		friend class ScenarioManager;
	};
}