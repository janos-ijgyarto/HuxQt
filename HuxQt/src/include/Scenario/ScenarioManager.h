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

		static QString convert_ao_to_html(const QString& ao_text);
		static QString export_hux_formatted_text(const QString& formatted_text);
	private:
		ScenarioManager(AppCore& core);

		void export_level_script(QFile& level_file, const Level& level);
		void export_terminal_script(const Terminal& terminal, QString& level_script_text);

		AppCore& m_core;

		class ScriptParser;
		friend AppCore;
	};
}