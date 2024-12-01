#include <HuxQt/UI/PreviewConfigWindow.h>

#include <HuxQt/AppCore.h>
#include <HuxQt/UI/HuxQt.h>
#include <HuxQt/UI/DisplaySystem.h>

namespace HuxApp
{
	PreviewConfigWindow::PreviewConfigWindow(AppCore& core)
		: m_core(core)
	{
		m_ui.setupUi(this);
		setAttribute(Qt::WA_DeleteOnClose);

		DisplaySystem::DisplayConfig current_config = m_core.get_display_system().get_display_config();

		m_ui.line_spacing_spinbox->setValue(current_config.m_line_spacing);
		m_ui.word_spacing_spinbox->setValue(current_config.m_word_spacing);
		m_ui.letter_spacing_spinbox->setValue(current_config.m_letter_spacing);
		m_ui.horizontal_margin_spinbox->setValue(current_config.m_horizontal_margin);
		m_ui.vertical_margin_spinbox->setValue(current_config.m_vertical_margin);
		m_ui.line_numbers_checkbox->setChecked(current_config.m_show_line_numbers);

		connect(m_ui.line_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.word_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.letter_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.horizontal_margin_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.vertical_margin_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.line_numbers_checkbox, &QCheckBox::stateChanged, this, &PreviewConfigWindow::config_changed);
	}

	PreviewConfigWindow::~PreviewConfigWindow()
	{
		emit(window_closed());
	}

	void PreviewConfigWindow::config_changed()
	{
		DisplaySystem::DisplayConfig new_config;

		new_config.m_line_spacing = m_ui.line_spacing_spinbox->value();
		new_config.m_word_spacing = m_ui.word_spacing_spinbox->value();
		new_config.m_letter_spacing = m_ui.letter_spacing_spinbox->value();
		new_config.m_horizontal_margin = m_ui.horizontal_margin_spinbox->value();
		new_config.m_vertical_margin = m_ui.vertical_margin_spinbox->value();
		new_config.m_show_line_numbers = m_ui.line_numbers_checkbox->isChecked();

		m_core.get_display_system().set_display_config(new_config);

		emit(config_update());
	}
}