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

		Screen& get_screen(int index, bool unfinished) { return (unfinished ? m_unfinished_screens[index] : m_finished_screens[index]); }
		std::vector<Screen>& get_screens(bool unfinished) { return (unfinished ? m_unfinished_screens : m_finished_screens); }
		Teleport& get_teleport_info(bool unfinished) { return (unfinished ? m_unfinished_teleport : m_finished_teleport); }

		const Screen& get_screen(int index, bool unfinished) const { return (unfinished ? m_unfinished_screens[index] : m_finished_screens[index]); }
		const std::vector<Screen>& get_screens(bool unfinished) const { return (unfinished ? m_unfinished_screens : m_finished_screens); }
		const Teleport& get_teleport_info(bool unfinished) const { return (unfinished ? m_unfinished_teleport : m_finished_teleport); }

		const QString& get_comments() const { return m_comments; }

		static QString get_screen_string(const Screen& screen_data);
	private:
		std::vector<Screen> m_unfinished_screens;
		Teleport m_unfinished_teleport;

		std::vector<Screen> m_finished_screens;
		Teleport m_finished_teleport;

		QString m_comments;

		friend class ScenarioManager;
	};
}