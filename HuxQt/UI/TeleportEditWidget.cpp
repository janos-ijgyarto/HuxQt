#include <HuxQt/UI/TeleportEditWidget.h>

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

	TeleportEditWidget::TeleportEditWidget(const QString& label, const Terminal::Teleport& teleport_info)
	{
		m_ui.setupUi(this);

        m_ui.label->setText(label);
		m_ui.index_edit->setValidator(new QIntValidator(this));

        // Initialize from teleport info
        for (int teleport_type_index = 0; teleport_type_index < Utils::to_integral(Terminal::TeleportType::TYPE_COUNT); ++teleport_type_index)
        {
            m_ui.type_combo->addItem(TELEPORT_TYPE_LABELS[teleport_type_index]);
        }

        m_ui.type_combo->setCurrentIndex(Utils::to_integral(teleport_info.m_type));
        if (teleport_info.m_type != Terminal::TeleportType::NONE)
        {
            m_ui.index_edit->setText(QString::number(teleport_info.m_index));
        }
	}

	Terminal::Teleport TeleportEditWidget::get_teleport_info() const
	{
		Terminal::Teleport teleport_info;

        teleport_info.m_type = Utils::to_enum<Terminal::TeleportType>(m_ui.type_combo->currentIndex());
        if (teleport_info.m_type != Terminal::TeleportType::NONE)
        {
            // Active teleport, so the user must provide a valid input
            const QString teleport_index_text = m_ui.index_edit->text();
            if (!teleport_index_text.isEmpty())
            {
                teleport_info.m_index = teleport_index_text.toInt();
            }
            else
            {
                teleport_info.m_index = 0;
            }
        }
        else
        {
            teleport_info.m_index = -1;
        }

        return teleport_info;
	}

    bool TeleportEditWidget::is_valid() const
    {
        Terminal::TeleportType teleport_type = Utils::to_enum<Terminal::TeleportType>(m_ui.type_combo->currentIndex());
        if (teleport_type != Terminal::TeleportType::NONE)
        {
            // Active teleport, so the user must provide a valid input
            const QString teleport_index_text = m_ui.index_edit->text();
            if (teleport_index_text.isEmpty())
            {
                return false;
            }
        }

        return true;
    }

    void TeleportEditWidget::connect_signals()
    {
        connect(m_ui.index_edit, &QLineEdit::textEdited, this, &TeleportEditWidget::teleport_info_modified);
        connect(m_ui.type_combo, QOverload<int>::of(&QComboBox::activated), this, &TeleportEditWidget::teleport_info_modified);
    }
}