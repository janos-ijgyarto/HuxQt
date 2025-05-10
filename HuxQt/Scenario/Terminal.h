#pragma once
#include <HuxQt/Utils/Utilities.h>

#include <QString>
#include <array>

namespace HuxApp
{
	class Terminal
	{
	public:
		enum class BranchType
		{
			UNFINISHED,
			FINISHED,
			FAILED,
			TYPE_COUNT
		};

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

		using ScreenVector = std::vector<Screen>;

		struct Branch
		{
			ScreenVector m_screens;
			Teleport m_teleport;

			bool is_valid() const { return (!m_screens.empty()) || (m_teleport.m_type != TeleportType::NONE); }
		};

		using BranchArray = std::array<Branch, Utils::to_integral(BranchType::TYPE_COUNT)>;

		const QString& get_name() const { return m_name; }
		void set_name(const QString& name) { m_name = name; }

		const BranchArray& get_branches() const { return m_branches; }

		const Branch& get_branch(BranchType branch) const { return m_branches[Utils::to_integral(branch)]; }
		Branch& get_branch(BranchType branch) { return const_cast<Branch&>(const_cast<const Terminal*>(this)->get_branch(branch)); }

		const QString& get_comments() const { return m_comments; }

		static const char* get_branch_type_name(BranchType type);
		static QString get_screen_string(const Screen& screen_data);
	private:
		QString m_name;

		BranchArray m_branches;

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
        bool operator!=(const TerminalID& rhs) const { return !(*this == rhs); }
	};
}
