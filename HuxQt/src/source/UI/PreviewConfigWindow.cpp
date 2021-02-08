#include <stdafx.h>
#include <UI/PreviewConfigWindow.h>

#include "AppCore.h"
#include "UI/HuxQt.h"
#include "UI/DisplaySystem.h"

namespace HuxApp
{
	PreviewConfigWindow::PreviewConfigWindow(AppCore& core)
		: m_core(core)
	{
		m_ui.setupUi(this);
		setAttribute(Qt::WA_DeleteOnClose);

		DisplaySystem::DisplayConfig current_config = m_core.get_display_system().get_display_config();

		m_ui.line_spacing_spinbox->setValue(current_config.m_lineSpacing);
		m_ui.word_spacing_spinbox->setValue(current_config.m_wordSpacing);
		m_ui.letter_spacing_spinbox->setValue(current_config.m_letterSpacing);
		m_ui.horizontal_margin_spinbox->setValue(current_config.m_horizontalMargin);
		m_ui.vertical_margin_spinbox->setValue(current_config.m_verticalMargin);

		connect(m_ui.line_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.word_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.letter_spacing_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.horizontal_margin_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
		connect(m_ui.vertical_margin_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PreviewConfigWindow::config_changed);
	}

	PreviewConfigWindow::~PreviewConfigWindow()
	{
		emit(window_closed());
	}

	void PreviewConfigWindow::config_changed()
	{
		DisplaySystem::DisplayConfig new_config;

		new_config.m_lineSpacing = m_ui.line_spacing_spinbox->value();
		new_config.m_wordSpacing = m_ui.word_spacing_spinbox->value();
		new_config.m_letterSpacing = m_ui.letter_spacing_spinbox->value();
		new_config.m_horizontalMargin = m_ui.horizontal_margin_spinbox->value();
		new_config.m_verticalMargin = m_ui.vertical_margin_spinbox->value();

		m_core.get_display_system().set_display_config(new_config);

		emit(config_update());
	}
}