#include <stdafx.h>
#include <UI/ScenarioBrowserView.h>

#include <Scenario/Scenario.h>
#include <Scenario/ScenarioBrowserModel.h>

namespace HuxApp
{
	struct ScenarioBrowserView::Internal
	{
		Ui::ScenarioBrowserView m_ui;
		ScenarioBrowserModel* m_model = nullptr;

		LevelModel* m_opened_level = nullptr;

		void update_view()
		{
			if (m_opened_level)
			{
				// Terminal list, allow multiple selection
				m_ui.scenario_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);

				// Enable the "up" button
				m_ui.scenario_up_button->setEnabled(true);

				// Set the level model for the view
				m_ui.scenario_browser_view->setModel(m_opened_level);

				// Update the path label
				const LevelInfo level_info = m_model->get_level_info(m_opened_level->get_id());
				m_ui.scenario_path_label->setText(QStringLiteral("%1 / %2").arg(m_model->get_name()).arg(level_info.m_name));
			}
			else
			{
				// Level list, only allow single selection
				m_ui.scenario_browser_view->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

				// Disable the "up" button
				m_ui.scenario_up_button->setEnabled(false);

				// Set the level list model for the view
				m_ui.scenario_browser_view->setModel(&m_model->get_level_list());

				// Reset the path label
				m_ui.scenario_path_label->setText(m_model->get_name());
			}

			update_buttons();
		}

		void open_level(const QModelIndex& index)
		{
			m_opened_level = m_model->get_level_model(index);
			update_view();
		}

		void show_level_list()
		{
			m_opened_level = nullptr;
			update_view();
		}		

		void update_buttons()
		{
			// Enable remove only if we have something selected
			m_ui.remove_button->setEnabled(m_ui.scenario_browser_view->selectionModel()->hasSelection());
		}
	};

	ScenarioBrowserView::ScenarioBrowserView(QWidget* parent)
		: m_internal(std::make_unique<Internal>())
	{
		m_internal->m_ui.setupUi(this);

		init_ui();
		connect_signals();
	}

	ScenarioBrowserView::~ScenarioBrowserView() = default;

	void ScenarioBrowserView::set_model(ScenarioBrowserModel* model) 
	{
		m_internal->m_model = model; 
		m_internal->m_opened_level = nullptr;
		m_internal->update_view();
	}

	void ScenarioBrowserView::init_ui()
	{
		m_internal->m_ui.scenario_browser_view->setEditTriggers(QAbstractItemView::EditTrigger::NoEditTriggers);
		m_internal->m_ui.scenario_browser_view->setDragDropMode(QAbstractItemView::DragDropMode::InternalMove);

		m_internal->m_ui.scenario_up_button->setEnabled(false);

		// Set the button icons
		m_internal->m_ui.scenario_up_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogToParent));

		m_internal->m_ui.add_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileIcon));
		m_internal->m_ui.remove_button->setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_TrashIcon));
	}

	void ScenarioBrowserView::connect_signals()
	{
		connect(m_internal->m_ui.scenario_up_button, &QPushButton::clicked, this, &ScenarioBrowserView::up_button_clicked);
		connect(m_internal->m_ui.add_button, &QPushButton::clicked, this, &ScenarioBrowserView::add_button_clicked);
		connect(m_internal->m_ui.remove_button, &QPushButton::clicked, this, &ScenarioBrowserView::remove_button_clicked);

		connect(m_internal->m_ui.scenario_browser_view, &QListView::clicked, this, &ScenarioBrowserView::scenario_item_clicked);
		connect(m_internal->m_ui.scenario_browser_view, &QListView::doubleClicked, this, &ScenarioBrowserView::scenario_item_double_clicked);

		m_internal->m_ui.scenario_browser_view->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_internal->m_ui.scenario_browser_view, &QListView::customContextMenuRequested, this, &ScenarioBrowserView::scenario_view_context_menu);
	}

	bool ScenarioBrowserView::in_level() const { return m_internal->m_opened_level; }

	void ScenarioBrowserView::scenario_item_clicked(const QModelIndex& index)
	{
		m_internal->update_buttons();

		if (in_level())
		{
			terminal_item_selected(index);
		}
	}

	void ScenarioBrowserView::scenario_item_double_clicked(const QModelIndex& index)
	{
		if (in_level())
		{
			terminal_item_double_clicked(index);
		}
		else
		{
			// Open the selected level
			m_internal->open_level(index);
		}
	}

	void ScenarioBrowserView::scenario_view_context_menu(const QPoint& point)
	{
		// Check if we have an item selected
		QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
		const QModelIndex selected_item = selection_model->hasSelection() ? m_internal->m_ui.scenario_browser_view->indexAt(point) : QModelIndex();
		if (in_level())
		{
			QMenu context_menu;

			// Add copy only if an item is selected
			if (selected_item.isValid())
			{
				// TODO: keyboard shortcut!
				context_menu.addAction("Copy Terminal(s)", this, &ScenarioBrowserView::copy_terminals_action);
			}

			// Add paste, enable only if applicable
			// TODO: keyboard shortcut!
			QAction* paste_action = context_menu.addAction("Paste Terminal(s)", this, &ScenarioBrowserView::paste_terminals_action);
			paste_action->setEnabled(!m_internal->m_model->get_terminal_clipboard().empty());

			const QPoint global_pos = m_internal->m_ui.scenario_browser_view->mapToGlobal(point);
			context_menu.exec(global_pos);
		}
		else
		{
			// Allow editing if a level was selected
			if (selected_item.isValid())
			{
				QMenu context_menu;
				context_menu.addAction("Edit Level", this, &ScenarioBrowserView::edit_level_action);

				const QPoint global_pos = m_internal->m_ui.scenario_browser_view->mapToGlobal(point);
				context_menu.exec(global_pos);
			}
		}
	}

	void ScenarioBrowserView::up_button_clicked()
	{
		m_internal->show_level_list();
	}

	void ScenarioBrowserView::add_button_clicked()
	{
		if (in_level())
		{
			add_terminal();
		}
		else
		{
			add_level();
		}
	}

	void ScenarioBrowserView::remove_button_clicked()
	{
		if (in_level())
		{
			remove_terminals();
		}
		else
		{
			remove_level();
		}
	}

	void ScenarioBrowserView::terminal_item_selected(const QModelIndex& index)
	{
		const int terminal_id = m_internal->m_opened_level->get_custom_data(index.row(), LevelModel::TerminalDataRoles::TERMINAL_ID).toInt();
		emit(terminal_selected(m_internal->m_opened_level->get_id(), terminal_id));
	}

	void ScenarioBrowserView::terminal_item_double_clicked(const QModelIndex& index)
	{
		const int terminal_id = m_internal->m_opened_level->get_custom_data(index.row(), LevelModel::TerminalDataRoles::TERMINAL_ID).toInt();
		emit(terminal_opened(m_internal->m_opened_level->get_id(), terminal_id));
	}

	void ScenarioBrowserView::add_level()
	{
		m_internal->m_model->add_level();
	}

	void ScenarioBrowserView::add_terminal()
	{
		m_internal->m_opened_level->add_terminal();
	}

	void ScenarioBrowserView::remove_level()
	{
		QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
		if (selection_model)
		{
			const QModelIndexList selected_indices = selection_model->selectedIndexes();
			if (!selected_indices.isEmpty())
			{
				const QModelIndex& selected_level_index = selected_indices.front();
				m_internal->m_model->remove_level(selected_level_index);
			}
		}
	}

	void ScenarioBrowserView::remove_terminals()
	{
		if (in_level())
		{
			QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
			if (selection_model)
			{
				const QModelIndexList selected_indices = selection_model->selectedIndexes();
				if (!selected_indices.isEmpty())
				{

					m_internal->m_opened_level->remove_terminals(selected_indices);
				}
			}
		}
	}

	void ScenarioBrowserView::edit_level_action()
	{
		QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
		if (selection_model)
		{
			const QModelIndexList selected_indices = selection_model->selectedIndexes();
			if (!selected_indices.isEmpty())
			{
				const QModelIndex& selected_level_index = selected_indices.front();
				LevelModel* selected_level = m_internal->m_model->get_level_model(selected_level_index);
				if (selected_level)
				{
					emit(edit_level(selected_level->get_id()));
				}
			}
		}
	}

	void ScenarioBrowserView::copy_terminals_action()
	{
		if (in_level())
		{
			QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
			if (selection_model)
			{
				const QModelIndexList selected_indices = selection_model->selectedIndexes();
				if (!selected_indices.isEmpty())
				{
					m_internal->m_model->copy_terminals(m_internal->m_opened_level->get_id(), selected_indices);
				}
			}
		}
	}

	void ScenarioBrowserView::paste_terminals_action()
	{
		if (in_level())
		{
			QItemSelectionModel* selection_model = m_internal->m_ui.scenario_browser_view->selectionModel();
			if (selection_model)
			{
				const QModelIndexList selected_indices = selection_model->selectedIndexes(); // No need to check for empty selection here
				m_internal->m_model->paste_terminals(m_internal->m_opened_level->get_id(), selected_indices);
			}
		}
	}
}