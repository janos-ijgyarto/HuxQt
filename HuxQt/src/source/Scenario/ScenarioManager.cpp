#include "stdafx.h"
#include "Scenario/ScenarioManager.h"

#include "AppCore.h"
#include "UI/HuxQt.h"

#include "Scenario/Scenario.h"

#include "Utils/Utilities.h"

namespace HuxApp
{
	namespace
	{
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
			"#INTRALEVEL TELEPORT"
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
				default:
					// Add the keyword and the resource ID
					terminal_script_text += QStringLiteral("%1 %2\n").arg(get_screen_keyword(current_screen.m_type), QString::number(current_screen.m_resource_id));
					break;
				}

				// Add any text we might have
				if ((current_screen.m_type != Terminal::ScreenType::NONE) && !current_screen.m_script.isEmpty())
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
				m_file_stream.setCodec("UTF-8");
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
				level_file.close();

				// Make sure what we parsed was valid
				if (m_state == ParserState::NONE)
				{
					return true;
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

		void validate_end_terminal(Terminal& terminal)
		{
			// Validate that the ID matches the one we were parsing
			QStringList terminal_header_strings = m_current_line.split(' ');
			const int end_id = terminal_header_strings.at(1).toInt();

			if (end_id == terminal.m_id)
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
			new_terminal.m_id = terminal_header_strings.at(1).toInt();

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
							validate_end_terminal(new_terminal);
							return;
						}
					}
						break;
					case ScriptKeywords::END_TERMINAL:
					{
						validate_end_terminal(new_terminal);
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
		QTextStream m_file_stream;
		QString m_current_line;
		ScriptKeywords m_current_line_type = ScriptKeywords::KEYWORD_COUNT;
		QString m_comment_buffer;
	};

	ScenarioManager::ScenarioManager(AppCore& core) 
		: m_core(core)
	{
	}

	ScenarioManager::~ScenarioManager() = default;

	void ScenarioManager::export_level_script(QFile& level_file, const Level& level)
	{
		QString level_script_text;
		for (const Terminal& current_terminal : level.m_terminals)
		{
			export_terminal_script(current_terminal, level_script_text);
		}

		QTextStream text_stream(&level_file);
		text_stream << level_script_text;
	}

	void ScenarioManager::export_terminal_script(const Terminal& terminal, QString& level_script_text)
	{
		// Start with a comment tag (apparently needed for formatting?)
		level_script_text += ";\n";
		
		// Add the terminal header
		level_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::TERMINAL), QString::number(terminal.get_id()));

		if (!terminal.m_unfinished_screens.empty())
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

		if (!terminal.m_finished_screens.empty())
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
		level_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::END_TERMINAL), QString::number(terminal.get_id())); 
	}

	bool ScenarioManager::save_scenario(const QString& path, const Scenario& scenario, bool modified_only)
	{
		for (const Level& current_level : scenario.m_levels)
		{
			// Check if we should only export modified levels
			if (current_level.m_modified || !modified_only)
			{
				// Open a file for each level, create folder if necessary
				const QString level_dir_path = path + "/" + current_level.get_level_dir_name();
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
				level_file.close();
			}
		}

		return true;
	}

	bool ScenarioManager::load_scenario(const QString& path, Scenario& scenario)
	{
		QStringList level_dir_list;
		if (!validate_scenario_folder(path, level_dir_list))
		{
			return false;
		}

		// Reset the scenario contents
		scenario.reset();

		scenario.m_name = QDir(path).dirName();
		scenario.m_merge_folder_path = path;

		for (const QString& level_dir_name : level_dir_list)
		{
			const QDir current_level_dir(path + "/" + level_dir_name);
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
						scenario.m_levels.push_back(parsed_level);
					}
					break;
				}
			}
		}

		return true;
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
}