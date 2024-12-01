#pragma once
#include <HuxQt/Scenario/Level.h>

namespace HuxApp
{
	// a.k.a the Merge Folder, contains all the levels and all the terminals within
	class Scenario
	{
	public:
		const QString& get_name() const { return m_name; }
		void set_name(const QString& name) { m_name = name; }

		Level& get_level(int index) { return m_levels[index]; }
		std::vector<Level>& get_levels() { return m_levels; }

		const Level& get_level(int index) const { return m_levels[index]; }
		const std::vector<Level>& get_levels() const { return m_levels; }
		void set_levels(const std::vector<Level>& levels) { m_levels = levels; }

		void reset();
	private:
		QString m_name;
		std::vector<Level> m_levels;

		friend class ScenarioManager;
	};
}