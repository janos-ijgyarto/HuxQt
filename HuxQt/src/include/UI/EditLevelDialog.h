#pragma once
#include <QDialog>
#include <ui_EditLevelDialog.h>

namespace HuxApp
{
	class HuxQt;
	
	struct LevelInfo;
	class ScenarioBrowserModel;

	class EditLevelDialog : public QDialog
	{
		Q_OBJECT
	public:
		EditLevelDialog(HuxQt* main_window, const LevelInfo& level_info);

		LevelInfo get_level_info() const;
	signals:
		void changes_accepted();
	private:
		void connect_signals();

		void level_name_edited();
		void script_name_edited();
		void folder_name_edited();

		void level_data_modified();

		void ok_clicked();

		Ui::EditLevelDialog m_ui;
		const int m_level_id;
		bool m_modified;
	};
}