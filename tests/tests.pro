
TEMPLATE = app
CONFIG += debug
#QT -= gui
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a
#LIBS += -ldataquay -lraptor -lrasqal -lrdf
LIBPATH += ../../ext
LIBS += -ldataquay -lext

HEADERS += TestObjects.h
SOURCES += TestDataquay.cpp TestQtWidgets.cpp

solaris* {
  QMAKE_CXXFLAGS_DEBUG += -xprofile=tcov
  debug: LIBS += -xprofile=tcov
  LIBS += -lsocket -lnsl

        release {
        QMAKE_CXXFLAGS_RELEASE += -xpg -g
	LIBS += -xpg
        }
}
