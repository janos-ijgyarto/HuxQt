#include <HuxQt/UI/EditLevelDialog.h>

#include <HuxQt/Scenario/ScenarioBrowserModel.h>
#include <HuxQt/UI/HuxQt.h>

#include <QRegularExpression>
#include <QPushButton>

namespace HuxApp
{
	EditLevelDialog::EditLevelDialog(HuxQt* main_window, const LevelInfo& level_info)
		: QDialog(main_window)
		, m_level_id(level_info.m_id)
		, m_modified(false)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		m_ui.setupUi(this);

		// Disable the OK button initially
		m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);

		m_ui.level_name_edit->setText(level_info.m_name);
		m_ui.script_name_edit->setText(level_info.m_script_name);
		m_ui.folder_name_edit->setText(level_info.m_dir_name);

		// Use a validator to exclude characters not allowed on Windows (also exclude "." so it doesn't mess up extensions)
		// TODO: make this cross-platform? Or find a compromise that works for everyone (which would allow them to share projects)?
		QRegularExpression path_regexp(R"([^/\\.:*?\"<>|]*)");
		QValidator* path_validator = new QRegularExpressionValidator(path_regexp, this);
		m_ui.script_name_edit->setValidator(path_validator);
		m_ui.folder_name_edit->setValidator(path_validator);

		connect_signals();
	}

	LevelInfo EditLevelDialog::get_level_info() const
	{
		LevelInfo edited_level_info;
		edited_level_info.m_id = m_level_id;
		edited_level_info.m_name = m_ui.level_name_edit->text();
		edited_level_info.m_script_name = m_ui.script_name_edit->text();
		edited_level_info.m_dir_name = m_ui.folder_name_edit->text();

		return edited_level_info;
	}

	void EditLevelDialog::connect_signals()
	{
		connect(m_ui.level_name_edit, &QLineEdit::textEdited, this, &EditLevelDialog::level_name_edited);
		connect(m_ui.script_name_edit, &QLineEdit::textEdited, this, &EditLevelDialog::script_name_edited);
		connect(m_ui.folder_name_edit, &QLineEdit::textEdited, this, &EditLevelDialog::folder_name_edited);

		connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &EditLevelDialog::ok_clicked);
		connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	}

	void EditLevelDialog::level_name_edited()
	{
		// TODO: anything else?
		level_data_modified();
	}

	void EditLevelDialog::script_name_edited()
	{
		// TODO: anything else?
		level_data_modified();
	}

	void EditLevelDialog::folder_name_edited()
	{
		// TODO: anything else?
		level_data_modified();
	}

	void EditLevelDialog::level_data_modified()
	{
		if (!m_modified)
		{
			// We made changes, enable the OK button
			m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
			m_modified = true;
		}
	}

	void EditLevelDialog::ok_clicked()
	{
		if (m_modified)
		{
			emit(changes_accepted());
		}
	}
}