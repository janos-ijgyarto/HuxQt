add_subdirectory(Scenario)
add_subdirectory(UI)
add_subdirectory(Utils)
add_subdirectory(resources)

target_sources(${PROJECT_NAME}
    PRIVATE
    AppCore.h
    AppCore.cpp
    main.cpp
   )