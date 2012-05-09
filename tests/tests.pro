
TEMPLATE = app
CONFIG += debug
QT += testlib
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a

OBJECTS_DIR = o
MOC_DIR = o

LIBS += ../libdataquay.a  -lsord-0 -lserd-0

#HEADERS += TestObjects.h
#SOURCES += TestDataquay.cpp

HEADERS += TestBasicStore.h TestDatatypes.h
SOURCES += TestDatatypes.cpp main.cpp

exists(./platform.pri) {
    include(./platform.pri)
}
!exists(./platform.pri) {
    exists(../platform.pri) {
	include(../platform.pri)
    }
}

