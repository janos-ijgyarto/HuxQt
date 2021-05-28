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
    protected:
        void closeEvent(QCloseEvent* event);
    private:
        void init_ui();
        void connect_signals();
        void clear_preview_display();
        void reset_ui();

        // Menu
        void open_scenario();
        void save_scenario_action();
        void export_scenario_scripts();
        void open_preview_config();
        void preview_config_closed();

        // Tree functions
        void scenario_item_selected(QListWidgetItem* current, QListWidgetItem* previous);
        void scenario_item_double_clicked(QListWidgetItem* item);
        void scenario_view_context_menu(const QPoint& point);
        void screen_item_selected(QTreeWidgetItem* current, QTreeWidgetItem* previous);

        // Scenario Editor
        void add_level_action();
        void remove_level_action();

        void copy_terminal_action();
        void paste_terminal_action();

        void scenario_up_clicked();
        void add_terminal_clicked();
        void move_terminal_up_clicked();
        void move_terminal_down_clicked();
        void remove_terminal_clicked();

        // Terminal preview
        void display_current_screen();
        void terminal_first_clicked();
        void terminal_prev_clicked();
        void terminal_next_clicked();
        void terminal_last_clicked();

        // Misc.
        void terminal_editor_closed();
        bool close_current_scenario();
        bool save_scenario();

        struct Internal;
        std::unique_ptr<Internal> m_internal;

        std::unique_ptr<AppCore> m_core;
    };

}