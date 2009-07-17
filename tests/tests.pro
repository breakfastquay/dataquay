
TEMPLATE = app
CONFIG += debug
QT -= gui
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a
LIBS += -ldataquay -lraptor -lrasqal -lrdf

SOURCES += TestDataquay.cpp

