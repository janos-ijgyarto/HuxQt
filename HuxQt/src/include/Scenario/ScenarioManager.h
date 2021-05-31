#pragma once

namespace HuxApp
{
	class AppCore;
	class Scenario;
	class Level;
	class Terminal;

	class ScenarioManager
	{
	public:
		enum class TextFont
		{
			BOLD,
			ITALIC,
			UNDERLINE,
			FONT_COUNT
		};

		enum class TextColor
		{
			LIGHT_GREEN,
			WHITE,
			RED,
			DARK_GREEN,
			LIGHT_BLUE,
			YELLOW,
			DARK_RED,
			DARK_BLUE,
			COLOR_COUNT
		};

		~ScenarioManager();

		bool save_scenario(const QString& path, const Scenario& scenario, bool modified_only = true);
		bool load_scenario(const QString& path, Scenario& scenario);

		QString print_level_script(const Level& level) const;

		const Terminal* get_screen_clipboard() const { return m_screen_clipboard.get(); }
		void set_screen_clipboard(const Terminal& terminal_data);
		void clear_screen_clipboard();

		static QString convert_ao_to_html(const QString& ao_text);
		static QString export_hux_formatted_text(const QString& formatted_text);

		static QStringList gather_additional_levels(const Scenario& scenario);
		static bool add_scenario_level(Scenario& scenario, const QString& level_dir_name);
		static bool delete_scenario_level_script(Scenario& scenario, size_t level_index);
		static void remove_scenario_level(Scenario& scenario, size_t level_index);

		static void add_level_terminal(Scenario& scenario, Level& level);
		static void add_level_terminal(Scenario& scenario, Level& level, const Terminal& terminal, size_t index);
		static void add_level_terminals(Scenario& scenario, Level& level, const std::vector<Terminal>& terminals, size_t index);
		static void reorder_level_terminals(Level& level, const std::vector<int>& terminal_ids, const std::unordered_set<int>& moved_ids);
		static void remove_level_terminals(Level& level, const std::vector<size_t>& terminal_indices);
	private:
		ScenarioManager(AppCore& core);

		void export_level_script(QFile& level_file, const Level& level) const;
		void export_terminal_script(const Terminal& terminal, int terminal_index, QString& level_script_text) const;

		AppCore& m_core;
		std::unique_ptr<Terminal> m_screen_clipboard;

		class ScriptParser;
		friend AppCore;
	};
}