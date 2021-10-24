#include "stdafx.h"
#include "UI/ScreenEditWidget.h"

#include "Scenario/ScenarioManager.h"

#include "UI/BrowsePictDialog.h"

#include "Utils/Utilities.h"

namespace HuxApp
{
    namespace
    {
        constexpr const char* SCREEN_TYPE_LABELS[Utils::to_integral(Terminal::ScreenType::TYPE_COUNT)] = {
            "NONE",
            "LOGON",
            "INFORMATION",
            "PICT",
            "CHECKPOINT",
            "LOGOFF",
            "TAG",
            "STATIC"
        };

        constexpr const char* SCREEN_ALIGNMENT_LABELS[Utils::to_integral(Terminal::ScreenAlignment::ALIGNMENT_COUNT)] = {
            "LEFT",
            "CENTER",
            "RIGHT",
        };

        constexpr const char* TEXT_COLOR_LABELS[Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT)] = {
            "Light Green (Default)",
            "White",
            "Red",
            "Dark Green",
            "Light Blue",
            "Yellow",
            "Dark Red",
            "Dark Blue"
        };

        const QColor SCREEN_TEXT_COLORS[Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT)] = {
            Qt::green,
            Qt::white,
            Qt::red,
            Qt::darkGreen,
            Qt::blue,
            Qt::yellow,
            Qt::darkRed,
            Qt::darkBlue
        };

        const QColor COLOR_COMBO_TEXT_COLORS[Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT)] = {
            Qt::black,
            Qt::black,
            Qt::white,
            Qt::white,
            Qt::white,
            Qt::black,
            Qt::white,
            Qt::white
        };

        QString get_resource_value_label(Terminal::ScreenType screen_type)
        {
            switch (screen_type)
            {
            case Terminal::ScreenType::LOGON:
            case Terminal::ScreenType::PICT:
            case Terminal::ScreenType::LOGOFF:
                return QStringLiteral("Image ID:");
            case Terminal::ScreenType::CHECKPOINT:
                return QStringLiteral("Checkpoint ID:");
            case Terminal::ScreenType::TAG:
                return QStringLiteral("Tag ID:");
            case Terminal::ScreenType::STATIC:
                return QStringLiteral("Static duration:");
            }

            return QStringLiteral("N/A");
        }
    }

    ScreenEditWidget::ScreenEditWidget(QWidget* parent)
        : QWidget(parent)
        , m_core(nullptr)
        , m_modified(false)
        , m_text_dirty(false)
        , m_initializing(false)
    {
        m_ui.setupUi(this);

        init_ui();
        connect_signals();
    }

    void ScreenEditWidget::initialize(AppCore& core)
    {
        m_core = &core;
    }

    void ScreenEditWidget::reset_editor(const Terminal::Screen& screen_data)
    {
        // Set initializing flag to lock the state changes
        m_initializing = true;

        // Reset flags
        m_modified = false;
        m_text_dirty = false;

        m_screen_data = screen_data;

        // Initialize controls based on screen data
        m_ui.screen_type_combo->setCurrentIndex(Utils::to_integral(screen_data.m_type));
        m_ui.alignment_combo->setCurrentIndex(Utils::to_integral(screen_data.m_alignment));

        // Enable controls based on whether the screen type is properly set (still update the controls, as we may have just toggled the type)
        update_controls(screen_data.m_type);

        // Update the control values
        m_ui.resource_value_edit->setText(QString::number(screen_data.m_resource_id));
        m_ui.screen_text_edit->setPlainText(screen_data.m_script);

        // Unset the flag to allow state changes
        m_initializing = false;
    }
    
    void ScreenEditWidget::clear_editor()
    {
        // Set initializing flag to lock the state changes
        m_initializing = true;

        m_ui.resource_value_edit->clear();
        m_ui.screen_text_edit->clear();

        // Unset the flag to allow state changes
        m_initializing = false;
    }

    bool ScreenEditWidget::save_screen()
    {
        // Validate screen data
        switch (m_screen_data.m_type)
        {
        case Terminal::ScreenType::LOGON:
        case Terminal::ScreenType::LOGOFF:
        case Terminal::ScreenType::PICT:
        case Terminal::ScreenType::CHECKPOINT:
        case Terminal::ScreenType::TAG:
        case Terminal::ScreenType::STATIC:
        {
            if (m_ui.resource_value_edit->text().isEmpty())
            {
                QMessageBox::warning(this, "Screen Data Error", "Must set a valid resource ID!");
                return false;
            }
        }
            break;
        }

        if ((m_screen_data.m_type == Terminal::ScreenType::PICT) && (m_screen_data.m_alignment == Terminal::ScreenAlignment::CENTER))
        {
            if (!m_screen_data.m_script.isEmpty())
            {
                QMessageBox::warning(this, "Screen Data Error", "Centered PICT must not have any text!");
                return false;
            }
        }

        m_modified = false;
        update_display_text();
        return true;
    }

    void ScreenEditWidget::update_display_text()
    {
        if (m_text_dirty)
        {
            m_screen_data.m_display_text = ScenarioManager::convert_ao_to_html(m_screen_data.m_script, Utils::to_integral(m_screen_data.m_type));
            m_text_dirty = false;
        }
    }

    void ScreenEditWidget::connect_signals()
    {
        connect(m_ui.screen_type_combo, QOverload<int>::of(&QComboBox::activated), this, &ScreenEditWidget::screen_type_combo_activated);
        connect(m_ui.alignment_combo, QOverload<int>::of(&QComboBox::activated), this, &ScreenEditWidget::screen_alignment_combo_activated);
        connect(m_ui.text_color_combo, QOverload<int>::of(&QComboBox::activated), this, &ScreenEditWidget::color_combo_activated);
        connect(m_ui.resource_value_edit, &QLineEdit::textEdited, this, &ScreenEditWidget::screen_resource_edited);
        connect(m_ui.screen_text_edit, &QPlainTextEdit::textChanged, this, &ScreenEditWidget::screen_text_edited);

        connect(m_ui.resource_browse_button, &QPushButton::clicked, this, &ScreenEditWidget::browse_resource_clicked);
        connect(m_ui.bold_button, &QPushButton::clicked, this, &ScreenEditWidget::bold_button_clicked);
        connect(m_ui.italic_button, &QPushButton::clicked, this, &ScreenEditWidget::italic_button_clicked);
        connect(m_ui.underline_button, &QPushButton::clicked, this, &ScreenEditWidget::underline_button_clicked);
        connect(m_ui.text_color_button, &QPushButton::clicked, this, &ScreenEditWidget::insert_color_tags);

        // Add shortcuts
        QShortcut* bold_shortcut = new QShortcut(QKeySequence("Ctrl+B"), m_ui.screen_text_edit);
        QShortcut* italic_shortcut = new QShortcut(QKeySequence("Ctrl+I"), m_ui.screen_text_edit);
        QShortcut* underline_shortcut = new QShortcut(QKeySequence("Ctrl+U"), m_ui.screen_text_edit);
        connect(bold_shortcut, &QShortcut::activated, this, &ScreenEditWidget::bold_button_clicked);
        connect(italic_shortcut, &QShortcut::activated, this, &ScreenEditWidget::italic_button_clicked);
        connect(underline_shortcut, &QShortcut::activated, this, &ScreenEditWidget::underline_button_clicked);
    }

    void ScreenEditWidget::init_ui()
    {
        m_ui.resource_value_edit->setValidator(new QIntValidator(this));

        for (int screen_type_index = 0; screen_type_index < Utils::to_integral(Terminal::ScreenType::TYPE_COUNT); ++screen_type_index)
        {
            m_ui.screen_type_combo->addItem(SCREEN_TYPE_LABELS[screen_type_index]);
        }

        for (int screen_align_index = 0; screen_align_index < Utils::to_integral(Terminal::ScreenAlignment::ALIGNMENT_COUNT); ++screen_align_index)
        {
            m_ui.alignment_combo->addItem(SCREEN_ALIGNMENT_LABELS[screen_align_index]);
        }

        for (int text_color_index = 0; text_color_index < Utils::to_integral(ScenarioManager::TextColor::COLOR_COUNT); ++text_color_index)
        {
            m_ui.text_color_combo->addItem(TEXT_COLOR_LABELS[text_color_index]);
            m_ui.text_color_combo->setItemData(text_color_index, SCREEN_TEXT_COLORS[text_color_index], Qt::BackgroundRole);
            m_ui.text_color_combo->setItemData(text_color_index, COLOR_COMBO_TEXT_COLORS[text_color_index], Qt::ForegroundRole);
        }

        m_ui.resource_browse_button->setVisible(false);

        m_ui.screen_type_combo->setCurrentIndex(0);
        m_ui.alignment_combo->setCurrentIndex(0);
        m_ui.text_color_combo->setCurrentIndex(0);
        color_combo_activated(0); // Set the button to the appropriate color
    }

    void ScreenEditWidget::update_controls(Terminal::ScreenType screen_type)
    {
        // Enable/disable specific controls based on screen type
        const bool valid_screen = (screen_type != Terminal::ScreenType::NONE) && (screen_type != Terminal::ScreenType::TAG) && (screen_type != Terminal::ScreenType::STATIC);

        m_ui.alignment_combo->setEnabled((screen_type == Terminal::ScreenType::PICT) || (screen_type == Terminal::ScreenType::CHECKPOINT)); // Alignment is only used by PICT and CHECKPOINT
        m_ui.screen_text_edit->setEnabled(valid_screen);
        m_ui.bold_button->setEnabled(valid_screen);
        m_ui.italic_button->setEnabled(valid_screen);
        m_ui.underline_button->setEnabled(valid_screen);
        m_ui.text_color_button->setEnabled(valid_screen);
        m_ui.text_color_combo->setEnabled(valid_screen);

        m_ui.resource_value_edit->setEnabled((screen_type != Terminal::ScreenType::NONE) && (screen_type != Terminal::ScreenType::INFORMATION));

        // Show the browse button if the screen type has an image
        m_ui.resource_browse_button->setVisible(valid_screen && (screen_type != Terminal::ScreenType::INFORMATION) && (screen_type != Terminal::ScreenType::CHECKPOINT));

        // Update the resource label
        m_ui.resource_value_label->setText(get_resource_value_label(screen_type));
    }

    void ScreenEditWidget::screen_type_combo_activated(int index)
    {
        if (index != Utils::to_integral(m_screen_data.m_type))
        {
            m_screen_data.m_type = static_cast<Terminal::ScreenType>(index);
            update_controls(m_screen_data.m_type); // We changed the screen type, so we need to make sure the correct controls are available
            screen_edited_internal(true);
        }
    }

    void ScreenEditWidget::browse_resource_clicked()
    {
        BrowsePictDialog* pict_dialog = new BrowsePictDialog(*m_core, this);
        connect(pict_dialog, &BrowsePictDialog::pict_selected, this, &ScreenEditWidget::pict_selected);
        pict_dialog->open();
    }

    void ScreenEditWidget::pict_selected(int pict_id)
    {
        if (m_screen_data.m_resource_id != pict_id)
        {
            m_ui.resource_value_edit->setText(QString::number(pict_id));
            m_screen_data.m_resource_id = pict_id;
            screen_edited_internal(true);
        }
    }

    void ScreenEditWidget::screen_resource_edited(const QString& text)
    {
        const int new_resource_id = text.toInt();
        if (m_screen_data.m_resource_id != new_resource_id)
        {
            m_screen_data.m_resource_id = new_resource_id;
            screen_edited_internal(true);
        }
    }

    void ScreenEditWidget::screen_alignment_combo_activated(int index)
    {
        if (index != Utils::to_integral(m_screen_data.m_alignment))
        {      
            m_screen_data.m_alignment = static_cast<Terminal::ScreenAlignment>(index);
            screen_edited_internal(false);
        }
    }

    void ScreenEditWidget::screen_text_edited()
    {
        m_text_dirty = true;
        m_screen_data.m_script = m_ui.screen_text_edit->toPlainText();
        screen_edited_internal(false);
    }

    void ScreenEditWidget::bold_button_clicked()
    {
        insert_font_tags(ScenarioManager::TextFont::BOLD);
    }

    void ScreenEditWidget::italic_button_clicked()
    {
        insert_font_tags(ScenarioManager::TextFont::ITALIC);
    }

    void ScreenEditWidget::underline_button_clicked()
    {
        insert_font_tags(ScenarioManager::TextFont::UNDERLINE);
    }

    void ScreenEditWidget::color_combo_activated(int index)
    {
        const ScenarioManager::TextColor selected_color = static_cast<ScenarioManager::TextColor>(index);
        
        // Generate the icon for the button
        QPixmap button_icon_pixmap(100, 100);
        button_icon_pixmap.fill(SCREEN_TEXT_COLORS[index]);

        QIcon button_icon(button_icon_pixmap);
        m_ui.text_color_button->setIcon(button_icon);
    }

    void ScreenEditWidget::insert_font_tags(ScenarioManager::TextFont font)
    {
        // TODO: possibly remove redundant tags within the selection?
        QTextCursor cursor = m_ui.screen_text_edit->textCursor();
        switch (font)
        {
        case ScenarioManager::TextFont::BOLD:
            cursor.insertText(QStringLiteral("$B%1$b").arg(cursor.selectedText()));
            break;
        case ScenarioManager::TextFont::ITALIC:
            cursor.insertText(QStringLiteral("$I%1$i").arg(cursor.selectedText()));
            break;
        case ScenarioManager::TextFont::UNDERLINE:
            cursor.insertText(QStringLiteral("$U%1$u").arg(cursor.selectedText()));
            break;
        }
    }

    void ScreenEditWidget::insert_color_tags()
    {
        // TODO: possibly remove redundant tags within the selection?
        QTextCursor cursor = m_ui.screen_text_edit->textCursor();
        const int color_index = m_ui.text_color_combo->currentIndex();

        cursor.insertText(QStringLiteral("$C%1%2").arg(QString::number(color_index), cursor.selectedText()));
    }

    void ScreenEditWidget::screen_edited_internal(bool attributes)
    {
        if (!m_initializing)
        {
            m_modified = true;
            emit(screen_edited(attributes));
        }
    }
}