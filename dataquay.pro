
TEMPLATE = lib
CONFIG += warn_on staticlib
QT -= gui
TARGET = dataquay

load(../platform.prf)
TEMPLATE += platform

INCLUDEPATH += \
	   ../ext/librdf/raptor-1.4.18/src \
	   ../ext/librdf/rasqal-0.9.16/src \
	   ../ext/librdf/redland-1.0.9/src

# Input
HEADERS += RDFException.h \
           PropertyObject.h \
           BasicStore.h \
           Store.h \
           Transaction.h \
           TransactionalConnection.h \
           TransactionalStore.h \
           Triple.h \
           Node.h 
           
SOURCES += Node.cpp \
           BasicStore.cpp \
           PropertyObject.cpp \
           Transaction.cpp \
           TransactionalConnection.cpp \
           TransactionalStore.cpp \
           Triple.cpp
