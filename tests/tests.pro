
TEMPLATE = app
CONFIG += debug
QT -= gui
TARGET = test-dataquay

INCLUDEPATH += . ..
DEPENDPATH += . ..
LIBPATH += ..
PRE_TARGETDEPS += ../libdataquay.a
#LIBS += -ldataquay -lraptor -lrasqal -lrdf
LIBPATH += ../../ext
LIBS += -ldataquay -lext

SOURCES += TestDataquay.cpp

solaris* {
  LIBS += -lsocket -lnsl
}
