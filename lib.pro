
exists(debug.pri) {
	include(./debug.pri)
}

TEMPLATE = lib
CONFIG += warn_on staticlib
QT -= gui

TARGET = dataquay

# Define this to use the Redland datastore (http://librdf.org/)
#DEFINES += USE_REDLAND

# Define this to use the Sord datastore (http://drobilla.net/software/sord/)
DEFINES += USE_SORD

OBJECTS_DIR = o
MOC_DIR = o

INCLUDEPATH += dataquay

!debug:DEFINES += NDEBUG

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
           dataquay/objectmapper/ObjectMapperDefs.h \
           dataquay/objectmapper/ObjectMapperForwarder.h \
           dataquay/objectmapper/ObjectStorer.h \
           dataquay/objectmapper/TypeMapping.h \
           src/Debug.h
           
SOURCES += src/Connection.cpp \
           src/Node.cpp \
           src/PropertyObject.cpp \
           src/RDFException.cpp \
           src/Store.cpp \
           src/Transaction.cpp \
           src/TransactionalStore.cpp \
           src/Triple.cpp \
           src/Uri.cpp \
           src/backend/BasicStoreRedland.cpp \
           src/backend/BasicStoreSord.cpp \
           src/backend/define-check.cpp \
           src/objectmapper/ContainerBuilder.cpp \
           src/objectmapper/ObjectBuilder.cpp \
           src/objectmapper/ObjectLoader.cpp \
           src/objectmapper/ObjectMapper.cpp \
           src/objectmapper/ObjectMapperForwarder.cpp \
           src/objectmapper/ObjectStorer.cpp \
           src/objectmapper/TypeMapping.cpp

linux* {
        libraries.path = /usr/local/lib
        libraries.files = libdataquay.a
        includes.path = /usr/local/include
        includes.files = dataquay
        pkgconfig.path = /usr/local/lib/pkgconfig
        pkgconfig.files = deploy/dataquay.pc
        INSTALLS += libraries includes pkgconfig
}

exists(platform.pri) {
	include(./platform.pri)
}
