#include <HuxQt/Scenario/ScenarioBrowserModel.h>

#include <HuxQt/Scenario/Scenario.h>

#include <HuxQt/Utils/Utilities.h>

#include <QApplication>
#include <QStyle>

namespace HuxApp
{
	namespace
	{
		QString get_terminal_label(int terminal_id, const Terminal& terminal_data)
		{
			return terminal_data.get_name().isEmpty() ? QStringLiteral("TERMINAL (%1)").arg(terminal_id) : terminal_data.get_name();
		}

		class LevelStandardItem : public QStandardItem
		{
		public:
			LevelStandardItem() = default;
			LevelStandardItem(int level_id, const Level& level_data) : LevelStandardItem(level_id, level_data.get_name(), level_data.get_dir_name(), level_data.get_script_name())
			{
			}

			void set_custom_data(const QVariant& data, ScenarioBrowserModel::LevelDataRoles role) { QStandardItem::setData(data, Utils::to_integral(role)); }

			QVariant get_custom_data(ScenarioBrowserModel::LevelDataRoles role) const { return QStandardItem::data(Utils::to_integral(role)); }

			QStandardItem* clone() const override { return new LevelStandardItem(*this); }

			void read(QDataStream& in) override
			{
				QStandardItem::read(in);

				// Expecting this to be a move operation, so the level is modified
				set_custom_data(true, ScenarioBrowserModel::LevelDataRoles::MODIFIED);
			}

			QVariant data(int role = Qt::UserRole + 1) const override
			{
				switch (role)
				{
				case Qt::FontRole:
				{
					// Set font to bold if the level has been modified
					const bool is_modified = get_custom_data(ScenarioBrowserModel::LevelDataRoles::MODIFIED).toBool();
					QFont current_font;
					if (is_modified)
					{
						current_font.setBold(true);
					}
					else
					{
						current_font.setBold(false);
					}
					return current_font;
				}
				break;
				}

				return QStandardItem::data(role);
			}

			Level export_level() const
			{
				Level exported_level;
				exported_level.set_name(text());
				exported_level.set_script_name(get_custom_data(ScenarioBrowserModel::LevelDataRoles::SCRIPT_NAME).toString());
				exported_level.set_dir_name(get_custom_data(ScenarioBrowserModel::LevelDataRoles::DIR_NAME).toString());
				return exported_level;
			}

		private:
			LevelStandardItem(int level_id, const QString& level_name, const QString& level_dir_name, const QString& level_script_name, bool modified = false)
				: QStandardItem(level_name)
			{
				set_custom_data(level_id, ScenarioBrowserModel::LevelDataRoles::LEVEL_ID);
				set_custom_data(level_dir_name, ScenarioBrowserModel::LevelDataRoles::DIR_NAME);
				set_custom_data(level_script_name, ScenarioBrowserModel::LevelDataRoles::SCRIPT_NAME);
				set_custom_data(modified, ScenarioBrowserModel::LevelDataRoles::MODIFIED);

				setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_FileDialogStart));

				// Allow drag, but prevent drop
				setFlags((flags() | Qt::ItemFlag::ItemIsDragEnabled) & ~Qt::ItemFlag::ItemIsDropEnabled);
			}

			LevelStandardItem(const LevelStandardItem& other)
				: LevelStandardItem(other.get_custom_data(ScenarioBrowserModel::LevelDataRoles::LEVEL_ID).toInt()
					, other.text()
					, other.get_custom_data(ScenarioBrowserModel::LevelDataRoles::DIR_NAME).toString()
					, other.get_custom_data(ScenarioBrowserModel::LevelDataRoles::SCRIPT_NAME).toString()
					, true) // Set modified to true, since we are sure that this was a newly added item
			{
			}
		};

		class TerminalStandardItem : public QStandardItem
		{
		public:
			TerminalStandardItem() = default;
			TerminalStandardItem(int terminal_id, const Terminal& terminal_data) : TerminalStandardItem(terminal_id, get_terminal_label(terminal_id, terminal_data))
			{
			}

			QStandardItem* clone() const override { return new TerminalStandardItem(*this); }

			void read(QDataStream& in) override
			{
				QStandardItem::read(in);

				// Expecting this to be a move operation, so the terminal is modified
				set_custom_data(true, LevelModel::TerminalDataRoles::MODIFIED);
			}

			void set_custom_data(const QVariant& data, LevelModel::TerminalDataRoles role) { QStandardItem::setData(data, Utils::to_integral(role)); }

			QVariant get_custom_data(LevelModel::TerminalDataRoles role) const { return QStandardItem::data(Utils::to_integral(role)); }

			QVariant data(int role = Qt::UserRole + 1) const override
			{
				switch (role)
				{
				case Qt::FontRole:
				{
					// Set font to bold if the terminal has been modified
					const bool is_modified = get_custom_data(LevelModel::TerminalDataRoles::MODIFIED).toBool();
					QFont current_font;
					current_font.setBold(is_modified);
					return current_font;
				}
				break;
				}

				return QStandardItem::data(role);
			}

		private:
			TerminalStandardItem(int terminal_id, const QString& terminal_name)
				: QStandardItem(terminal_name)
			{
				set_custom_data(terminal_id, LevelModel::TerminalDataRoles::TERMINAL_ID);
				setIcon(QApplication::style()->standardIcon(QStyle::StandardPixmap::SP_ComputerIcon));

				// Allow drag, but prevent drop
				setFlags((flags() | Qt::ItemFlag::ItemIsDragEnabled) & ~Qt::ItemFlag::ItemIsDropEnabled);
			}

			TerminalStandardItem(const TerminalStandardItem& other)
				: TerminalStandardItem(other.get_custom_data(LevelModel::TerminalDataRoles::TERMINAL_ID).toInt()
					, other.text())
			{
			}
		};
	}

	LevelModel::LevelModel(int id, const std::vector<Terminal>& terminals)
		: m_id(id)
		, m_terminal_id_counter(0)
		, m_signal_lock(false)
	{
		setColumnCount(1);

		setItemPrototype(new TerminalStandardItem());

		// Add an item for each terminal
		for (const Terminal& current_terminal : terminals)
		{
			add_terminal_internal(current_terminal);
		}

		connect_signals();
	}

	int LevelModel::find_terminal_row(const TerminalID& terminal_id) const
	{
		// Make sure the level and terminal IDs are valid
		if ((terminal_id.m_level_id == m_id) && (m_terminal_pool.find(terminal_id.m_terminal_id) != m_terminal_pool.end()))
		{
			for (int current_row = 0; current_row < rowCount(); ++current_row)
			{
				if (get_custom_data(current_row, TerminalDataRoles::TERMINAL_ID).toInt() == terminal_id.m_terminal_id)
				{
					return current_row;
				}
			}
		}
		return -1;
	}

	const Terminal* LevelModel::get_terminal(const TerminalID& terminal_id) const
	{
		// Make sure the level and terminal IDs are valid
		if (terminal_id.m_level_id == m_id)
		{
			return get_terminal_internal(terminal_id.m_terminal_id);
		}
		return nullptr;
	}

	void LevelModel::add_terminal()
	{
		QStandardItem* new_terminal_item = add_terminal_internal(Terminal());
		set_custom_data(new_terminal_item, TerminalDataRoles::MODIFIED, true);
	}

	std::vector<Terminal> LevelModel::copy_terminals(const QModelIndexList& terminal_indices)
	{
		std::vector<Terminal> selected_terminals;
		for (const QModelIndex& current_index : terminal_indices)
		{
			if (const Terminal* current_terminal = get_terminal_internal(current_index))
			{
				selected_terminals.push_back(*current_terminal);
			}
		}
		return selected_terminals;
	}

	void LevelModel::paste_terminals(const std::vector<Terminal>& terminals, const QModelIndexList& terminal_indices)
	{
		// TODO: paste at the selection
		m_signal_lock = true; // Lock signals so we only send one update
		for (const Terminal& current_terminal : terminals)
		{
			QStandardItem* pasted_terminal_item = add_terminal_internal(current_terminal);
			set_custom_data(pasted_terminal_item, TerminalDataRoles::MODIFIED, true);
		}
		level_modified_internal();
		m_signal_lock = false;
	}

	void LevelModel::remove_terminals(const QModelIndexList& terminal_indices)
	{
		QModelIndexList reverse_sorted_indices = terminal_indices;
		if (reverse_sorted_indices.size() > 1)
		{
			// Have to reverse sort the indices so we delete the last row first (this preserves the row indices as we delete the rows)
			std::sort(reverse_sorted_indices.begin(), reverse_sorted_indices.end());
		}

		QList<int> removed_indices;

		for (const QModelIndex& current_removed_index : reverse_sorted_indices)
		{
			const int removed_terminal_id = get_custom_data(current_removed_index.row(), TerminalDataRoles::TERMINAL_ID).toInt();
			removeRow(current_removed_index.row());

			m_terminal_pool.erase(removed_terminal_id);

			removed_indices << removed_terminal_id;
		}

		// Signal listeners
		emit(terminals_removed(m_id, removed_indices));
		level_modified_internal();
	}

	QVariant LevelModel::get_custom_data(int row, TerminalDataRoles role) const { return data(index(row, 0), Utils::to_integral(role)); }

	QVariant LevelModel::get_custom_data(const QStandardItem* item, TerminalDataRoles role) const
	{
		if (item)
		{
			return item->data(Utils::to_integral(role));
		}
		return QVariant();
	}

	void LevelModel::set_custom_data(int row, TerminalDataRoles role, const QVariant& data) { setData(index(row, 0), data, Utils::to_integral(role)); }

	void LevelModel::set_custom_data(QStandardItem* item, TerminalDataRoles role, const QVariant& data)
	{
		if (item)
		{
			item->setData(data, Utils::to_integral(role));
		}
	}

	void LevelModel::export_level_contents(Level& level) const
	{
		std::vector<Terminal> exported_terminals(rowCount());
		auto current_exported_terminal_it = exported_terminals.begin();
		for (int current_row = 0; current_row < rowCount(); ++current_row)
		{
			// Get the terminal ID and copy the contents
			Terminal& current_exported_terminal = *current_exported_terminal_it;
			const TerminalStandardItem* current_terminal_item = static_cast<TerminalStandardItem*>(item(current_row));
			
			const int current_terminal_id = current_terminal_item->get_custom_data(TerminalDataRoles::TERMINAL_ID).toInt();
			const Terminal* terminal_data = get_terminal_internal(current_terminal_id);
			current_exported_terminal = *terminal_data;

			++current_exported_terminal_it;
		}

		level.set_terminals(exported_terminals);
	}

	void LevelModel::clear_modified()
	{
		for (int current_row = 0; current_row < rowCount(); ++current_row)
		{
			set_custom_data(current_row, TerminalDataRoles::MODIFIED, false);
		}
	}

	void LevelModel::connect_signals()
	{
		// Connect to the item changed signal (using this to detect drag & drop)
		connect(this, &QStandardItemModel::itemChanged, this, &LevelModel::terminal_item_changed);
	}

	QStandardItem* LevelModel::add_terminal_internal(const Terminal& terminal)
	{
		// Add a new terminal to the pool (use array access operator to generate the entry)
		const int terminal_id = m_terminal_id_counter++;
		m_terminal_pool[terminal_id] = terminal;

		// Add item to the model
		TerminalStandardItem* terminal_standard_item = new TerminalStandardItem(terminal_id, terminal);
		appendRow(terminal_standard_item);

		return terminal_standard_item;
	}

	void LevelModel::update_terminal_data(const TerminalID& terminal_id, const Terminal& terminal_data)
	{
		const int terminal_row = find_terminal_row(terminal_id);
		if (terminal_row >= 0)
		{
			m_terminal_pool[terminal_id.m_terminal_id] = terminal_data;

			QStandardItem* terminal_item = item(terminal_row);
			terminal_item->setText(get_terminal_label(terminal_id.m_terminal_id, terminal_data));

			set_custom_data(terminal_row, TerminalDataRoles::MODIFIED, true);
		}
	}

	void LevelModel::terminal_item_changed(QStandardItem* item)
	{
		if (m_signal_lock)
		{
			return;
		}

		if (get_custom_data(item, TerminalDataRoles::MODIFIED).toBool())
		{
			level_modified_internal();
		}
	}

	const Terminal* LevelModel::get_terminal_internal(int terminal_id) const
	{
		auto terminal_it = m_terminal_pool.find(terminal_id);
		if (terminal_it != m_terminal_pool.end())
		{
			return &terminal_it->second;
		}
		return nullptr;
	}

	const Terminal* LevelModel::get_terminal_internal(const QModelIndex& index) const
	{
		const int terminal_id = get_custom_data(index.row(), TerminalDataRoles::TERMINAL_ID).toInt();
		return get_terminal_internal(terminal_id);
	}

	void LevelModel::level_modified_internal()
	{
		emit(level_modified(m_id));
	}

	ScenarioBrowserModel::ScenarioBrowserModel(QObject* parent)
		: QObject(parent)
	{
		m_level_list_model.setItemPrototype(new LevelStandardItem());

		// Connect to the item changed signal (using this to detect drag & drop)
		connect(&m_level_list_model, &QStandardItemModel::itemChanged, this, &ScenarioBrowserModel::level_item_changed);
	}

	void ScenarioBrowserModel::load_scenario(const Scenario& scenario)
	{
		// First clear the current model
		clear_internal();

		m_name = scenario.get_name();

		m_level_list_model.setColumnCount(1);

		for (const Level& current_level : scenario.get_levels())
		{
			add_level_internal(current_level);
		}
	}

	Scenario ScenarioBrowserModel::export_scenario() const
	{
		Scenario exported_scenario;
		exported_scenario.set_name(m_name);

		std::vector<Level> exported_levels(m_level_list_model.rowCount());
		auto current_exported_level_it = exported_levels.begin();
		for (int current_level_row = 0; current_level_row < m_level_list_model.rowCount(); ++current_level_row)
		{
			// First initialize the level attributes
			Level& current_exported_level = *current_exported_level_it;
			const LevelStandardItem* current_level_item = static_cast<LevelStandardItem*>(m_level_list_model.item(current_level_row));
			current_exported_level = current_level_item->export_level();

			// Get the level contents from the model
			const LevelModel* current_level_model = get_level_model(current_level_item->get_custom_data(LevelDataRoles::LEVEL_ID).toInt());
			current_level_model->export_level_contents(current_exported_level);

			++current_exported_level_it;
		}

		exported_scenario.set_levels(exported_levels);
		return exported_scenario;
	}

	const LevelModel* ScenarioBrowserModel::get_level_model(int id) const
	{
		auto level_model_it = m_level_pool.find(id);
		if (level_model_it != m_level_pool.end())
		{
			return &(level_model_it->second);
		}

		return nullptr;
	}

	LevelModel* ScenarioBrowserModel::get_level_model(int id)
	{
		return const_cast<LevelModel*>(const_cast<const ScenarioBrowserModel*>(this)->get_level_model(id));
	}

	const LevelModel* ScenarioBrowserModel::get_level_model(const QModelIndex& index) const
	{
		// Make sure the index is valid
		if (m_level_list_model.itemFromIndex(index))
		{
			return get_level_model(get_level_custom_data(index.row(), LevelDataRoles::LEVEL_ID).toInt());
		}
		return nullptr;
	}

	LevelModel* ScenarioBrowserModel::get_level_model(const QModelIndex& index)
	{
		return const_cast<LevelModel*>(const_cast<const ScenarioBrowserModel*>(this)->get_level_model(index));
	}

	LevelInfo ScenarioBrowserModel::get_level_info(int id) const
	{
		LevelInfo level_info;
		const int level_row = find_level_row(id);
		if (level_row >= 0)
		{
			level_info.m_id = id;
			level_info.m_name = m_level_list_model.item(level_row)->text();
			level_info.m_dir_name = get_level_custom_data(level_row, LevelDataRoles::DIR_NAME).toString();
			level_info.m_script_name = get_level_custom_data(level_row, LevelDataRoles::SCRIPT_NAME).toString();
		}

		return level_info;
	}

	void ScenarioBrowserModel::add_level()
	{
		// Create new unique level name and folder name (start with template)
		LevelInfo new_level_info;
		new_level_info.m_id = m_level_id_counter + 1;
		new_level_info.m_name = QStringLiteral("New Level");
		new_level_info.m_script_name = new_level_info.m_name;
		new_level_info.m_dir_name = QStringLiteral("New_Level");

		// Check to make sure it doesn't collide with existing levels, iterate until we have no collisions
		QString error_msg;
		int duplicate_count = 1;
		while (!validate_level_name(new_level_info, error_msg) && !validate_level_folder_name(new_level_info, error_msg))
		{
			new_level_info.m_name = QStringLiteral("New Level %1").arg(duplicate_count);
			new_level_info.m_dir_name = QStringLiteral("New_Level_%1").arg(duplicate_count);
			++duplicate_count;
		}

		// Create an empty level object
		Level new_level;
		new_level.set_name(new_level_info.m_name);
		new_level.set_script_name(new_level_info.m_script_name);
		new_level.set_dir_name(new_level_info.m_dir_name);

		QStandardItem* new_level_item = add_level_internal(new_level);
		set_level_custom_data(new_level_item, LevelDataRoles::MODIFIED, true);
	}

	void ScenarioBrowserModel::remove_level(const QModelIndex& index)
	{
		if (const LevelModel* removed_level = get_level_model(index))
		{
			m_level_list_model.removeRow(index.row());
			m_level_pool.erase(removed_level->get_id());
			scenario_modified_internal();
		}
	}

	bool ScenarioBrowserModel::update_level_data(const LevelInfo& info, QString& error_msg)
	{
		// Validate the incoming data
		if (validate_level_name(info, error_msg) && validate_level_script_name(info, error_msg) && validate_level_folder_name(info, error_msg))
		{
			// All validation passed, update the level item
			const int level_row = find_level_row(info.m_id);
			if (level_row >= 0)
			{
				m_level_list_model.item(level_row)->setText(info.m_name);
				set_level_custom_data(level_row, LevelDataRoles::DIR_NAME, info.m_dir_name);
				set_level_custom_data(level_row, LevelDataRoles::SCRIPT_NAME, info.m_script_name);

				level_modified(info.m_id);
				return true;
			}
			else
			{
				error_msg = QStringLiteral("level not found");
			}
		}

		return false;
	}

	void ScenarioBrowserModel::update_terminal_data(const TerminalID& terminal_id, const Terminal& data)
	{
		if (terminal_id.is_valid())
		{
			if (LevelModel* level_model = get_level_model(terminal_id.m_level_id))
			{
				level_model->update_terminal_data(terminal_id, data);
				terminal_modified(terminal_id.m_level_id, terminal_id.m_terminal_id);
			}
		}
	}

	void ScenarioBrowserModel::copy_terminals(int level_id, const QModelIndexList& terminal_indices)
	{
		if (LevelModel* level_model = get_level_model(level_id))
		{
			m_terminal_clipboard = level_model->copy_terminals(terminal_indices);
		}
	}

	void ScenarioBrowserModel::paste_terminals(int level_id, const QModelIndexList& terminal_indices)
	{
		if (LevelModel* level_model = get_level_model(level_id))
		{
			level_model->paste_terminals(m_terminal_clipboard, terminal_indices);
		}
	}

	void ScenarioBrowserModel::clear_modified()
	{
		// Clear the flag in the model items
		for (int current_row = 0; current_row < m_level_list_model.rowCount(); ++current_row)
		{
			set_level_custom_data(current_row, LevelDataRoles::MODIFIED, false);
		}

		// Clear the state in the level objects
		for (auto& current_level_pair : m_level_pool)
		{
			current_level_pair.second.clear_modified();
		}

		// Clear the flag
		m_modified = false;
	}

	void ScenarioBrowserModel::clear_internal()
	{
		// Clear the models and the clipboard
		m_level_pool.clear();
		m_level_list_model.clear();
		m_terminal_clipboard.clear();

		// Reset the ID counter (IDs start from 1, 0 is considered an invalid ID)
		m_level_id_counter = 0;

		// Clear the flag
		m_modified = false;
	}

	QStandardItem* ScenarioBrowserModel::add_level_internal(const Level& level)
	{
		// Prepare a model for the level
		const int level_id = m_level_id_counter++;
		LevelModel& level_model = m_level_pool.emplace(std::piecewise_construct, std::forward_as_tuple(level_id), std::forward_as_tuple(level_id, level.get_terminals())).first->second;

		// Add an item to the level list model
		LevelStandardItem* level_standard_item = new LevelStandardItem(level_id, level);
		m_level_list_model.appendRow(level_standard_item);

		connect(&level_model, &LevelModel::level_modified, this, &ScenarioBrowserModel::level_modified);
		connect(&level_model, &LevelModel::terminals_removed, this, &ScenarioBrowserModel::terminals_removed);

		return level_standard_item;
	}

	QVariant ScenarioBrowserModel::get_level_custom_data(int row, LevelDataRoles role) const { return m_level_list_model.data(m_level_list_model.index(row, 0), Utils::to_integral(role)); }

	QVariant ScenarioBrowserModel::get_level_custom_data(const QStandardItem* item, LevelDataRoles role) const 
	{
		if (item)
		{
			return item->data(Utils::to_integral(role));
		}

		return QVariant();
	}

	void ScenarioBrowserModel::set_level_custom_data(int row, LevelDataRoles role, const QVariant& data) { m_level_list_model.setData(m_level_list_model.index(row, 0), data, Utils::to_integral(role)); }

	void ScenarioBrowserModel::set_level_custom_data(QStandardItem* item, LevelDataRoles role, const QVariant& data) 
	{
		if (item)
		{
			item->setData(data, Utils::to_integral(role));
		}
	}

	int ScenarioBrowserModel::find_level_row(int level_id) const
	{
		for (int current_row = 0; current_row < m_level_list_model.rowCount(); ++current_row)
		{
			if (get_level_custom_data(current_row, LevelDataRoles::LEVEL_ID).toInt() == level_id)
			{
				return current_row;
			}
		}
		return -1;
	}

	void ScenarioBrowserModel::scenario_modified_internal()
	{
		m_modified = true;
		emit(scenario_modified());
	}

	void ScenarioBrowserModel::level_modified(int level_id)
	{
		const int level_row = find_level_row(level_id);
		if (level_row >= 0)
		{
			// Setting the level data will also signal the scenario change
			set_level_custom_data(level_row, LevelDataRoles::MODIFIED, true);
		}
	}

	void ScenarioBrowserModel::level_item_changed(QStandardItem* item)
	{
		if (get_level_custom_data(item, LevelDataRoles::MODIFIED).toBool())
		{
			scenario_modified_internal();
		}
	}

	bool ScenarioBrowserModel::validate_level_name(const LevelInfo& info, QString& error_msg)
	{
		if (info.m_name.isEmpty())
		{
			error_msg = QStringLiteral("level name must not be empty");
			return false;
		}
		// Make sure the name is unique
		QList<QStandardItem*> matched_levels = m_level_list_model.findItems(info.m_name);
		if (!matched_levels.isEmpty())
		{
			for (QStandardItem* current_level_item : matched_levels)
			{
				if (get_level_custom_data(current_level_item->row(), LevelDataRoles::LEVEL_ID).toInt() != info.m_id)
				{
					// Found a level other than the one we just edited having the same name
					error_msg = QStringLiteral("level name must be unique");
					return false;
				}
			}
		}
		return true;
	}

	bool ScenarioBrowserModel::validate_level_script_name(const LevelInfo& info, QString& error_msg)
	{
		if (info.m_script_name.isEmpty())
		{
			error_msg = QStringLiteral("script name must not be empty");
			return false;
		}

		return true;
	}

	bool ScenarioBrowserModel::validate_level_folder_name(const LevelInfo& info, QString& error_msg)
	{
		if (info.m_dir_name.isEmpty())
		{
			error_msg = QStringLiteral("folder name must not be empty");
			return false;
		}

		// Make sure the folder name is unique
		for (int current_row = 0; current_row < m_level_list_model.rowCount(); ++current_row)
		{
			const int current_level_id = get_level_custom_data(current_row, LevelDataRoles::LEVEL_ID).toInt();
			const QString current_level_folder_name = get_level_custom_data(current_row, LevelDataRoles::DIR_NAME).toString();
			if ((current_level_id != info.m_id) && (current_level_folder_name == info.m_dir_name))
			{
				// Found a level other than the one we just edited having the same folder name
				error_msg = QStringLiteral("folder name must be unique");
				return false;
			}
		}
		return true;
	}
}