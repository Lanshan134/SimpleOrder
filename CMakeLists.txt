cmake_minimum_required(VERSION 3.16)
project(SimpleOrder VERSION 2.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Sql)
add_executable(SimpleOrder WIN32
    src/app/controller.cpp
    src/app/main.cpp
    src/app/preferences.cpp
    src/domain/entryparser.cpp
    src/domain/item.cpp
    src/domain/plan.cpp
    src/storage/store.cpp
    src/ui/theme/appearance.cpp
    src/ui/widgets/calendar.cpp
    src/ui/widgets/planeditor.cpp
    src/ui/widgets/smartentry.cpp
    src/ui/widgets/tasklist.cpp
    src/ui/widgets/widgettools.cpp
    src/ui/windows/editor.cpp
    src/ui/windows/floating.cpp
    src/ui/windows/manager.cpp
    src/ui/windows/settings.cpp
    src/ui/windows/window.cpp
    tests/entryparser_tests.cpp
    tests/settings_smoke.cpp
    tests/ui_smoke.cpp
    resources/theme.qrc
)
target_include_directories(SimpleOrder PRIVATE src)
add_custom_command(TARGET SimpleOrder POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/theme/style.qss"
        "$<TARGET_FILE_DIR:SimpleOrder>/style.qss"
    VERBATIM
)
target_link_libraries(SimpleOrder PRIVATE Qt5::Widgets Qt5::Sql)
if(MSVC)
 target_compile_options(SimpleOrder PRIVATE /utf-8)
endif()
enable_testing()
add_test(NAME persistence_and_ui COMMAND SimpleOrder --self-test)
if(WIN32)
 set_tests_properties(persistence_and_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
else()
 set_tests_properties(persistence_and_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
