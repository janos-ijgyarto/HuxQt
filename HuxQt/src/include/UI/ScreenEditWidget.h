#pragma once
#include <QWidget>
#include <ui_ScreenEditWidget.h>

#include "Scenario/Terminal.h"
#include "Scenario/ScenarioManager.h"

namespace HuxApp
{
	class AppCore;

	class ScreenEditWidget : public QWidget
	{
		Q_OBJECT
	public:
		ScreenEditWidget(QWidget* parent = nullptr);
		void initialize(AppCore& core);

		void reset_editor(const Terminal::Screen& screen_data);
		void clear_editor();

		const Terminal::Screen& get_screen_data() const { return m_screen_data; }
		bool is_modified() const { return m_modified; }
		
		bool save_screen();
		void update_display_text();
	signals:
		void screen_edited(bool attributes);
	protected:
		bool eventFilter(QObject* obj, QEvent* event) override;
	private:
		void connect_signals();
		void init_ui();
		void enable_controls(bool enable);
		void validate_screen(Terminal::ScreenType screen_type);

		void screen_type_combo_activated(int index);
		void screen_resource_clicked();
		void pict_selected(int pict_id);
		void screen_resource_edited(const QString& text);
		void screen_alignment_combo_activated(int index);
		void screen_text_edited();

		void bold_button_clicked();
		void italic_button_clicked();
		void underline_button_clicked();
		void color_combo_activated(int index);

		void insert_font_tags(ScenarioManager::TextFont font);
		void insert_color_tags();

		void screen_edited_internal(bool attributes);

		AppCore* m_core;

		// Data
		Terminal::Screen m_screen_data;

		// Flags
		bool m_modified;
		bool m_text_dirty;
		bool m_resource_browser_enabled;
		bool m_initializing;

		Ui::ScreenEditWidget m_ui;
	};
}