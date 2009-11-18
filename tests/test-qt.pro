
TEMPLATE = app
CONFIG += debug
TARGET = test-qt-widgets

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a
LIBPATH += ../../ext
LIBS += -ldataquay -lext

SOURCES += TestQtWidgets.cpp

solaris* {
  QMAKE_CXXFLAGS_DEBUG += -xprofile=tcov
  debug: LIBS += -xprofile=tcov
  LIBS += -lsocket -lnsl
}
