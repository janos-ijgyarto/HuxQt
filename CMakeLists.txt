cmake_minimum_required(VERSION 3.16)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "In-tree build attempted, aborting build.")
endif()

project(HuxQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/HuxQt/forms)

qt_add_executable(HuxQt)

add_subdirectory(HuxQt)

get_property("HUXQT_SOURCES" TARGET HuxQt PROPERTY SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${HUXQT_SOURCES})

target_include_directories(HuxQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(HuxQt PRIVATE Qt6::Widgets)

set_target_properties(HuxQt PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)