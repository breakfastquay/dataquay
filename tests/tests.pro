
TEMPLATE = app
CONFIG += debug
QT -= gui
TARGET = test-dataquay

INCLUDEPATH += . ..
LIBPATH += ..
LIBS += -ldataquay -lraptor -lrasqal -lrdf

HEADERS += TestDataquay.h
SOURCES += TestDataquay.cpp

