
TEMPLATE = app
CONFIG += debug
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a

LIBS += -ldataquay -lraptor -lrasqal -lrdf

#LIBPATH += ../../ext
#LIBS += -ldataquay -lext

HEADERS += TestObjects.h
SOURCES += TestDataquay.cpp TestQtWidgets.cpp

