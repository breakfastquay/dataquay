
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
HEADERS += BasicStore.h \
           Connection.h \
           Node.h \
           PropertyObject.h \
           RDFException.h \
           Store.h \
           Transaction.h \
           TransactionalStore.h \
           Triple.h \
           Uri.h \
           objectmapper/ContainerBuilder.h \
           objectmapper/ObjectBuilder.h \
           objectmapper/ObjectMapper.h \
           objectmapper/ObjectStorer.h \
           objectmapper/TypeMapping.h
           
SOURCES += BasicStore.cpp \
           Connection.cpp \
           Node.cpp \
           PropertyObject.cpp \
           Transaction.cpp \
           TransactionalStore.cpp \
           Triple.cpp \
           Uri.cpp \
           objectmapper/ContainerBuilder.cpp \
           objectmapper/ObjectBuilder.cpp \
           objectmapper/ObjectMapper.cpp \
           objectmapper/ObjectStorer.cpp \
           objectmapper/TypeMapping.cpp
