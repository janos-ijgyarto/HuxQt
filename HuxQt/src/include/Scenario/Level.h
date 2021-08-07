#pragma once
#include "Scenario/Terminal.h"

namespace HuxApp
{
	class Level
	{
	public:
		const QString& get_name() const { return m_name; }
		void set_name(const QString& name) { m_name = name; }

		const QString& get_dir_name() const { return m_dir_name; }
		void set_dir_name(const QString& dir_name) { m_dir_name = dir_name; }

		const QString& get_script_name() const { return m_script_name; }
		void set_script_name(const QString& script_name) { m_script_name = script_name; }

		Terminal& get_terminal(int index) { return m_terminals[index]; }
		std::vector<Terminal>& get_terminals() { return m_terminals; }

		const Terminal& get_terminal(int index) const { return m_terminals[index]; }
		const std::vector<Terminal>& get_terminals() const { return m_terminals; }

		void set_terminals(const std::vector<Terminal>& terminals) { m_terminals = terminals; }
	private:
		QString m_name;
		QString m_dir_name;
		QString m_script_name;

		std::vector<Terminal> m_terminals;
		QString m_comments;

		friend class ScenarioManager;
	};
}