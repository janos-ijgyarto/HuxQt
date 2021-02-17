#include "stdafx.h"
#include "UI/ScreenEditorWindow.h"
#include <ui_ScreenEditorWindow.h>

#include "AppCore.h"

#include "UI/HuxQt.h"
#include "UI/ScreenEditTab.h"
#include "UI/DisplayData.h"
#include "UI/DisplaySystem.h"

namespace HuxApp
{
	namespace
	{
		constexpr int SCREEN_EDIT_TIMEOUT = 1000; // Wait this number of ms after the last user edit before updating the screen preview
		constexpr int SCREEN_EDIT_PROGRESS_STEP_COUNT = 100; // Have this many steps in the progress bar shown to the user
		constexpr int SCREEN_EDIT_TIMER_INTERVAL = SCREEN_EDIT_TIMEOUT / SCREEN_EDIT_PROGRESS_STEP_COUNT;
	}

	struct ScreenEditorWindow::Internal
	{
		Ui::ScreenEditorWindow m_ui;

		QTimer m_edit_timer; // Timer that updates the UI after an edit (delays the full update so we don't change the display immediately on every slight change)
	};

	ScreenEditorWindow::ScreenEditorWindow(AppCore& core)
		: m_core(core)
		, m_internal(std::make_unique<Internal>())
	{
		m_internal->m_ui.setupUi(this);
		m_internal->m_edit_timer.stop();
		reset_edit_notification();

		core.get_display_system().set_graphics_view(DisplaySystem::View::SCREEN_EDITOR, m_internal->m_ui.screen_preview);
		connect_signals();
	}

	void ScreenEditorWindow::open_screen(QTreeWidgetItem* screen_item, const Terminal::Screen& screen_data)
	{
		const int screen_tab_index = find_screen_index(screen_item);
		if (screen_tab_index != -1)
		{
			// Tab already open, set it as the current tab
			m_internal->m_ui.screen_edit_tabs->setCurrentIndex(screen_tab_index);
			return;
		}

		// Add new tab, connect to its signals
		ScreenEditTab* new_tab = new ScreenEditTab(m_core, screen_item, screen_data);
		connect(new_tab, &ScreenEditTab::screen_edited, this, &ScreenEditorWindow::screen_edited);

		const int new_tab_index = m_internal->m_ui.screen_edit_tabs->addTab(new_tab, m_core.get_main_window()->get_screen_path(screen_item));
		m_internal->m_ui.screen_edit_tabs->setCurrentIndex(new_tab_index);
	}

	void ScreenEditorWindow::update_screen(QTreeWidgetItem* screen_item, const QString& screen_path)
	{
		const int screen_tab_index = find_screen_index(screen_item);
		if (screen_tab_index != -1)
		{
			m_internal->m_ui.screen_edit_tabs->setTabText(screen_tab_index, screen_path);
		}
	}

	void ScreenEditorWindow::close_screen(QTreeWidgetItem* screen_item)
	{
		const int screen_tab_index = find_screen_index(screen_item);
		if (screen_tab_index != -1)
		{
			QWidget* selected_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(screen_tab_index);
			ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(selected_tab_widget);

			if(prompt_save(selected_tab))
			{
				m_internal->m_ui.screen_edit_tabs->removeTab(screen_tab_index);
				selected_tab->deleteLater();
			}
		}
	}

	void ScreenEditorWindow::remove_screen(QTreeWidgetItem* screen_item)
	{
		const int screen_tab_index = find_screen_index(screen_item);
		if (screen_tab_index != -1)
		{
			QWidget* selected_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(screen_tab_index);
			ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(selected_tab_widget);

			m_internal->m_ui.screen_edit_tabs->removeTab(screen_tab_index);
			selected_tab->deleteLater();
		}
	}

	bool ScreenEditorWindow::check_unsaved()
	{
		for (int current_tab_index = 0; current_tab_index < m_internal->m_ui.screen_edit_tabs->count(); ++current_tab_index)
		{
			QWidget* current_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(current_tab_index);
			ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);

			if (!prompt_save(current_tab))
			{
				// We cancelled somewhere along the way
				return false;
			}
		}

		return true;
	}

	void ScreenEditorWindow::clear_editor()
	{
		for (int current_tab_index = 0; current_tab_index < m_internal->m_ui.screen_edit_tabs->count(); ++current_tab_index)
		{
			QWidget* current_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(current_tab_index);
			current_tab_widget->deleteLater();
		}

		m_internal->m_ui.screen_edit_tabs->clear();
	}

	void ScreenEditorWindow::connect_signals()
	{
		connect(&m_internal->m_edit_timer, &QTimer::timeout, this, &ScreenEditorWindow::update_edit_notification);

		connect(m_internal->m_ui.screen_edit_tabs, &QTabWidget::currentChanged, this, &ScreenEditorWindow::current_tab_changed);
		connect(m_internal->m_ui.screen_edit_tabs, &QTabWidget::tabCloseRequested, this, &ScreenEditorWindow::close_tab);
		connect(m_internal->m_ui.save_all_button, &QPushButton::clicked, this, &ScreenEditorWindow::save_all);
	}

	int ScreenEditorWindow::find_screen_index(QTreeWidgetItem* screen_item)
	{
		for (int current_tab_index = 0; current_tab_index < m_internal->m_ui.screen_edit_tabs->count(); ++current_tab_index)
		{
			QWidget* current_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(current_tab_index);
			ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);

			if (current_tab->get_screen_item() == screen_item)
			{
				return current_tab_index;
			}
		}

		return -1;
	}

	void ScreenEditorWindow::current_tab_changed(int index)
	{
		reset_edit_notification();
		update_preview();
	}

	void ScreenEditorWindow::close_tab(int index)
	{
		QWidget* selected_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(index);
		ScreenEditTab* selected_tab = qobject_cast<ScreenEditTab*>(selected_tab_widget);

		if (!prompt_save(selected_tab))
		{
			return;
		}

		m_core.get_main_window()->screen_edit_tab_closed(selected_tab->get_screen_item()); // Let the main window know that we are no longer editing this screen
		m_internal->m_ui.screen_edit_tabs->removeTab(index);
		selected_tab->deleteLater();
	}

	void ScreenEditorWindow::screen_edited(QTreeWidgetItem* screen_item)
	{
		// Set the modified tab's label to red
		const int screen_tab_index = find_screen_index(screen_item);
		m_internal->m_ui.screen_edit_tabs->tabBar()->setTabTextColor(screen_tab_index, Qt::red);

		// Start/restart the timer for updating the preview
		m_internal->m_edit_timer.start(SCREEN_EDIT_TIMER_INTERVAL);
		m_internal->m_ui.screen_update_progress_bar->setValue(0);
		m_internal->m_ui.screen_update_progress_bar->setVisible(true);
	}

	bool ScreenEditorWindow::prompt_save(ScreenEditTab* screen_tab)
	{
		if (screen_tab->is_modified())
		{
			const int tab_index = m_internal->m_ui.screen_edit_tabs->indexOf(screen_tab);

			const QMessageBox::StandardButton user_response = QMessageBox::question(this, "Screen Modified",
				QStringLiteral("Save Changes to \"%1\"?").arg(m_internal->m_ui.screen_edit_tabs->tabBar()->tabText(tab_index)),
				QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel));

			switch (user_response)
			{
			case QMessageBox::Yes:
				save_screen(screen_tab);
				return true;
			case QMessageBox::No:
				return true;
			case QMessageBox::Cancel:
				return false;
			}
		}

		return true;
	}

	void ScreenEditorWindow::save_screen(ScreenEditTab* screen_tab)
	{
		if (screen_tab->is_modified())
		{
			// Reset the tab label color
			const int screen_tab_index = m_internal->m_ui.screen_edit_tabs->indexOf(screen_tab);
			m_internal->m_ui.screen_edit_tabs->tabBar()->setTabTextColor(screen_tab_index, Qt::black);

			// Save the screen data
			if (screen_tab->save_screen())
			{
				m_core.get_main_window()->save_screen(screen_tab->get_screen_item(), screen_tab->get_screen_data());
			}
		}
	}

	void ScreenEditorWindow::save_all()
	{
		for (int current_tab_index = 0; current_tab_index < m_internal->m_ui.screen_edit_tabs->count(); ++current_tab_index)
		{
			QWidget* current_tab_widget = m_internal->m_ui.screen_edit_tabs->widget(current_tab_index);
			ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);

			save_screen(current_tab);
		}
	}

	void ScreenEditorWindow::update_preview()
	{
		QWidget* current_tab_widget = m_internal->m_ui.screen_edit_tabs->currentWidget();
		if (current_tab_widget)
		{
			ScreenEditTab* current_tab = qobject_cast<ScreenEditTab*>(current_tab_widget);
			current_tab->update_display_text();
			const Terminal::Screen& current_screen_data = current_tab->get_screen_data();

			if (current_screen_data.m_type != Terminal::ScreenType::NONE)
			{
				// Update the terminal preview display
				DisplayData display_data;
				display_data.m_resource_id = current_screen_data.m_resource_id;
				display_data.m_text = current_screen_data.m_display_text;
				display_data.m_screen_type = current_screen_data.m_type;
				display_data.m_alignment = current_screen_data.m_alignment;

				m_core.get_display_system().update_display(DisplaySystem::View::SCREEN_EDITOR, display_data);
			}
		}
	}

	void ScreenEditorWindow::update_edit_notification()
	{
		const int current_progress = m_internal->m_ui.screen_update_progress_bar->value();
		if (current_progress < SCREEN_EDIT_PROGRESS_STEP_COUNT)
		{
			m_internal->m_ui.screen_update_progress_bar->setValue(current_progress + 1);
		}

		if (m_internal->m_ui.screen_update_progress_bar->value() >= SCREEN_EDIT_PROGRESS_STEP_COUNT)
		{
			reset_edit_notification();
			update_preview();
		}
	}

	void ScreenEditorWindow::reset_edit_notification()
	{
		m_internal->m_edit_timer.stop();
		m_internal->m_ui.screen_update_progress_bar->setValue(0);
		m_internal->m_ui.screen_update_progress_bar->setVisible(false);
	}
}