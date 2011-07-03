
TEMPLATE = app
CONFIG += debug
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a

OBJECTS_DIR = o
MOC_DIR = o

LIBS += ../libdataquay.a 

HEADERS += TestObjects.h
SOURCES += TestDataquay.cpp

exists(./platform.pri) {
    include(./platform.pri)
}
!exists(./platform.pri) {
    exists(../platform.pri) {
	include(../platform.pri)
    }
}

