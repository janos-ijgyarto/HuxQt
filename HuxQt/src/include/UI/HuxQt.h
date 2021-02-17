#pragma once
#include <QtWidgets/QMainWindow>
#include "Scenario/Terminal.h"

namespace HuxApp
{
    class AppCore;

    // Main window
    class HuxQt : public QMainWindow
    {
        Q_OBJECT
    public:
        HuxQt(QWidget* parent = Q_NULLPTR);
        ~HuxQt();

        QGraphicsView* get_graphics_view();
        
        bool add_level(const QString& level_name, const QString& level_dir_name);

        bool save_terminal_info(const QString& terminal_path, const Terminal& terminal_info);

        void screen_edit_tab_closed(QTreeWidgetItem* screen_item);
        void save_screen(QTreeWidgetItem* screen_item, const Terminal::Screen& screen_data);

        QString get_screen_path(QTreeWidgetItem* screen_item);
    protected:
        void closeEvent(QCloseEvent* event);
    private:
        void init_ui();
        void connect_signals();

        void reset_ui();
        void reset_scenario_ui();
        void reset_terminal_ui();

        // Menu
        void open_scenario();
        bool save_scenario();
        void export_scenario_scripts();
        void open_preview_config();
        void preview_config_closed();

        // Tree functions
        void scenario_tree_clicked(QTreeWidgetItem* item, int column);
        void scenario_tree_double_clicked(QTreeWidgetItem* item, int column);
        void terminal_node_double_clicked(QTreeWidgetItem* item);
        void screen_node_selected(QTreeWidgetItem* item);
        void screen_node_double_clicked(QTreeWidgetItem* item);
        void scenario_tree_context_menu(const QPoint& point);
        void scenario_tree_item_removed(QTreeWidgetItem* item);

        // Scenario Editor
        void add_level_action();
        void remove_level_action();
        void add_terminal_action();
        void move_terminal_action();
        void remove_terminal_action();
        void add_screen_action();
        void move_screen_action();
        void remove_screen_action();
        void clear_group_action();

        // Terminal preview
        void display_current_screen();
        void terminal_first_clicked();
        void terminal_prev_clicked();
        void terminal_next_clicked();
        void terminal_last_clicked();
        void update_navigation_buttons(int current_index, int screen_count);

        // Misc.
        void scenario_edited();
        void clear_scenario_edited();
        bool close_current_scenario();

        struct Internal;
        std::unique_ptr<Internal> m_internal;

        std::unique_ptr<AppCore> m_core;
    };

}