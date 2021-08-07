#pragma once

namespace HuxApp
{
	class Terminal
	{
	public:
		enum class TeleportType
		{
			NONE,
			INTERLEVEL,
			INTRALEVEL,
			TYPE_COUNT
		};

		struct Teleport
		{
			TeleportType m_type = TeleportType::NONE;
			int m_index = 0;
		};

		enum class ScreenType
		{
			NONE,
			LOGON,
			INFORMATION,
			PICT,
			CHECKPOINT,
			LOGOFF,
			TAG,
			STATIC,
			TYPE_COUNT
		};
		
		enum class ScreenAlignment
		{
			LEFT,
			CENTER,
			RIGHT,
			ALIGNMENT_COUNT
		};

		struct Screen
		{
			ScreenType m_type = ScreenType::NONE;
			ScreenAlignment m_alignment = ScreenAlignment::LEFT; // Only relevant for PICT (possibly CHECKPOINT?)
			int m_resource_id = -1;
			QString m_display_text;
			QString m_script;
			QString m_comments;

			void reset();
		};

		const QString& get_name() const { return m_name; }
		void set_name(const QString& name) { m_name = name; }

		Screen& get_screen(int index, bool unfinished) { return (unfinished ? m_unfinished_screens[index] : m_finished_screens[index]); }
		std::vector<Screen>& get_screens(bool unfinished) { return (unfinished ? m_unfinished_screens : m_finished_screens); }
		Teleport& get_teleport_info(bool unfinished) { return (unfinished ? m_unfinished_teleport : m_finished_teleport); }

		const Screen& get_screen(int index, bool unfinished) const { return (unfinished ? m_unfinished_screens[index] : m_finished_screens[index]); }
		const std::vector<Screen>& get_screens(bool unfinished) const { return (unfinished ? m_unfinished_screens : m_finished_screens); }
		const Teleport& get_teleport_info(bool unfinished) const { return (unfinished ? m_unfinished_teleport : m_finished_teleport); }

		const QString& get_comments() const { return m_comments; }

		static QString get_screen_string(const Screen& screen_data);
	private:
		QString m_name;

		std::vector<Screen> m_unfinished_screens;
		Teleport m_unfinished_teleport;

		std::vector<Screen> m_finished_screens;
		Teleport m_finished_teleport;

		QString m_comments;

		friend class ScenarioManager;
	};

	// Utility object for model/view logic
	struct TerminalID
	{
		int m_level_id = -1;
		int m_terminal_id = -1;

		bool is_valid() const { return (m_level_id >= 0) && (m_terminal_id >= 0); }
		void invalidate()
		{
			m_level_id = -1;
			m_terminal_id = -1;
		}

		bool operator==(const TerminalID& rhs) const { return (m_level_id == rhs.m_level_id) && (m_terminal_id == rhs.m_terminal_id); }
	};
}