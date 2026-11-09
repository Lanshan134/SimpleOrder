QT += widgets sql
CONFIG += c++17
TARGET = SimpleOrder
VERSION = 0.3.0
SOURCES += src/main.cpp src/store.cpp src/ui.cpp src/settings.cpp src/plan.cpp src/planeditor.cpp src/smartentry.cpp
HEADERS += src/store.h src/ui.h src/settings.h src/planeditor.h src/editorwidgets.h src/theme.h src/smartentry.h
RESOURCES += resources/theme.qrc

msvc:QMAKE_CXXFLAGS += /utf-8
