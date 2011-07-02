
TEMPLATE = lib
CONFIG += warn_on staticlib
QT -= gui

TARGET = dataquay

#DEFINES += USE_REDLAND
DEFINES += USE_SORD

OBJECTS_DIR = o
MOC_DIR = o

INCLUDEPATH += dataquay

HEADERS += dataquay/BasicStore.h \
           dataquay/Connection.h \
           dataquay/Node.h \
           dataquay/PropertyObject.h \
           dataquay/RDFException.h \
           dataquay/Store.h \
           dataquay/Transaction.h \
           dataquay/TransactionalStore.h \
           dataquay/Triple.h \
           dataquay/Uri.h \
           dataquay/objectmapper/ContainerBuilder.h \
           dataquay/objectmapper/ObjectBuilder.h \
           dataquay/objectmapper/ObjectLoader.h \
           dataquay/objectmapper/ObjectMapper.h \
           dataquay/objectmapper/ObjectMapperForwarder.h \
           dataquay/objectmapper/ObjectStorer.h \
           dataquay/objectmapper/TypeMapping.h \
           src/Debug.h
           
SOURCES += src/Connection.cpp \
           src/Node.cpp \
           src/PropertyObject.cpp \
           src/Store.cpp \
           src/Transaction.cpp \
           src/TransactionalStore.cpp \
           src/Triple.cpp \
           src/Uri.cpp \
           src/backend/BasicStoreRedland.cpp \
           src/backend/BasicStoreSord.cpp \
           src/objectmapper/ContainerBuilder.cpp \
           src/objectmapper/ObjectBuilder.cpp \
           src/objectmapper/ObjectLoader.cpp \
           src/objectmapper/ObjectMapper.cpp \
           src/objectmapper/ObjectMapperForwarder.cpp \
           src/objectmapper/ObjectStorer.cpp \
           src/objectmapper/TypeMapping.cpp

exists(platform.pri) {
	include(./platform.pri)
}

