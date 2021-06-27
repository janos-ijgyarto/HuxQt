#include "stdafx.h"
#include "Scenario/ScenarioManager.h"

#include "AppCore.h"
#include "UI/HuxQt.h"

#include "Scenario/Scenario.h"

#include "Utils/Utilities.h"

#include <QJsonDocument>

namespace HuxApp
{
	namespace
	{
		constexpr const char* TERMINAL_SCRIPT_CODEC = "UTF-8";

		enum class ScriptKeywords
		{
			TERMINAL,
			END_TERMINAL,
			UNFINISHED,
			FINISHED,
			END,
			LOGON,
			INFORMATION,
			PICT,
			CHECKPOINT,
			LOGOFF,
			INTERLEVEL_TELEPORT,
			INTRALEVEL_TELEPORT,
			TAG,
			STATIC,
			KEYWORD_COUNT,
			NON_KEYWORD = KEYWORD_COUNT
		};

		constexpr const char* SCRIPT_KEYWORD_STRINGS[Utils::to_integral(ScriptKeywords::KEYWORD_COUNT)] = {
			"#TERMINAL",
			"#ENDTERMINAL",
			"#UNFINISHED",
			"#FINISHED",
			"#END",
			"#LOGON",
			"#INFORMATION",
			"#PICT",
			"#CHECKPOINT",
			"#LOGOFF",
			"#INTERLEVEL TELEPORT",
			"#INTRALEVEL TELEPORT",
			"#TAG",
			"#STATIC"
		};

		constexpr const char* SCREEN_ALIGNMENT_KEYWORDS[Utils::to_integral(Terminal::ScreenAlignment::ALIGNMENT_COUNT)] = {
			"LEFT",
			"CENTER",
			"RIGHT"
		};

		constexpr const char* get_script_keyword(ScriptKeywords keyword) { return SCRIPT_KEYWORD_STRINGS[Utils::to_integral(keyword)]; }
		constexpr const char* get_screen_keyword(Terminal::ScreenType type)
		{
			switch (type)
			{
			case Terminal::ScreenType::LOGON:
				return get_script_keyword(ScriptKeywords::LOGON);
			case Terminal::ScreenType::INFORMATION:
				return get_script_keyword(ScriptKeywords::INFORMATION);
			case Terminal::ScreenType::PICT:
				return get_script_keyword(ScriptKeywords::PICT);
			case Terminal::ScreenType::CHECKPOINT:
				return get_script_keyword(ScriptKeywords::CHECKPOINT);
			case Terminal::ScreenType::LOGOFF:
				return get_script_keyword(ScriptKeywords::LOGOFF);
			case Terminal::ScreenType::TAG:
				return get_script_keyword(ScriptKeywords::TAG);
			case Terminal::ScreenType::STATIC:
				return get_script_keyword(ScriptKeywords::STATIC);
			}

			return "";
		}

		enum class TextFormattingTags
		{
			BOLD_START,
			BOLD_END,
			ITALICIZED_START,
			ITALICIZED_END,
			UNDERLINED_START,
			UNDERLINED_END,
			TEXT_COLOR,
			TAG_COUNT
		};

		/*$Cn changes colors of text to color n, where n can be any number between 0 and 7.

		$B  starts  bold  text.
		$b  ends  bold  text.
		$I  begins  italicized  text.
		$i  ends  italics.
		$U  starts  underlined  text.
		$u  ends  underline*/

		// Formatting tags in Aleph One 
		constexpr const char* AO_FORMATTING_TAG_ARRAY[Utils::to_integral(TextFormattingTags::TAG_COUNT)] = {
			"$B",
			"$b",
			"$I",
			"$i",
			"$U",
			"$u",
			"\\$C(?<color>[0-7])"
		};

		// Regexp for finding color tags
		const QRegularExpression AO_COLOR_REGEXP = QRegularExpression(AO_FORMATTING_TAG_ARRAY[Utils::to_integral(TextFormattingTags::TEXT_COLOR)]);

		// HTML tags in Hux output
		constexpr const char* HTML_TAG_ARRAY[Utils::to_integral(TextFormattingTags::TAG_COUNT)] = {
			"<b>",
			"</b>",
			"<i>",
			"</i>",
			"<u>",
			"</u>",
			"<span style=\"color:%1\">"
		};

		// Regexp for finding color span tags in Hux output
		const QRegularExpression HTML_COLOR_REGEXP = QRegularExpression("<span style=\"color:(?<color>\\w+)\">");

		constexpr const char* HTML_COLORS_ARRAY[Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT)] = {
			"Lime",
			"White",
			"Red",
			"DarkGreen",
			"LightBlue",
			"Yellow",
			"DarkRed",
			"DarkBlue"
		};

		bool validate_scenario_folder(const QString& path, QStringList& level_dir_list)
		{
			// Assume we were given a path to a split map folder (e.g via Atque)
			QDirIterator dir_it(path, QDir::Dirs | QDir::NoDotAndDotDot);

			bool has_resource_folder = false;
			while (dir_it.hasNext())
			{
				dir_it.next();
				const QString current_dir_name = dir_it.fileInfo().baseName();
				if (current_dir_name != "Resources")
				{
					// Assume any folder that isn't "Resources" is a level folder
					level_dir_list << current_dir_name;
				}
				else
				{
					has_resource_folder = true;
				}
			}

			return has_resource_folder;
		}

		int find_ao_color_index(const QString& html_color)
		{
			for (int color_index = 0; color_index < Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT); ++color_index)
			{
				if (html_color == HTML_COLORS_ARRAY[color_index])
				{
					return color_index;
				}
			}

			// If all else fails, return default
			return 0;
		}

		Terminal::ScreenAlignment get_screen_alignment(const QString& alignment_text)
		{
			if (alignment_text == "CENTER")
			{
				return Terminal::ScreenAlignment::CENTER;
			}
			else if (alignment_text == "RIGHT")
			{
				return Terminal::ScreenAlignment::RIGHT;
			}
			else
			{
				return Terminal::ScreenAlignment::LEFT;
			}
		}

		bool screen_has_text(const Terminal::Screen& screen)
		{
			switch (screen.m_type)
			{
			case Terminal::ScreenType::NONE:
			case Terminal::ScreenType::TAG:
			case Terminal::ScreenType::STATIC:
				return false;
			case Terminal::ScreenType::PICT:
			{
				if (screen.m_alignment == Terminal::ScreenAlignment::CENTER)
				{
					return false;
				}
			}
			}

			return true;
		}

		void export_terminal_screens(const std::vector<Terminal::Screen>& terminal_screens, QString& terminal_script_text)
		{
			for (const Terminal::Screen& current_screen : terminal_screens)
			{
				// First add the screen header
				switch (current_screen.m_type)
				{
				case Terminal::ScreenType::NONE:
					break;
				case Terminal::ScreenType::INFORMATION:
					terminal_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::INFORMATION));
					break;
				case Terminal::ScreenType::PICT:
				{
					if (current_screen.m_alignment == Terminal::ScreenAlignment::LEFT)
					{
						terminal_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::PICT), QString::number(current_screen.m_resource_id));
					}
					else
					{
						const int alignment_index = Utils::to_integral(current_screen.m_alignment);
						terminal_script_text += QStringLiteral("%1 %2 %3\n").arg(get_script_keyword(ScriptKeywords::PICT), QString::number(current_screen.m_resource_id), SCREEN_ALIGNMENT_KEYWORDS[alignment_index]);
					}
					break;
				}
				default:
					// Add the keyword and the resource ID
					terminal_script_text += QStringLiteral("%1 %2\n").arg(get_screen_keyword(current_screen.m_type), QString::number(current_screen.m_resource_id));
					break;
				}

				// Add any text we might have
				if (screen_has_text(current_screen) && !current_screen.m_script.isEmpty())
				{
					terminal_script_text += current_screen.m_script;
					terminal_script_text += "\n";
				}
			}
		}

		void export_terminal_teleport(const Terminal::Teleport& teleport_info, QString& terminal_script_text)
		{
			switch (teleport_info.m_type)
			{
			case Terminal::TeleportType::INTERLEVEL:
				terminal_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::INTERLEVEL_TELEPORT), QString::number(teleport_info.m_index));
				break;
			case Terminal::TeleportType::INTRALEVEL:
				terminal_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::INTRALEVEL_TELEPORT), QString::number(teleport_info.m_index));
				break;
			}
		}		
	}

	class ScenarioManager::ScriptParser
	{
	public:
		ScriptParser(Level& level) : m_level(level) {}

		bool parse_level(const QFileInfo& level_file_info)
		{
			QFile level_file(level_file_info.absoluteFilePath());
			if (level_file.open(QIODevice::ReadOnly))
			{
				m_file_stream.setDevice(&level_file);
				m_file_stream.setCodec(TERMINAL_SCRIPT_CODEC);
				while (!m_file_stream.atEnd())
				{
					switch (m_state)
					{
					case ParserState::NONE:
						parse_non_term_lines();
					break;
					case ParserState::TERMINAL:
						parse_terminal();
						break;
					default:
						// We entered an invalid state
						return false;
					}
				}

				// Make sure what we parsed was valid, and that we parsed anything to begin with
				if (m_state == ParserState::NONE)
				{
					return m_valid_file;
				}
			}

			return false;
		}
	private:
		enum class ParserState
		{
			NONE,
			TERMINAL,
			SCREENS,
			PAGE,
			INVALID
		};

		void read_line()
		{
			m_current_line = m_file_stream.readLine();
			parse_current_line_type();
		}

		void append_comment()
		{
			// TODO: if comment starts with ; then separate(?)
			m_comment_buffer += m_comment_buffer.isEmpty() ? m_current_line : (QStringLiteral("\n") + m_current_line);
		}

		void add_script_line(QString& script_text)
		{
			script_text += m_current_line + "\n";
		}

		void parse_current_line_type()
		{
			if (!m_current_line.isEmpty())
			{
				for (int keyword_index = 0; keyword_index < Utils::to_integral(ScriptKeywords::KEYWORD_COUNT); ++keyword_index)
				{
					if (m_current_line.indexOf(SCRIPT_KEYWORD_STRINGS[keyword_index]) == 0)
					{
						m_current_line_type = static_cast<ScriptKeywords>(keyword_index);
						return;
					}
				}
			}

			m_current_line_type = ScriptKeywords::NON_KEYWORD;
		}

		void parse_non_term_lines()
		{
			// Assume all lines are comments before we get to a recognized keyword
			while (!m_file_stream.atEnd())
			{
				read_line();

				if (m_current_line_type != ScriptKeywords::NON_KEYWORD)
				{
					// Change state according to the keyword we encountered
					switch (m_current_line_type)
					{
					case ScriptKeywords::TERMINAL:
					{
						// Terminal section, start parsing
						m_valid_file = true;
						m_state = ParserState::TERMINAL;
						return;
					}
					default:
						// Invalid keyword, interpret as comment(?)
						append_comment();
						break;
					}
				}
				else
				{
					// Interpret as comment
					append_comment();
				}
			}
		}

		void validate_end_terminal(const Terminal& terminal, int terminal_id)
		{
			// Validate that the ID matches the one we were parsing
			QStringList terminal_header_strings = m_current_line.split(' ');
			const int end_id = terminal_header_strings.at(1).toInt();

			if (end_id == terminal_id)
			{
				// We are done with this terminal
				m_state = ParserState::NONE;
				m_level.m_terminals.push_back(terminal);
			}
			else
			{
				// Invalid state
				m_state = ParserState::INVALID;
			}
		}

		void parse_terminal()
		{
			Terminal new_terminal;

			// Store comments buffered up to this point
			new_terminal.m_comments = m_comment_buffer;
			m_comment_buffer.clear();

			// Start with the first received line (has the terminal ID)
			QStringList terminal_header_strings = m_current_line.split(' ');
			const int terminal_id = terminal_header_strings.at(1).toInt();

			// Parse the terminal
			while (!m_file_stream.atEnd())
			{
				read_line();
				if (m_current_line_type != ScriptKeywords::NON_KEYWORD)
				{
					switch (m_current_line_type)
					{
					case ScriptKeywords::UNFINISHED:
					case ScriptKeywords::FINISHED:
					{
						m_state = ParserState::SCREENS;
						parse_terminal_screens(new_terminal, m_current_line_type == ScriptKeywords::UNFINISHED);

						if (m_state == ParserState::INVALID)
						{
							return;
						}
						else if (m_current_line_type == ScriptKeywords::END_TERMINAL)
						{
							validate_end_terminal(new_terminal, terminal_id);
							return;
						}
					}
						break;
					case ScriptKeywords::END_TERMINAL:
					{
						validate_end_terminal(new_terminal, terminal_id);
						return;
					}
					default:
						// Invalid keyword, interpret as comment(?)
						append_comment();
						break;
					}
				}
				else
				{
					// Interpret as comment
					append_comment();
				}
			}

			// We are meant to exit using a keyword, otherwise the terminal script is invalid
			m_state = ParserState::INVALID;
		}

		void parse_terminal_screens(Terminal& terminal, bool unfinished)
		{
			// Parse pages
			std::vector<Terminal::Screen>& screen_vec = unfinished ? terminal.m_unfinished_screens : terminal.m_finished_screens;
			Terminal::Teleport& teleport_info = unfinished ? terminal.m_unfinished_teleport : terminal.m_finished_teleport;

			Terminal::Screen current_screen;

			while (!m_file_stream.atEnd())
			{
				read_line();
				if (m_current_line_type != ScriptKeywords::NON_KEYWORD)
				{
					if (current_screen.m_type != Terminal::ScreenType::NONE)
					{
						// Was parsing info for a valid screen, and we hit a new keyword, so we can now store this screen
						current_screen.m_script.chop(1); // Parsing will add a redundant endline at the very end, remove it
						current_screen.m_display_text = convert_ao_to_html(current_screen.m_script);
						screen_vec.push_back(current_screen);
						current_screen.reset();
						current_screen.m_comments = m_comment_buffer; // All comments up to this point will be interpreted as for this screen
						m_comment_buffer.clear();
					}

					switch (m_current_line_type)
					{
					case ScriptKeywords::LOGON:
					case ScriptKeywords::LOGOFF:
					{
						// Logon screen, get the PICT ID
						QStringList line_split = m_current_line.split(' ');
						current_screen.m_resource_id = line_split.at(1).toInt();
						current_screen.m_type = (m_current_line_type == ScriptKeywords::LOGON) ? Terminal::ScreenType::LOGON : Terminal::ScreenType::LOGOFF;
					}
					break;
					case ScriptKeywords::INFORMATION:
					{
						// Information (i.e text-only) screen
						current_screen.m_type = Terminal::ScreenType::INFORMATION;
					}
					break;
					case ScriptKeywords::PICT:
					{
						// Picture screen, get the PICT ID
						QStringList line_split = m_current_line.split(' ');
						current_screen.m_resource_id = line_split.at(1).toInt();
						current_screen.m_type = Terminal::ScreenType::PICT;

						if (line_split.length() > 2)
						{
							// PICT also has alignment info
							current_screen.m_alignment = get_screen_alignment(line_split.at(2));
						}
					}
					break;
					case ScriptKeywords::CHECKPOINT:
					{
						// Checkpoint screen, get the polygon ID
						QStringList line_split = m_current_line.split(' ');
						current_screen.m_resource_id = line_split.at(1).toInt();
						current_screen.m_type = Terminal::ScreenType::CHECKPOINT;
					}
					break;
					case ScriptKeywords::END:
					case ScriptKeywords::END_TERMINAL:
					{
						// We finished parsing this section
						m_state = ParserState::TERMINAL;
						return;
					}
					case ScriptKeywords::INTERLEVEL_TELEPORT:
					case ScriptKeywords::INTRALEVEL_TELEPORT:
					{
						// Get the teleport index from the third string
						QStringList line_split = m_current_line.split(' ');
						teleport_info.m_index = line_split.at(2).toInt();
						teleport_info.m_type = (m_current_line_type == ScriptKeywords::INTERLEVEL_TELEPORT) ? Terminal::TeleportType::INTERLEVEL : Terminal::TeleportType::INTRALEVEL;
					}
					break;
					case ScriptKeywords::TAG:
					{
						// Tag screen, get the tag index
						QStringList line_split = m_current_line.split(' ');
						current_screen.m_resource_id = line_split.at(1).toInt();
						current_screen.m_type = Terminal::ScreenType::TAG;
					}
					break;
					case ScriptKeywords::STATIC:
					{
						// Static screen, get the duration
						QStringList line_split = m_current_line.split(' ');
						current_screen.m_resource_id = line_split.at(1).toInt();
						current_screen.m_type = Terminal::ScreenType::STATIC;
					}
					break;
					default:
					{
						// If we are inside a page, interpret as more text, otherwise interpret as comments
						if (current_screen.m_type != Terminal::ScreenType::NONE)
						{
							add_script_line(current_screen.m_script);
							current_screen.m_script += current_screen.m_script.isEmpty() ? m_current_line : (QStringLiteral("\n") + m_current_line);
						}
						else
						{
							append_comment();
						}
					}
					break;
					}
				}
				else
				{
					// If we are inside a page, interpret as more text, otherwise interpret as comments
					if (current_screen.m_type != Terminal::ScreenType::NONE)
					{
						add_script_line(current_screen.m_script);
					}
					else
					{
						append_comment();
					}
				}
			}

			// We are meant to exit via an "END", otherwise we are in an invalid state
			m_state = ParserState::INVALID;
		}

		Level& m_level;
		ParserState m_state = ParserState::NONE;
		bool m_valid_file = false;

		QTextStream m_file_stream;
		QString m_current_line;
		ScriptKeywords m_current_line_type = ScriptKeywords::KEYWORD_COUNT;
		QString m_comment_buffer;
	};

	class ScenarioManager::ScriptJSONSerializer
	{
	public:
		static void serialize_teleport_info(const Terminal::Teleport& teleport, QJsonObject& teleport_json)
		{
			teleport_json["TYPE"] = Utils::to_integral(teleport.m_type);
			teleport_json["INDEX"] = teleport.m_index;
		}

		static void serialize_screen_json(const Terminal::Screen& screen, QJsonObject& screen_json)
		{
			screen_json["TYPE"] = Utils::to_integral(screen.m_type);
			screen_json["ALIGNMENT"] = Utils::to_integral(screen.m_alignment);
			screen_json["RESOURCE_ID"] = screen.m_resource_id;
			screen_json["SCRIPT"] = screen.m_script;
		}

		static void serialize_terminal_json(const Terminal& terminal, QJsonObject& terminal_json)
		{
			// Unfinished data
			{
				QJsonArray unfinished_screens_array;
				for (const Terminal::Screen& current_screen : terminal.m_unfinished_screens)
				{
					QJsonObject current_screen_json;
					serialize_screen_json(current_screen, current_screen_json);
					unfinished_screens_array.append(current_screen_json);
				}
				terminal_json["UNFINISHED_SCREENS"] = unfinished_screens_array;

				QJsonObject unfinished_teleport_json;
				serialize_teleport_info(terminal.m_unfinished_teleport, unfinished_teleport_json);
				terminal_json["UNFINISHED_TELEPORT"] = unfinished_teleport_json;
			}

			// Finished data
			{
				QJsonArray finished_screens_array;
				for (const Terminal::Screen& current_screen : terminal.m_finished_screens)
				{
					QJsonObject current_screen_json;
					serialize_screen_json(current_screen, current_screen_json);
					finished_screens_array.append(current_screen_json);
				}
				terminal_json["FINISHED_SCREENS"] = finished_screens_array;

				QJsonObject finished_teleport_json;
				serialize_teleport_info(terminal.m_finished_teleport, finished_teleport_json);
				terminal_json["FINISHED_TELEPORT"] = finished_teleport_json;
			}
		}

		static void serialize_level_json(const Level& level, QJsonObject& level_json)
		{
			level_json["NAME"] = level.get_name();
			level_json["DIR_NAME"] = level.get_level_dir_name();
			level_json["SCRIPT_NAME"] = level.get_level_script_name();

			QJsonArray terminal_array;
			for (const Terminal& current_terminal : level.get_terminals())
			{
				QJsonObject current_terminal_json;
				serialize_terminal_json(current_terminal, current_terminal_json);
				terminal_array.append(current_terminal_json);
			}

			level_json["TERMINALS"] = terminal_array;
		}

		static void deserialize_teleport_info(const QJsonObject& teleport_json, Terminal::Teleport& teleport)
		{
			teleport.m_type = Utils::to_enum<Terminal::TeleportType>(teleport_json["TYPE"].toInt());
			teleport.m_index = teleport_json["INDEX"].toInt();
		}

		static void deserialize_screen_json(const QJsonObject& screen_json, Terminal::Screen& screen)
		{
			screen.m_type = Utils::to_enum<Terminal::ScreenType>(screen_json["TYPE"].toInt());
			screen.m_alignment = Utils::to_enum<Terminal::ScreenAlignment>(screen_json["ALIGNMENT"].toInt());
			screen.m_resource_id = screen_json["RESOURCE_ID"].toInt();
			screen.m_script = screen_json["SCRIPT"].toString();
		}

		static void deserialize_terminal_json(const QJsonObject& terminal_json, Terminal& terminal)
		{
			// Unfinished data
			{
				const QJsonArray unfinished_screens_array = terminal_json["UNFINISHED_SCREENS"].toArray();
				for (const QJsonValue& current_screen_json_value : unfinished_screens_array)
				{
					Terminal::Screen& current_screen = terminal.m_unfinished_screens.emplace_back();
					const QJsonObject current_screen_json = current_screen_json_value.toObject();
					deserialize_screen_json(current_screen_json, current_screen);
				}

				const QJsonObject unfinished_teleport_json = terminal_json["UNFINISHED_TELEPORT"].toObject();
				deserialize_teleport_info(unfinished_teleport_json, terminal.m_unfinished_teleport);
			}

			// Finished data
			{
				const QJsonArray finished_screens_array = terminal_json["FINISHED_SCREENS"].toArray();
				for (const QJsonValue& current_screen_json_value : finished_screens_array)
				{
					Terminal::Screen& current_screen = terminal.m_finished_screens.emplace_back();
					const QJsonObject current_screen_json = current_screen_json_value.toObject();
					deserialize_screen_json(current_screen_json, current_screen);
				}

				const QJsonObject finished_teleport_json = terminal_json["FINISHED_TELEPORT"].toObject();
				deserialize_teleport_info(finished_teleport_json, terminal.m_finished_teleport);
			}
		}

		static void deserialize_level_json(const QJsonObject& level_json, Scenario& scenario, Level& level)
		{
			level.m_name = level_json["NAME"].toString();
			level.m_level_dir_name = level_json["DIR_NAME"].toString();
			level.m_level_script_name = level_json["SCRIPT_NAME"].toString();

			const QJsonArray terminal_array = level_json["TERMINALS"].toArray();
			for (const QJsonValue& current_terminal_value : terminal_array)
			{
				Terminal& current_terminal = level.m_terminals.emplace_back();
				const QJsonObject current_terminal_json = current_terminal_value.toObject();
				deserialize_terminal_json(current_terminal_json, current_terminal);

				// Terminal has been parsed, assign ID
				current_terminal.m_id = scenario.m_terminal_id_counter++;
			}
		}
	};

	ScenarioManager::ScenarioManager(AppCore& core) 
		: m_core(core)
	{
	}

	ScenarioManager::~ScenarioManager() = default;

	void ScenarioManager::export_level_script(QFile& level_file, const Level& level) const
	{
		QString level_script_text = print_level_script(level);
		QTextStream text_stream(&level_file);
		text_stream.setCodec(TERMINAL_SCRIPT_CODEC);
		text_stream << level_script_text;
	}

	void ScenarioManager::export_terminal_script(const Terminal& terminal, int terminal_index, QString& level_script_text) const
	{
		// Start with a comment tag (apparently needed for formatting?)
		level_script_text += ";\n";
		
		// Add the terminal header
		const QString terminal_id_string = QString::number(terminal_index);
		level_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::TERMINAL), terminal_id_string);

		if (!terminal.m_unfinished_screens.empty() || (terminal.m_unfinished_teleport.m_type != Terminal::TeleportType::NONE))
		{
			// Add the UNFINISHED header
			level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::UNFINISHED));

			// Add screens and teleport info
			export_terminal_screens(terminal.m_unfinished_screens, level_script_text);
			if (terminal.m_unfinished_teleport.m_type != Terminal::TeleportType::NONE)
			{
				export_terminal_teleport(terminal.m_unfinished_teleport, level_script_text);
			}

			// Add the end header
			level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::END));
		}

		if (!terminal.m_finished_screens.empty() || (terminal.m_finished_teleport.m_type != Terminal::TeleportType::NONE))
		{
			// Add the FINISHED header
			level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::FINISHED));

			// Add screens and teleport info
			export_terminal_screens(terminal.m_finished_screens, level_script_text);
			if (terminal.m_finished_teleport.m_type != Terminal::TeleportType::NONE)
			{
				export_terminal_teleport(terminal.m_finished_teleport, level_script_text);
			}

			// Add the end header
			level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::END));
		}

		// Add the terminal end header
		level_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::END_TERMINAL), terminal_id_string);
	}

	bool ScenarioManager::save_scenario(const QString& file_path, const Scenario& scenario)
	{
		// Prepare the JSON file
		QFile scenario_file(file_path);
		if (!scenario_file.open(QIODevice::WriteOnly))
		{
			QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Unable to open file \"%1\"!").arg(file_path));
			return false;
		}

		// Create JSON root object
		QJsonObject scenario_root_json;

		// Serialize the levels
		QJsonArray levels_json_array;
		for (const Level& current_level : scenario.m_levels)
		{
			QJsonObject current_level_json;
			ScriptJSONSerializer::serialize_level_json(current_level, current_level_json);
			levels_json_array.append(current_level_json);
		}

		// Store the level array
		scenario_root_json["LEVELS"] = levels_json_array;

		// Write to file
		if (scenario_file.write(QJsonDocument(scenario_root_json).toJson()) == -1)
		{
			QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Error writing to file \"%1\"!").arg(file_path));
			return false;
		}

		return true;
	}

	bool ScenarioManager::export_scenario(const QString& split_folder_path, const Scenario& scenario)
	{
		for (const Level& current_level : scenario.m_levels)
		{
			// Open a file for each level, create folder if necessary
			const QString level_dir_path = split_folder_path + "/" + current_level.get_level_dir_name();
			QDir level_dir;
			if (!level_dir.exists(level_dir_path))
			{
				if (!level_dir.mkpath(level_dir_path))
				{
					QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Unable to save to directory \"%1\"!").arg(level_dir_path));
					return false;
				}
			}

			const QString level_file_path = level_dir_path + "/" + current_level.get_level_script_name();
			QFile level_file(level_file_path);
			if (!level_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
			{
				QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Unable to save to file \"%1\"!").arg(level_file_path));
				level_file.close();
				return false;
			}
			export_level_script(level_file, current_level);
		}

		return true;
	}

	bool ScenarioManager::load_scenario(const QString& file_path, Scenario& scenario)
	{
		// First make sure the file exists and is the correct type
		QFileInfo file_info(file_path);
		if (!file_info.isFile())
		{
			return false;
		}

		// Open the file
		QFile scenario_file(file_path);
		if (!scenario_file.open(QIODevice::ReadOnly))
		{
			QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Unable to open file \"%1\"!").arg(file_path));
			return false;
		}

		// Load the JSON data and parse it
		QByteArray scenario_file_data = scenario_file.readAll();

		QJsonParseError parse_error;
		QJsonDocument scenario_json_document = QJsonDocument::fromJson(scenario_file_data, &parse_error);
		if (parse_error.error != QJsonParseError::NoError)
		{
			QMessageBox::warning(m_core.get_main_window(), "Scenario File Error", QStringLiteral("Invalid scenario file! Error: \"%1\"!").arg(parse_error.errorString()));
			return false;
		}

		// Parse successful, reset the scenario contents
		scenario.reset();

		// Load the scenario from the root object
		const QJsonObject scenario_root_json = scenario_json_document.object();
		const QJsonArray levels_json_array = scenario_root_json["LEVELS"].toArray();
		for (const QJsonValue& current_level_value : levels_json_array)
		{
			Level& current_level = scenario.m_levels.emplace_back();
			const QJsonObject current_level_json = current_level_value.toObject();
			ScriptJSONSerializer::deserialize_level_json(current_level_json, scenario, current_level);
		}

		QDir file_dir = file_info.absoluteDir();
		if (!file_dir.cd("Resources"))
		{
			QMessageBox::warning(m_core.get_main_window(), "Scenario Warning", QStringLiteral("No Resources folder present for this scenario file! Images will not be available for terminal previews!"));
		}
		return true;
	}

	bool ScenarioManager::import_scenario(const QString& split_folder_path, Scenario& scenario)
	{
		QStringList level_dir_list;
		if (!validate_scenario_folder(split_folder_path, level_dir_list))
		{
			QMessageBox::warning(m_core.get_main_window(), "Scenario Load Error", "The selected folder does not contain a valid Aleph One scenario!");
			return false;
		}

		// Reset the scenario contents
		scenario.reset();

		scenario.m_name = QDir(split_folder_path).dirName();

		for (const QString& level_dir_name : level_dir_list)
		{
			const QDir current_level_dir(split_folder_path + "/" + level_dir_name);
			const QFileInfoList file_info_list = current_level_dir.entryInfoList(QDir::Files);

			for (const QFileInfo& current_file : file_info_list)
			{
				if (current_file.completeSuffix() == "term.txt")
				{
					// We found a terminal script, parse it
					Level parsed_level;
					parsed_level.m_name = current_file.baseName();
					parsed_level.m_level_dir_name = level_dir_name;
					parsed_level.m_level_script_name = current_file.fileName();
					ScriptParser parser(parsed_level);
					if (parser.parse_level(current_file))
					{
						// Level successfully parsed, assign terminal IDs (used by the UI)
						for (Terminal& current_terminal : parsed_level.m_terminals)
						{
							current_terminal.m_id = scenario.m_terminal_id_counter++;
						}
						scenario.m_levels.push_back(parsed_level);
					}
					break;
				}
			}
		}

		return true;
	}

	bool ScenarioManager::import_level_terminals(Scenario& scenario, Level& destination_level, const QString& level_script_path)
	{
		const QFileInfo level_script_file(level_script_path);
		Level temp_level;
		ScriptParser parser(temp_level);
		if (parser.parse_level(level_script_file))
		{
			// Level successfully parsed, assign terminal IDs (used by the UI)
			for (Terminal& current_terminal : temp_level.m_terminals)
			{
				current_terminal.m_id = scenario.m_terminal_id_counter++;
				current_terminal.set_modified(true);
				destination_level.m_terminals.push_back(current_terminal);
			}
			destination_level.set_modified();
			return true;
		}
		return false;
	}

	QString ScenarioManager::print_level_script(const Level& level) const
	{
		QString level_script_text;
		int terminal_index = 0;
		for (const Terminal& current_terminal : level.m_terminals)
		{
			export_terminal_script(current_terminal, terminal_index, level_script_text);
			++terminal_index;
		}
		return level_script_text;
	}

	void ScenarioManager::set_screen_clipboard(const Terminal& terminal_data)
	{
		if (!m_screen_clipboard)
		{
			m_screen_clipboard = std::make_unique<Terminal>();
		}
		*m_screen_clipboard = terminal_data;
	}

	void ScenarioManager::clear_screen_clipboard()
	{
		if (m_screen_clipboard)
		{
			m_screen_clipboard.reset();
		}
	}

	QString ScenarioManager::convert_ao_to_html(const QString& ao_text)
	{
		// Parse the Aleph One formatting tags, turning them into HTML tags that Qt can display
		QString parsed_text = ao_text.toHtmlEscaped(); // First make sure we have no unintended HTML tags
		bool color_changed = false;

		// Iterate through each possible tag, converting each match
		for (int current_tag_index = 0; current_tag_index < Utils::to_integral(TextFormattingTags::TAG_COUNT); ++current_tag_index)
		{
			switch (static_cast<TextFormattingTags>(current_tag_index))
			{
			case TextFormattingTags::TEXT_COLOR:
			{
				// Color change is a special case, as we need to extract the color index
				QRegularExpressionMatch color_match = AO_COLOR_REGEXP.match(parsed_text);
				while (color_match.hasMatch())
				{
					// Found a match
					QString color_tag;
					if (color_changed)
					{
						// Add an end tag so we drop the previous color
						color_tag = QStringLiteral("</span>");
					}

					// Get the color index and generate the corresponding HTML tag
					const int new_color_index = color_match.captured("color").toInt();
					color_tag += QString(HTML_TAG_ARRAY[Utils::to_integral(TextFormattingTags::TEXT_COLOR)]).arg(HTML_COLORS_ARRAY[new_color_index]);
					color_changed = true;

					// Replace the tag and find the next match (if any)
					parsed_text.replace(color_match.capturedStart(), color_match.capturedLength(), color_tag);
					color_match = AO_COLOR_REGEXP.match(parsed_text);
				}
			}
			default:
			{
				// Simply replace the tags with the corresponding HTML tags
				parsed_text.replace(AO_FORMATTING_TAG_ARRAY[current_tag_index], HTML_TAG_ARRAY[current_tag_index]);
			}
			}
		}

		// If the color was changed at some point, we need to close off the last span tag
		if (color_changed)
		{
			parsed_text += QStringLiteral("</span>");
		}
		// Wrap the text in paragraph tags
		parsed_text = QStringLiteral("<p style=\"white-space: pre-wrap\">%1</p>").arg(parsed_text);
		return parsed_text;
	}

	QString ScenarioManager::export_hux_formatted_text(const QString& formatted_text)
	{
		// For exporting, we need to invert the parsing and create AO tags from HTML
		QString exported_ao_text = formatted_text;

		// First clean up the paragraph tags
		exported_ao_text.remove(QStringLiteral("<p style=\"white-space: pre-wrap\">"));
		exported_ao_text.remove(QStringLiteral("</p>"));

		for (int current_tag_index = 0; current_tag_index < Utils::to_integral(TextFormattingTags::TAG_COUNT); ++current_tag_index)
		{
			switch (static_cast<TextFormattingTags>(current_tag_index))
			{
			case TextFormattingTags::TEXT_COLOR:
			{
				// Color change is a special case, we need to find the span tags and convert the attribute name to a color index	
				QRegularExpressionMatch color_match = HTML_COLOR_REGEXP.match(exported_ao_text);
				while (color_match.hasMatch())
				{
					// Found a match, get the color index and generate the corresponding AO tag
					const QString color_name = color_match.captured("color");
					const int color_index = find_ao_color_index(color_name);

					// Replace the tag and find the next match (if any)
					exported_ao_text.replace(color_match.capturedStart(), color_match.capturedLength(), QStringLiteral("$C%1").arg(color_index));
					color_match = HTML_COLOR_REGEXP.match(exported_ao_text);
				}

				// Remove all the end tags for span sections
				exported_ao_text.remove(QStringLiteral("</span>"));
			}
			default:
			{
				// For all others, simply convert the HTML tags to their AO equivalent
				exported_ao_text.replace(HTML_TAG_ARRAY[current_tag_index], AO_FORMATTING_TAG_ARRAY[current_tag_index]);
			}
			}

		}

		// Finally, we need to revert the HTML-escaped content
		exported_ao_text.replace("&amp", "&");
		exported_ao_text.replace("&lt", "<");
		exported_ao_text.replace("&gt", ">");

		return exported_ao_text;
	}

	QStringList ScenarioManager::gather_additional_levels(const Scenario& scenario, const QString& split_folder_path)
	{
		// Gather a set of the directory names that were already added
		QSet<QString> existing_levels;
		for (const Level& current_level : scenario.m_levels)
		{
			existing_levels << current_level.get_level_dir_name();
		}

		// Create list of directories in the scenario folder which aren't in the current list of levels
		QDirIterator dir_it(split_folder_path, QDir::Dirs | QDir::NoDotAndDotDot);
		QStringList non_scripted_levels;
		while (dir_it.hasNext())
		{
			dir_it.next();
			const QString current_dir_name = dir_it.fileInfo().baseName();
			// Make sure the folder isn't the Resources folder, nor is it an already added level
			if ((current_dir_name != "Resources") && !existing_levels.contains(current_dir_name))
			{
				non_scripted_levels << current_dir_name;
			}
		}

		// Finally check the level directory contents to find out which ones have a valid level
		QStringList available_levels;
		for (const QString& current_level_name : non_scripted_levels)
		{
			const QDir current_level_dir(split_folder_path + "/" + current_level_name);
			const QFileInfoList file_info_list = current_level_dir.entryInfoList(QDir::Files);

			for (const QFileInfo& current_file : file_info_list)
			{
				const QString suffix = current_file.completeSuffix();
				if (suffix == "sceA")
				{
					// We found a level file, so we can add it to the list
					available_levels << current_file.baseName();
					available_levels << current_level_name;
				}
			}
		}

		return available_levels;
	}

	bool ScenarioManager::add_scenario_level(Scenario& scenario, const QString& level_dir_name)
	{
		// FIXME: implement this!
		//const QDir new_level_dir(scenario.get_merge_folder_path() + "/" + level_dir_name);
		//const QFileInfoList file_info_list = new_level_dir.entryInfoList(QDir::Files);
		//
		//for (const QFileInfo& current_file : file_info_list)
		//{
		//	if (current_file.completeSuffix() == "sceA")
		//	{
		//		// Use the level file name as the name for our terminal script name
		//		Level new_level;
		//		new_level.m_name = current_file.baseName();
		//		new_level.m_level_dir_name = level_dir_name;
		//		new_level.m_level_script_name = current_file.baseName() + "term.txt";

		//		// Add level to the scenario
		//		scenario.m_levels.push_back(new_level);
		//		return true;
		//	}
		//}

		// Something went wrong
		return false;
	}

	bool ScenarioManager::delete_scenario_level_script(Scenario& scenario, size_t level_index)
	{
		// FIXME: implement this correctly!
		/*const Level& selected_level = scenario.m_levels[level_index];
		const QString level_script_path = scenario.get_merge_folder_path() + "/" + selected_level.m_level_dir_name + "/" + selected_level.m_level_script_name;

		if (QFile::exists(level_script_path))
		{
			return QFile::remove(level_script_path);
		}*/
		return true;
	}

	void ScenarioManager::remove_scenario_level(Scenario& scenario, size_t level_index)
	{
		scenario.m_levels.erase(scenario.m_levels.begin() + level_index);
	}

	void ScenarioManager::add_level_terminal(Scenario& scenario, Level& level)
	{
		level.m_terminals.emplace_back(); 
		level.m_terminals.back().m_id = scenario.m_terminal_id_counter++;
		level.set_modified();
	}

	void ScenarioManager::add_level_terminal(Scenario& scenario, Level& level, const Terminal& terminal, size_t index)
	{
		const size_t terminal_index = std::clamp(index, size_t(0), level.m_terminals.size());
		auto new_terminal_it = level.m_terminals.insert(level.m_terminals.begin() + terminal_index, terminal);
		new_terminal_it->m_id = scenario.m_terminal_id_counter++;
		new_terminal_it->set_modified(true);
		level.set_modified();
	}

	void ScenarioManager::add_level_terminals(Scenario& scenario, Level& level, const std::vector<Terminal>& terminals, size_t index)
	{
		const size_t terminal_index = std::clamp(index, size_t(0), level.m_terminals.size());

		auto new_terminals_begin = level.m_terminals.insert(level.m_terminals.begin() + terminal_index, terminals.begin(), terminals.end());
		auto new_terminals_end = new_terminals_begin + terminals.size();
		for (auto new_terminal_it = new_terminals_begin; new_terminal_it != new_terminals_end; ++new_terminal_it)
		{
			new_terminal_it->m_id = scenario.m_terminal_id_counter++;
			new_terminal_it->set_modified(true);
		}
		level.set_modified();
	}

	void ScenarioManager::reorder_level_terminals(Level& level, const std::vector<int>& terminal_ids, const std::unordered_set<int>& moved_ids)
	{
		assert(terminal_ids.size() == level.m_terminals.size());
		const std::vector<Terminal> level_terminals = level.m_terminals;
		auto terminal_it = level.m_terminals.begin();
		for (int current_id : terminal_ids)
		{
			auto moving_terminal_it = std::find_if(level_terminals.begin(), level_terminals.end(),
				[current_id](const Terminal& current_terminal)
				{
					return (current_terminal.get_id() == current_id);
				}
			);
			assert(moving_terminal_it != level_terminals.end());

			*terminal_it = *moving_terminal_it;
			// Indicate which terminals were moved
			if (moved_ids.find(current_id) != moved_ids.end())
			{
				terminal_it->set_modified(true);
			}
			++terminal_it;
		}
		level.set_modified();
	}

	void ScenarioManager::remove_level_terminals(Level& level, const std::vector<size_t>& terminal_indices)
	{
		const std::vector<Terminal> prev_terminals = level.m_terminals;
		level.m_terminals.clear();

		size_t current_index = 0;
		for (const Terminal& current_terminal : prev_terminals)
		{
			if (std::find(terminal_indices.begin(), terminal_indices.end(), current_index) == terminal_indices.end())
			{
				// Index not among those to be removed
				level.m_terminals.push_back(current_terminal);
			}
			++current_index;
		}
		level.set_modified();
	}
}