#include "stdafx.h"
#include "UI/TerminalEditDialog.h"

#include "UI/HuxQt.h"
#include "Utils/Utilities.h"

namespace HuxApp
{
    namespace
    {
        constexpr const char* TELEPORT_TYPE_LABELS[Utils::to_integral(Terminal::TeleportType::TYPE_COUNT)] =
        {
            "NONE",
            "INTERLEVEL",
            "INTRALEVEL"
        };
    }

	TerminalEditDialog::TerminalEditDialog(HuxQt* main_window, const QString& terminal_path, const Terminal& terminal_data)
        : QDialog(main_window)
        , m_terminal_path(terminal_path)
        , m_modified(false)
	{
        m_ui.setupUi(this);
        setAttribute(Qt::WA_DeleteOnClose, true);

        // Change the window title to include the "terminal path" (this helps remind the user exactly which terminal is being edited)
        setWindowTitle(windowTitle() + QStringLiteral(" (%1)").arg(terminal_path));

        init_ui();

        // Initialize based on the terminal data
        m_terminal_data.set_id(terminal_data.get_id());
        m_terminal_data.get_teleport_info(true) = terminal_data.get_teleport_info(true);
        m_terminal_data.get_teleport_info(false) = terminal_data.get_teleport_info(false);

        m_ui.terminal_id_edit->setText(QString::number(terminal_data.get_id()));

        // Teleport info
        {
            const Terminal::Teleport& unfinished_teleport = terminal_data.get_teleport_info(true);
            m_ui.unfinished_teleport_type->setCurrentIndex(Utils::to_integral(unfinished_teleport.m_type));
            if (unfinished_teleport.m_type != Terminal::TeleportType::NONE)
            {
                m_ui.unfinished_teleport_edit->setText(QString::number(unfinished_teleport.m_index));
            }
        }

        {
            const Terminal::Teleport& finished_teleport = terminal_data.get_teleport_info(false);
            m_ui.finished_teleport_type->setCurrentIndex(Utils::to_integral(finished_teleport.m_type));

            m_ui.finished_teleport_type->setCurrentIndex(Utils::to_integral(finished_teleport.m_type));
            if (finished_teleport.m_type != Terminal::TeleportType::NONE)
            {
                m_ui.finished_teleport_edit->setText(QString::number(finished_teleport.m_index));
            }
        }

        connect_signals();
	}

    void TerminalEditDialog::connect_signals()
    {
        connect(m_ui.dialog_button_box, &QDialogButtonBox::accepted, this, &TerminalEditDialog::ok_clicked);
        connect(m_ui.dialog_button_box, &QDialogButtonBox::rejected, this, &TerminalEditDialog::reject);

        connect(m_ui.terminal_id_edit, &QLineEdit::textEdited, this, &TerminalEditDialog::modified);
        connect(m_ui.unfinished_teleport_edit, &QLineEdit::textEdited, this, &TerminalEditDialog::modified);
        connect(m_ui.finished_teleport_edit, &QLineEdit::textEdited, this, &TerminalEditDialog::modified);
        connect(m_ui.unfinished_teleport_type, QOverload<int>::of(&QComboBox::activated), this, &TerminalEditDialog::modified);
        connect(m_ui.finished_teleport_type, QOverload<int>::of(&QComboBox::activated), this, &TerminalEditDialog::modified);
    }

    void TerminalEditDialog::init_ui()
    {
        // Set validators so we only accept numbers
        m_ui.terminal_id_edit->setValidator(new QIntValidator(this));
        m_ui.unfinished_teleport_edit->setValidator(new QIntValidator(this));
        m_ui.finished_teleport_edit->setValidator(new QIntValidator(this));

        for (int teleport_type_index = 0; teleport_type_index < Utils::to_integral(Terminal::TeleportType::TYPE_COUNT); ++teleport_type_index)
        {
            m_ui.unfinished_teleport_type->addItem(TELEPORT_TYPE_LABELS[teleport_type_index]);
            m_ui.finished_teleport_type->addItem(TELEPORT_TYPE_LABELS[teleport_type_index]);
        }

        m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(false);
    }

    void TerminalEditDialog::ok_clicked()
    {
        if (m_modified)
        {
            // Gather and validate terminal info
            const QString new_terminal_id_text = m_ui.terminal_id_edit->text();
            if (new_terminal_id_text.isEmpty())
            {
                QMessageBox::warning(this, "Terminal Error", "Terminal ID field must not be empty!");
                return;
            }
            m_terminal_data.set_id(new_terminal_id_text.toInt());

            if (gather_teleport_info(true) && gather_teleport_info(false))
            {
                // Internal validation OK, call main window func to validate and save
                HuxQt* main_window = qobject_cast<HuxQt*>(parent());
                if (!main_window->save_terminal_info(m_terminal_path, m_terminal_data))
                {
                    QMessageBox::warning(this, "Level Error", "Terminal ID conflicts with another terminal in this level!");
                    return;
                }
            }
            else
            {
                QMessageBox::warning(this, "Terminal Error", "Active teleport must have valid destination (field must not be empty)!");
                return;
            }
        }

        // Everything is OK, we can close
        QDialog::accept();
    }

    void TerminalEditDialog::modified()
    {
        if (!m_modified)
        {
            setWindowTitle(windowTitle() + " - Modified");
            m_ui.dialog_button_box->button(QDialogButtonBox::StandardButton::Ok)->setEnabled(true);
            m_modified = true;
        }
    }

    bool TerminalEditDialog::gather_teleport_info(bool unfinished)
    {
        Terminal::Teleport& teleport_info = m_terminal_data.get_teleport_info(unfinished);
        QLineEdit* teleport_index_edit = unfinished ? m_ui.unfinished_teleport_edit : m_ui.finished_teleport_edit;
        QComboBox* teleport_type_combo = unfinished ? m_ui.unfinished_teleport_type : m_ui.finished_teleport_type;

        // First set the type
        teleport_info.m_type = static_cast<Terminal::TeleportType>(teleport_type_combo->currentIndex());

        if (teleport_info.m_type != Terminal::TeleportType::NONE)
        {
            // Active teleport, so the user must provide a valid input
            const QString teleport_index_text = teleport_index_edit->text();
            if (teleport_index_text.isEmpty())
            {
                return false;
            }
            teleport_info.m_index = teleport_index_text.toInt();
        }
        else
        {
            teleport_info.m_index = -1;
        }
        return true;
    }
}