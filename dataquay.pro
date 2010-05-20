
TEMPLATE = lib
CONFIG += warn_on staticlib
QT -= gui

TARGET = dataquay

OBJECTS_DIR = o
MOC_DIR = o

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
           objectmapper/ObjectLoader.h \
           objectmapper/ObjectMapper.h \
           objectmapper/ObjectMapperForwarder.h \
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
           objectmapper/ObjectLoader.cpp \
           objectmapper/ObjectMapper.cpp \
           objectmapper/ObjectMapperForwarder.cpp \
           objectmapper/ObjectStorer.cpp \
           objectmapper/TypeMapping.cpp

exists(platform.pri) {
	include(./platform.pri)
}

