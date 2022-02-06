#pragma once
#include <Scenario/Terminal.h>

class QStandardItemModel;

namespace HuxApp
{
	class Scenario;
	class Level;

	struct TerminalID;

	struct LevelInfo
	{
		int m_id = -1;
		QString m_name;
		QString m_dir_name;
		QString m_script_name;
	};

	class LevelModel : public QStandardItemModel
	{
		Q_OBJECT
	public:
		enum class TerminalDataRoles
		{
			TERMINAL_ID = Qt::UserRole + 1,
			MODIFIED
		};

		LevelModel(int id, const std::vector<Terminal>& terminals);

		int get_id() const { return m_id; }

		int find_terminal_row(const TerminalID& terminal_id) const;
		const Terminal* get_terminal(const TerminalID& terminal_id) const;

		void add_terminal();
		std::vector<Terminal> copy_terminals(const QModelIndexList& terminal_indices);
		void paste_terminals(const std::vector<Terminal>& terminals, const QModelIndexList& terminal_indices);
		void remove_terminals(const QModelIndexList& terminal_indices);

		QVariant get_custom_data(int row, TerminalDataRoles role) const;
		QVariant get_custom_data(const QStandardItem* item, TerminalDataRoles role) const;

		void set_custom_data(int row, TerminalDataRoles role, const QVariant& data);
		void set_custom_data(QStandardItem* item, TerminalDataRoles role, const QVariant& data);

		void export_level_contents(Level& level) const;

		void clear_modified();
	signals:
		void level_modified(int id);
		void terminals_removed(int level_id, const QList<int>& terminal_ids);
	private:
		void connect_signals();

		QStandardItem* add_terminal_internal(const Terminal& terminal);
		void update_terminal_data(const TerminalID& terminal_id, const Terminal& terminal_data);
		void terminal_item_changed(QStandardItem* item);

		const Terminal* get_terminal_internal(int terminal_id) const;
		const Terminal* get_terminal_internal(const QModelIndex& index) const;

		void level_modified_internal();

		int m_id;
		int m_terminal_id_counter;
		std::unordered_map<int, Terminal> m_terminal_pool;
		bool m_signal_lock;

		friend class ScenarioBrowserModel;
	};

	class ScenarioBrowserModel : public QObject
	{
		Q_OBJECT
	public:
		enum class LevelDataRoles
		{
			LEVEL_ID = Qt::UserRole + 1,
			MODIFIED,
			DIR_NAME,
			SCRIPT_NAME
		};

		ScenarioBrowserModel(QObject* parent = nullptr);

		void set_name(const QString& name) { m_name = name; emit(scenario_name_changed()); }
		const QString& get_name() const { return m_name; }

		const QString& get_file_name() const { return m_file_name; }
		void set_file_name(const QString& file_name) { m_file_name = file_name; }

		const QString& get_path() const { return m_path; }
		void set_path(const QString& path) { m_path = path; }

		bool is_modified() const { return m_modified; }

		void load_scenario(const Scenario& scenario);
		Scenario export_scenario() const;

		QStandardItemModel& get_level_list() { return m_level_list_model; }
		
		const LevelModel* get_level_model(int id) const;
		LevelModel* get_level_model(int id);

		const LevelModel* get_level_model(const QModelIndex& index) const;
		LevelModel* get_level_model(const QModelIndex& index);

		LevelInfo get_level_info(int id) const;

		void add_level();
		void remove_level(const QModelIndex& index);

		bool update_level_data(const LevelInfo& info, QString& error_msg);
		void update_terminal_data(const TerminalID& terminal_id, const Terminal& data);

		void copy_terminals(int level_id, const QModelIndexList& terminal_indices);
		void paste_terminals(int level_id, const QModelIndexList& terminal_indices);

		void clear_modified();

		std::vector<Terminal>& get_terminal_clipboard() { return m_terminal_clipboard; }
	signals:
		void scenario_name_changed();
		void scenario_modified();
		void terminal_modified(int level_id, int terminal_id);
		void terminals_removed(int level_id, const QList<int>& terminal_ids);
	private:
		void clear_internal();

		QStandardItem* add_level_internal(const Level& level);

		QVariant get_level_custom_data(int row, LevelDataRoles role) const;
		QVariant get_level_custom_data(const QStandardItem* item, LevelDataRoles role) const;
		void set_level_custom_data(int row, LevelDataRoles role, const QVariant& data);
		void set_level_custom_data(QStandardItem* item, LevelDataRoles role, const QVariant& data);

		int find_level_row(int level_id) const;

		void scenario_modified_internal();
		void level_modified(int level_id);
		void level_item_changed(QStandardItem* item);

		bool validate_level_name(const LevelInfo& info, QString& error_msg);
		bool validate_level_script_name(const LevelInfo& info, QString& error_msg);
		bool validate_level_folder_name(const LevelInfo& info, QString& error_msg);

		QString m_name;
		QString m_file_name;
		QString m_path;

		int m_level_id_counter = 0;
		bool m_modified = false;

		QStandardItemModel m_level_list_model;
		std::unordered_map<int, LevelModel> m_level_pool;
		std::vector<Terminal> m_terminal_clipboard;
	};
}