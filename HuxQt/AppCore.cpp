#include <HuxQt/AppCore.h>

#include <HuxQt/Scenario/ScenarioManager.h>
#include <HuxQt/UI/DisplaySystem.h>

namespace HuxApp
{
    struct AppCore::Internal
    {
        Internal(AppCore& core, HuxQt* main_window)
            : m_main_window(main_window)
            , m_scenario_manager(core)
            , m_display_system(core)
        {}

        HuxQt* m_main_window;

        ScenarioManager m_scenario_manager;
        DisplaySystem m_display_system;
    };

    AppCore::AppCore(HuxQt* main_window)
        : m_internal(std::make_unique<Internal>(*this, main_window))
    {
    }

    AppCore::~AppCore() = default;

    ScenarioManager& AppCore::get_scenario_manager() { return m_internal->m_scenario_manager; }
    const ScenarioManager& AppCore::get_scenario_manager() const { return m_internal->m_scenario_manager; }

    DisplaySystem& AppCore::get_display_system() { return m_internal->m_display_system; }
    const DisplaySystem& AppCore::get_display_system() const { return m_internal->m_display_system; }

    HuxQt* AppCore::get_main_window() { return m_internal->m_main_window; }
}