QT += widgets sql
CONFIG += c++17
TARGET = SimpleOrder
VERSION = 2.1.0
SOURCES += src/main.cpp src/store.cpp src/ui.cpp src/settings.cpp src/plan.cpp src/planeditor.cpp src/smartentry.cpp
HEADERS += src/store.h src/ui.h src/settings.h src/planeditor.h src/editorwidgets.h src/smartentry.h src/theme.h
RESOURCES += resources/theme.qrc

# Debug our application against the installed release Qt libraries.
win32-g++:contains(CONFIG, app_debug) {
    QMAKE_CXXFLAGS_RELEASE = -O0 -g3 -fno-omit-frame-pointer
    QMAKE_LFLAGS_RELEASE =
}

msvc:QMAKE_CXXFLAGS += /utf-8
