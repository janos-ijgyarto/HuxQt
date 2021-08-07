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

		bool save_scenario(const QString& file_path, const Scenario& scenario); // Save to Hux-specific file
		bool export_scenario(const QString& split_folder_path, const Scenario& scenario); // Export to split folder
		bool load_scenario(const QString& file_path, Scenario& scenario); // Load from Hux-specific file
		bool import_scenario(const QString& split_folder_path, Scenario& scenario); // Import from split folder

		QString print_level_script(const Level& level) const;

		const Terminal* get_screen_clipboard() const;
		void set_screen_clipboard(const Terminal& terminal_data);
		void clear_screen_clipboard();

		static QString convert_ao_to_html(const QString& ao_text);
		static QString export_hux_formatted_text(const QString& formatted_text);
	private:
		ScenarioManager(AppCore& core);

		void export_level_script(QFile& level_file, const Level& level) const;
		void export_terminal_script(const Terminal& terminal, int terminal_index, QString& level_script_text) const;

		AppCore& m_core;

		struct Internal;
		std::unique_ptr<Internal> m_internal;

		class ScriptParser;
		class ScriptJSONSerializer;
		friend AppCore;
	};
}