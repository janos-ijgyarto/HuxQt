#pragma once

namespace HuxApp
{
	class ScenarioManager;
	class DisplaySystem;
	class HuxQt;

	// App core, serves as the "root" of the app backend (users can access other subsystems of the app through this object)
	class AppCore
	{
	public:
		AppCore(HuxQt* main_window);
		~AppCore();

		ScenarioManager& get_scenario_manager();
		const ScenarioManager& get_scenario_manager() const;

		DisplaySystem& get_display_system();
		const DisplaySystem& get_display_system() const;

		HuxQt* get_main_window();

	private:
		struct Internal;
		std::unique_ptr<Internal> m_internal;
	};
}