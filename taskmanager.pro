QT += widgets sql
CONFIG += c++17
TARGET = SimpleOrder
VERSION = 2.1.0
INCLUDEPATH += $$PWD/src

SOURCES += \
    src/app/controller.cpp \
    src/app/main.cpp \
    src/app/preferences.cpp \
    src/domain/entryparser.cpp \
    src/domain/item.cpp \
    src/domain/plan.cpp \
    src/storage/store.cpp \
    src/ui/theme/appearance.cpp \
    src/ui/widgets/calendar.cpp \
    src/ui/widgets/planeditor.cpp \
    src/ui/widgets/smartentry.cpp \
    src/ui/widgets/tasklist.cpp \
    src/ui/widgets/widgettools.cpp \
    src/ui/windows/editor.cpp \
    src/ui/windows/floating.cpp \
    src/ui/windows/manager.cpp \
    src/ui/windows/settings.cpp \
    src/ui/windows/window.cpp \
    tests/entryparser_tests.cpp \
    tests/settings_smoke.cpp \
    tests/ui_smoke.cpp

HEADERS += \
    src/app/controller.h \
    src/domain/entryparser.h \
    src/domain/item.h \
    src/storage/store.h \
    src/ui/theme/appearance.h \
    src/ui/theme/theme.h \
    src/ui/widgets/calendar.h \
    src/ui/widgets/editorwidgets.h \
    src/ui/widgets/planeditor.h \
    src/ui/widgets/smartentry.h \
    src/ui/widgets/tasklist.h \
    src/ui/widgets/widgettools.h \
    src/ui/windows/settings.h \
    src/ui/windows/window.h

RESOURCES += resources/theme.qrc

# Debug our application against the installed release Qt libraries.
win32-g++:contains(CONFIG, app_debug) {
    QMAKE_CXXFLAGS_RELEASE = -O0 -g3 -fno-omit-frame-pointer
    QMAKE_LFLAGS_RELEASE =
}

msvc:QMAKE_CXXFLAGS += /utf-8
