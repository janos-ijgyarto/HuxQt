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

		bool is_modified() const { return m_modified; }
		void set_modified() { m_modified = true; }
		void clear_modified();

		void reset();
	private:
		QString m_name;
		QString m_merge_folder_path;
		std::vector<Level> m_levels;
		int m_terminal_id_counter = 0;
		bool m_modified = false;

		friend class ScenarioManager;
	};
}