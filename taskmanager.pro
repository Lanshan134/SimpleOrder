QT += widgets sql
CONFIG += c++17
TARGET = SimpleOrder
SOURCES += src/main.cpp src/store.cpp src/ui.cpp src/settings.cpp src/plan.cpp src/planeditor.cpp
HEADERS += src/store.h src/ui.h src/settings.h src/planeditor.h src/editorwidgets.h

msvc:QMAKE_CXXFLAGS += /utf-8
