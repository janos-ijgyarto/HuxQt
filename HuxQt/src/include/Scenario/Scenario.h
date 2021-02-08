#pragma once
#include "Scenario/Level.h"

namespace HuxApp
{
	// a.k.a the Merge Folder, contains all the levels and all the terminals within
	class Scenario
	{
	public:
		const QString& get_name() const { return m_name; }
		const QString& get_merge_folder_path() const { return m_merge_folder_path; }

		Level& get_level(int index) { return m_levels[index]; }
		std::vector<Level>& get_levels() { return m_levels; }

		const Level& get_level(int index) const { return m_levels[index]; }
		const std::vector<Level>& get_levels() const { return m_levels; }

		void reset();
	private:
		QString m_name;
		QString m_merge_folder_path;
		std::vector<Level> m_levels;

		friend class ScenarioManager;
	};
}