#pragma once
#include <HuxQt/Scenario/Terminal.h>

#include <QtWidgets/QMainWindow>
#include <QGraphicsView>
#include <QTreeWidgetItem>

namespace HuxApp
{
    class AppCore;
    struct TerminalID;
    class Scenario;

    // Main window
    class HuxQt : public QMainWindow
    {
        Q_OBJECT
    public:
        HuxQt(QWidget* parent = Q_NULLPTR);
        ~HuxQt();

        QGraphicsView* get_graphics_view();
    protected:
        void closeEvent(QCloseEvent* event) override;
    private:
        void init_ui();
        void connect_signals();
        void clear_preview_display();
        void reset_ui();
        void update_title(const QString& text = QString());

        // Menu
        void open_scenario();
        void save_scenario_action();
        void save_scenario_as_action();
        void export_scenario_scripts();
        void import_scenario_scripts();
        void open_preview_config();
        void preview_config_closed();
        void override_text_colors();
        void set_app_theme();

        // Scenario browser
        void edit_level(int level_id);
        void level_changes_accepted();
        void terminal_selected(int level_id, int terminal_id);
        void terminal_opened(int level_id, int terminal_id);
        void screen_item_selected(QTreeWidgetItem* current, QTreeWidgetItem* previous);
        void scenario_modified();

        // Terminal preview
        void display_current_screen();
        void terminal_first_clicked();
        void terminal_prev_clicked();
        void terminal_next_clicked();
        void terminal_last_clicked();
        void terminal_modified(int level_id, int terminal_id);
        void terminals_removed(int level_id, const QList<int>& terminal_ids);

        // Misc.
        void terminal_editor_closed(QObject* object);
        bool close_current_scenario();
        bool save_scenario(const QString& file_name);
        bool export_scenario(const QString& export_path);
        void scenario_loaded(const Scenario& scenario, const QString& path);

        struct Internal;
        std::unique_ptr<Internal> m_internal;

        std::unique_ptr<AppCore> m_core;
    };

}