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

		static QString convert_ao_to_html(const QString& ao_text);
		static QString export_hux_formatted_text(const QString& formatted_text);

		static QStringList gather_additional_levels(const Scenario& scenario);
		static bool add_scenario_level(Scenario& scenario, const QString& level_dir_name);
		static bool delete_scenario_level_script(Scenario& scenario, size_t level_index);
		static void remove_scenario_level(Scenario& scenario, size_t level_index);

		static void add_level_terminal(Scenario& scenario, Level& level);
		static void add_level_terminal(Scenario& scenario, Level& level, const Terminal& terminal, size_t index);
		static void move_level_terminal(Level& level, size_t terminal_index, size_t new_index);
		static void remove_level_terminal(Level& level, size_t terminal_index);

		static void add_terminal_screen(Terminal& terminal, size_t index, bool unfinished);
		static void move_terminal_screen(Terminal& terminal, size_t screen_index, size_t new_index, bool unfinished);
		static void remove_terminal_screen(Terminal& terminal, size_t screen_index, bool unfinished);
		static void clear_terminal_screen_group(Terminal& terminal, bool unfinished);
	private:
		ScenarioManager(AppCore& core);

		void export_level_script(QFile& level_file, const Level& level) const;
		void export_terminal_script(const Terminal& terminal, int terminal_index, QString& level_script_text) const;

		AppCore& m_core;

		class ScriptParser;
		friend AppCore;
	};
}