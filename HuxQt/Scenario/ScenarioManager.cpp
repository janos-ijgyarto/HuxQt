#include <HuxQt/Scenario/ScenarioManager.h>

#include <HuxQt/AppCore.h>
#include <HuxQt/UI/HuxQt.h>

#include <HuxQt/Scenario/Scenario.h>
#include <HuxQt/Scenario/ScenarioBrowserModel.h>

#include <HuxQt/Utils/Utilities.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDirIterator>

#include <QMessageBox>

namespace HuxApp
{
	namespace
	{
		constexpr auto TERMINAL_SCRIPT_ENCODING = QStringConverter::Encoding::Utf8;
		constexpr const char* TERMINAL_SCRIPT_SUFFIX = ".term.txt";

		enum class ScriptKeywords
		{
			TERMINAL,
			END_TERMINAL,
			UNFINISHED,
			FINISHED,
			FAILED,
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
			"#FAILED",
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
			"$C[0-7]"
		};

		// Regexp for finding color tags
		const QRegularExpression AO_COLOR_REGEXP = QRegularExpression(R"(\$C(?<color>[0-7]))");
		const QRegularExpression AO_LINE_SEP_REGEXP = QRegularExpression(R"([&*\+\-<=>\/^|])");

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

		const QColor DEFAULT_QT_TEXT_COLORS_ARRAY[ScenarioManager::TEXT_COLOR_COUNT] = {
			Qt::green,
			Qt::white,
			Qt::red,
			Qt::darkGreen,
			Qt::blue,
			Qt::yellow,
			Qt::darkRed,
			Qt::darkBlue
		};

		constexpr const char* DEFAULT_HTML_COLORS_ARRAY[ScenarioManager::TEXT_COLOR_COUNT] = {
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

		Terminal::BranchType get_branch_type(ScriptKeywords keyword)
		{
			switch (keyword)
			{
			case ScriptKeywords::UNFINISHED:
				return Terminal::BranchType::UNFINISHED;
			case ScriptKeywords::FINISHED:
				return Terminal::BranchType::FINISHED;
			case ScriptKeywords::FAILED:
				return Terminal::BranchType::FAILED;
			}

			// Default to "unfinished" (we should never be here though)
			return Terminal::BranchType::UNFINISHED;
		}

		ScriptKeywords get_branch_type_keyword(Terminal::BranchType branch_type)
		{
			switch (branch_type)
			{
			case Terminal::BranchType::UNFINISHED:
				return ScriptKeywords::UNFINISHED;
			case Terminal::BranchType::FINISHED:
				return ScriptKeywords::FINISHED;
			case Terminal::BranchType::FAILED:
				return ScriptKeywords::FAILED;
			}

			// Default to "unfinished" (we should never be here though)
			return ScriptKeywords::UNFINISHED;
		}

		Terminal::BranchType get_branch_type(const QString& branch_type_text)
		{
			if (branch_type_text == "UNFINISHED")
			{
				return Terminal::BranchType::UNFINISHED;
			}
			else if (branch_type_text == "FINISHED")
			{
				return Terminal::BranchType::FINISHED;
			}
			else if(branch_type_text == "FAILED")
			{
				return Terminal::BranchType::FAILED;
			}
			else
			{
				// Default to "unfinished" (we should never be here though)
				return Terminal::BranchType::UNFINISHED;
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

		int get_screen_character_limit(Terminal::ScreenType screen_type)
		{
			switch (screen_type)
			{
			case Terminal::ScreenType::INFORMATION:
				return 70;
			case Terminal::ScreenType::PICT:
			case Terminal::ScreenType::CHECKPOINT:
				return 43;
			}

			return -1;
		}

		bool is_separator_character(QChar character)
		{
			const QRegularExpressionMatch match = AO_LINE_SEP_REGEXP.match(character);
			return match.hasMatch();
		}

		QString get_tag_regex_string()
		{
			QStringList tag_regex_list;
			for (int current_tag_index = 0; current_tag_index < Utils::to_integral(TextFormattingTags::TAG_COUNT); ++current_tag_index)
			{
				tag_regex_list << QStringLiteral("(\\%1)").arg(AO_FORMATTING_TAG_ARRAY[current_tag_index]);
			}
			return tag_regex_list.join('|');
		}

		const QRegularExpression& get_tag_regex()
		{
			// Create a RegEx that can match the AO formatting tags 
			static QRegularExpression tag_regex(get_tag_regex_string());
			return tag_regex;
		}

		void wrap_line(const QString& line, Terminal::ScreenType screen_type, QStringList& wrapped_lines)
		{
			const QRegularExpression& tag_regex = get_tag_regex();

			// Use a struct to store the tag contents
			struct FormattingTag
			{
				QString m_tag;
				int m_offset = -1;
			};

			std::vector<FormattingTag> formatting_tags;

			// Filter out the tags from the line
			QString filtered_line = line;
			QRegularExpressionMatch tag_match = tag_regex.match(filtered_line, 0, QRegularExpression::MatchType::PartialPreferFirstMatch);

			while (tag_match.hasMatch())
			{
				// Found a tag, cache it and remove from processed string
                formatting_tags.emplace_back();
                FormattingTag& found_tag = formatting_tags.back();

				const int last_captured_index = tag_match.lastCapturedIndex();
				found_tag.m_offset = tag_match.capturedStart(last_captured_index); // Make sure the offset takes into account that tags will be re-added before this one
				found_tag.m_tag = filtered_line.mid(found_tag.m_offset, tag_match.capturedLength(last_captured_index));
				filtered_line.remove(found_tag.m_offset, tag_match.capturedLength(last_captured_index));

				// Check if we have any more tags (starting from the offset we found, since we removed the tag itself)
				tag_match = tag_regex.match(filtered_line, found_tag.m_offset, QRegularExpression::MatchType::PartialPreferFirstMatch);
			}

			// Traverse the filtered line and wrap based on line length
			QStringList filtered_wrapped_lines;
			{
				int current_line_start = 0;
				int current_line_end = 0;
				const int character_limit = get_screen_character_limit(screen_type);
				while (true)
				{
					if (current_line_end == filtered_line.length())
					{
						// Add whatever else was left and end the loop
						filtered_wrapped_lines << filtered_line.mid(current_line_start);
						break;
					}

					const int current_length = current_line_end - current_line_start;
					if (current_length > character_limit)
					{
						// Reached limit, go back and see where we can wrap
						int wrap_index = (current_line_end - 1);
						for (; wrap_index > current_line_start; --wrap_index)
						{
							const QChar wrap_char = filtered_line[wrap_index];
							if (wrap_char == ' ')
							{
								// Eat space
								current_line_end = (wrap_index + 1);
								break;
							}
							else if (is_separator_character(wrap_char))
							{
								// Found a separator, use as break point
								current_line_end = wrap_index;
								break;
							}
						}
						assert(wrap_index >= 0);

						// Add the line (if we didn't find a place to wrap, we'll simply wrap at the character limit)
						filtered_wrapped_lines << filtered_line.mid(current_line_start, (current_line_end - current_line_start));
						current_line_start = current_line_end;
					}
					else
					{
						++current_line_end;
					}
				}
			}

			if (!formatting_tags.empty())
			{
				// Iterate over our strings and re-insert the tags
				int total_character_count = 0;
				auto formatting_tag_it = formatting_tags.begin();

				for (const QString& current_wrapped_line : filtered_wrapped_lines)
				{
					if (formatting_tag_it != formatting_tags.end())
					{
						QString finalized_wrapped_line;
						int current_char_offset = 0;
						while (current_char_offset < current_wrapped_line.length())
						{
							if (total_character_count == formatting_tag_it->m_offset)
							{
								finalized_wrapped_line += formatting_tag_it->m_tag;
								++formatting_tag_it;
								if (formatting_tag_it == formatting_tags.end())
								{
									// Finished adding all tags, just add the rest of the string
									finalized_wrapped_line += current_wrapped_line.mid(current_char_offset);
									break;
								}
								// Go again (may return in case multiple tags were at the same offset)
								continue;
							}
							// Add the current character and update the offsets
							finalized_wrapped_line += current_wrapped_line[current_char_offset];
							++total_character_count;
							++current_char_offset;
						}
						wrapped_lines << finalized_wrapped_line;
					}
					else
					{
						// No more tags, just add the rest of the lines
						wrapped_lines << current_wrapped_line;
					}
				}

				// If any tags were left, we add them to the end of the last line
				if (formatting_tag_it != formatting_tags.end())
				{
					if (wrapped_lines.isEmpty())
					{
						// Add an empty string, just in case this is a completely empty line with just formatting tags in it
						wrapped_lines << QString();
					}
					for (formatting_tag_it; formatting_tag_it != formatting_tags.end(); ++formatting_tag_it)
					{
						wrapped_lines.back() += formatting_tag_it->m_tag;
					}
				}
			}
			else
			{
				// No tags to process, add lines verbatim
				wrapped_lines << filtered_wrapped_lines;
			}
		}

		QString wrap_ao_text(const QString& ao_text, Terminal::ScreenType screen_type)
		{
			if (ao_text.isEmpty())
			{
				return QString();
			}
			else if ((screen_type != Terminal::ScreenType::INFORMATION)
				&& (screen_type != Terminal::ScreenType::PICT)
				&& (screen_type != Terminal::ScreenType::CHECKPOINT))
			{
				// Only perform wrapping for screen types where it's relevant
				return ao_text;
			}

			// First split the line along line breaks
			const QStringList lines = ao_text.split('\n');
			QStringList wrapped_lines;

			for (const QString& current_line : lines)
			{
				if (!current_line.isEmpty())
				{
					wrap_line(current_line, screen_type, wrapped_lines);
				}
				else
				{
					// Empty line, add it so we don't miss any line breaks
					wrapped_lines << current_line;
				}
			}

			return wrapped_lines.join('\n');
		}
	}

	class ScenarioManager::ScriptParser
	{
	public:
		ScriptParser(const ScenarioManager& scenario_manager, Level& level) 
			: m_scenario_manager(scenario_manager)
			, m_level(level) 
		{}

		bool parse_level(const QFileInfo& level_file_info)
		{
			QFile level_file(level_file_info.absoluteFilePath());
			if (level_file.open(QIODevice::ReadOnly))
			{
				m_file_stream.setDevice(&level_file);
				m_file_stream.setEncoding(TERMINAL_SCRIPT_ENCODING);
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
			if (terminal_header_strings.size() != 2)
			{
				// Invalid state
				m_state = ParserState::INVALID;
				return;
			}
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
					case ScriptKeywords::FAILED:
					{
						m_state = ParserState::SCREENS;
						parse_terminal_screens(new_terminal, get_branch_type(m_current_line_type));

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

		void parse_terminal_screens(Terminal& terminal, Terminal::BranchType branch)
		{
			// Parse pages
			Terminal::Branch& selected_branch = terminal.get_branch(branch);

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
						current_screen.m_display_text = m_scenario_manager.convert_ao_to_html(current_screen.m_script, Utils::to_integral(current_screen.m_type));
						selected_branch.m_screens.push_back(current_screen);
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
						selected_branch.m_teleport.m_index = line_split.at(2).toInt();
						selected_branch.m_teleport.m_type = (m_current_line_type == ScriptKeywords::INTERLEVEL_TELEPORT) ? Terminal::TeleportType::INTERLEVEL : Terminal::TeleportType::INTRALEVEL;
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

		const ScenarioManager& m_scenario_manager;

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

		static void serialize_terminal_branch_json(const Terminal::Branch& terminal_branch, Terminal::BranchType branch_type, QJsonObject& terminal_branch_json)
		{
			QJsonArray screens_array;
			for (const Terminal::Screen& current_screen : terminal_branch.m_screens)
			{
				QJsonObject current_screen_json;
				serialize_screen_json(current_screen, current_screen_json);
				screens_array.append(current_screen_json);
			}

			terminal_branch_json["SCREENS"] = screens_array;

			QJsonObject teleport_json;
			serialize_teleport_info(terminal_branch.m_teleport, teleport_json);
			terminal_branch_json["TELEPORT"] = teleport_json;
		}

		static void serialize_terminal_json(const Terminal& terminal, QJsonObject& terminal_json)
		{
			// Serialize name and all the valid branches
			terminal_json["NAME"] = terminal.m_name;

			QJsonObject branches_json;

			int current_branch_index = 0;
			for (const Terminal::Branch& current_branch : terminal.m_branches)
			{
				const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
				if (current_branch.is_valid())
				{
					QJsonObject current_branch_json;
					serialize_terminal_branch_json(current_branch, current_branch_type, current_branch_json);

					branches_json[Terminal::get_branch_type_name(current_branch_type)] = current_branch_json;
				}
				++current_branch_index;
			}

			terminal_json["BRANCHES"] = branches_json;
		}

		static void serialize_level_json(const Level& level, QJsonObject& level_json)
		{
			level_json["NAME"] = level.get_name();
			level_json["DIR_NAME"] = level.get_dir_name();
			level_json["SCRIPT_NAME"] = level.get_script_name();

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

		static void deserialize_screen_json(const ScenarioManager& scenario_manager, const QJsonObject& screen_json, Terminal::Screen& screen)
		{
			screen.m_type = Utils::to_enum<Terminal::ScreenType>(screen_json["TYPE"].toInt());
			screen.m_alignment = Utils::to_enum<Terminal::ScreenAlignment>(screen_json["ALIGNMENT"].toInt());
			screen.m_resource_id = screen_json["RESOURCE_ID"].toInt();
			screen.m_script = screen_json["SCRIPT"].toString();

			// Have to parse the script to get the display text
			screen.m_display_text = scenario_manager.convert_ao_to_html(screen.m_script, Utils::to_integral(screen.m_type));
		}

		static void deserialize_terminal_branch_json(const ScenarioManager& scenario_manager, const QJsonObject& terminal_branch_json, Terminal::Branch& terminal_branch)
		{
			const QJsonArray screens_array = terminal_branch_json["SCREENS"].toArray();
			for (const QJsonValue& current_screen_json_value : screens_array)
			{
                terminal_branch.m_screens.emplace_back();
                Terminal::Screen& current_screen = terminal_branch.m_screens.back();
				const QJsonObject current_screen_json = current_screen_json_value.toObject();
				deserialize_screen_json(scenario_manager, current_screen_json, current_screen);
			}

			const QJsonObject teleport_json = terminal_branch_json["TELEPORT"].toObject();
			deserialize_teleport_info(teleport_json, terminal_branch.m_teleport);
		}

		static void deserialize_terminal_json(const ScenarioManager& scenario_manager, const QJsonObject& terminal_json, Terminal& terminal)
		{
			terminal.m_name = terminal_json["NAME"].toString();

			const QJsonObject terminal_branches_json = terminal_json["BRANCHES"].toObject();

			for (const QString& current_key : terminal_branches_json.keys())
			{
				const QJsonObject current_branch_object = terminal_branches_json[current_key].toObject();
				Terminal::Branch& current_branch = terminal.get_branch(get_branch_type(current_key));

				deserialize_terminal_branch_json(scenario_manager, current_branch_object, current_branch);
			}
		}

		static void deserialize_level_json(const ScenarioManager& scenario_manager, const QJsonObject& level_json, Scenario& scenario, Level& level)
		{
			level.m_name = level_json["NAME"].toString();
			level.m_dir_name = level_json["DIR_NAME"].toString();
			level.m_script_name = level_json["SCRIPT_NAME"].toString();

			const QJsonArray terminal_array = level_json["TERMINALS"].toArray();
			for (const QJsonValue& current_terminal_value : terminal_array)
			{
                level.m_terminals.emplace_back();
                Terminal& current_terminal = level.m_terminals.back();
				const QJsonObject current_terminal_json = current_terminal_value.toObject();
				deserialize_terminal_json(scenario_manager, current_terminal_json, current_terminal);
			}
		}
	};

	struct ScenarioManager::Internal
	{
		std::unique_ptr<Terminal> m_screen_clipboard;

		std::unordered_map<int, Level> m_level_pool;
		std::unordered_map<int, Terminal> m_terminal_pool;

		int m_id_counter = 0;

		TextColorArray m_text_colors;

		Internal()
		{
			reset();
		}

		void reset()
		{
			int current_color_index = 0;
			for (QColor& current_color : m_text_colors)
			{
				current_color = DEFAULT_QT_TEXT_COLORS_ARRAY[current_color_index];
				++current_color_index;
			}
		}
	};

	ScenarioManager::~ScenarioManager() = default;

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
			const QString level_dir_path = split_folder_path + "/" + current_level.get_dir_name();
			QDir level_dir;
			if (!level_dir.exists(level_dir_path))
			{
				if (!level_dir.mkpath(level_dir_path))
				{
					QMessageBox::warning(m_core.get_main_window(), "File I/O Error", QStringLiteral("Unable to save to directory \"%1\"!").arg(level_dir_path));
					return false;
				}
			}

			const QString level_file_path = QStringLiteral("%1/%2.term.txt").arg(level_dir_path).arg(current_level.get_script_name());
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

		// Use the file name as the scenario name
		scenario.m_name = file_info.baseName();

		// Load the scenario from the root object
		const QJsonObject scenario_root_json = scenario_json_document.object();
		const QJsonArray levels_json_array = scenario_root_json["LEVELS"].toArray();
		for (const QJsonValue& current_level_value : levels_json_array)
		{            
            scenario.m_levels.emplace_back();
            Level& current_level = scenario.m_levels.back();
			const QJsonObject current_level_json = current_level_value.toObject();
			ScriptJSONSerializer::deserialize_level_json(*this, current_level_json, scenario, current_level);
		}

		QDir file_dir = file_info.absoluteDir();
		if (!file_dir.cd("Resources"))
		{
			QMessageBox::warning(m_core.get_main_window(), "Scenario Warning", QStringLiteral("No Resources folder present for this scenario file! Images will not be available for terminal previews!"));
		}

		m_internal->reset();
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
				// Need to check the end to make sure we catch the correct suffix (using the QFileInfo helper func might return an incorrect suffix)
				const QString file_name = current_file.fileName();
				if (file_name.endsWith(TERMINAL_SCRIPT_SUFFIX))
				{
					// We found a terminal script, parse it
					Level parsed_level;

					// Remove the suffix (should preserve the name in case someone put periods into it)
					parsed_level.m_name = file_name;
					parsed_level.m_name.remove(TERMINAL_SCRIPT_SUFFIX);

					parsed_level.m_dir_name = level_dir_name;
					parsed_level.m_script_name = parsed_level.m_name;
					ScriptParser parser(*this, parsed_level);
					if (parser.parse_level(current_file))
					{
						// Level successfully parsed
						scenario.m_levels.push_back(parsed_level);
					}
					break;
				}
			}
		}

		m_internal->reset();
		return true;
	}

	const ScenarioManager::TextColorArray& ScenarioManager::get_text_colors() const
	{
		return m_internal->m_text_colors;
	}

	void ScenarioManager::set_text_colors(const TextColorArray& colors)
	{
		m_internal->m_text_colors = colors;
	}

	QString ScenarioManager::print_level_script(const Level& level) const
	{
		QString level_script_text;
		int terminal_index = 0;
		for (const Terminal& current_terminal : level.m_terminals)
		{
			export_terminal_script(current_terminal, terminal_index, level_script_text);
			if (terminal_index < (level.m_terminals.size() - 1))
			{
				level_script_text += "\n";
			}
			++terminal_index;
		}
		return level_script_text;
	}

	const Terminal* ScenarioManager::get_screen_clipboard() const
	{
		return m_internal->m_screen_clipboard.get();
	}

	void ScenarioManager::set_screen_clipboard(const Terminal& terminal_data)
	{
		if (!m_internal->m_screen_clipboard)
		{
			m_internal->m_screen_clipboard = std::make_unique<Terminal>();
		}
		*m_internal->m_screen_clipboard = terminal_data;
	}

	void ScenarioManager::clear_screen_clipboard()
	{
		if (m_internal->m_screen_clipboard)
		{
			m_internal->m_screen_clipboard.reset();
		}
	}

	QString ScenarioManager::convert_ao_to_html(const QString& ao_text, int screen_type) const
	{
		// Wrap the text (so it matches the AO line wrapping)
		const QString wrapped_text = wrap_ao_text(ao_text, Utils::to_enum<Terminal::ScreenType>(screen_type));

		// Parse the Aleph One formatting tags, turning them into HTML tags that Qt can display
		QString parsed_text = wrapped_text.toHtmlEscaped(); // First make sure we have no unintended HTML tags
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
					color_tag += QString(HTML_TAG_ARRAY[Utils::to_integral(TextFormattingTags::TEXT_COLOR)]).arg(m_internal->m_text_colors[new_color_index].name());
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

		// Tabs in AO are a single space, so replace them
		parsed_text.replace('\t', ' ');

		// If the color was changed at some point, we need to close off the last span tag
		if (color_changed)
		{
			parsed_text += QStringLiteral("</span>");
		}
		// Wrap the text in paragraph tags
		parsed_text = QStringLiteral("<p style=\"white-space: pre-wrap\">%1</p>").arg(parsed_text);
		return parsed_text;
	}

	ScenarioManager::ScenarioManager(AppCore& core)
		: m_core(core)
		, m_internal(std::make_unique<Internal>())
	{
	}

	void ScenarioManager::export_level_script(QFile& level_file, const Level& level) const
	{
		QString level_script_text = print_level_script(level);
		QTextStream text_stream(&level_file);
		text_stream.setEncoding(TERMINAL_SCRIPT_ENCODING);
		text_stream << level_script_text;
	}

	void ScenarioManager::export_terminal_script(const Terminal& terminal, int terminal_index, QString& level_script_text) const
	{
		// Start with a comment tag (apparently needed for formatting?)
		level_script_text += ";\n";

		// Add the terminal header
		const QString terminal_id_string = QString::number(terminal_index);
		level_script_text += QStringLiteral("%1 %2\n").arg(get_script_keyword(ScriptKeywords::TERMINAL), terminal_id_string);

		int current_branch_index = 0;
		for (const Terminal::Branch& current_branch : terminal.m_branches)
		{
			const Terminal::BranchType current_branch_type = Utils::to_enum<Terminal::BranchType>(current_branch_index);
			if (current_branch.is_valid())
			{
				// Add the branch type header
				level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(get_branch_type_keyword(current_branch_type)));

				// Add screens and teleport info
				export_terminal_screens(current_branch.m_screens, level_script_text);
				if (current_branch.m_teleport.m_type != Terminal::TeleportType::NONE)
				{
					export_terminal_teleport(current_branch.m_teleport, level_script_text);
				}

				// Add the end header
				level_script_text += QStringLiteral("%1\n").arg(get_script_keyword(ScriptKeywords::END));
			}
			++current_branch_index;
		}

		// Add the terminal end header
		level_script_text += QStringLiteral("%1 %2").arg(get_script_keyword(ScriptKeywords::END_TERMINAL), terminal_id_string);
	}
}
