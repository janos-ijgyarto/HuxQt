#pragma once
#include <QDialog>
#include <ui_TerminalEditDialog.h>

#include "Scenario/Terminal.h"

namespace HuxApp
{
	class HuxQt;

	class TerminalEditDialog : public QDialog
	{
		Q_OBJECT
	public:
		TerminalEditDialog(HuxQt* main_window, const QString& terminal_path, const Terminal& terminal_data);
	private:
		void connect_signals();
		void init_ui();

		void ok_clicked();
		void modified();
		bool gather_teleport_info(bool unfinished);

		const QString m_terminal_path;
		Terminal m_terminal_data;
		bool m_modified;

		Ui::TerminalEditDialog m_ui;
	};
}