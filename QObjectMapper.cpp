/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009 Chris Cannam.
  
    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the name of Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#include "QObjectMapper.h"
#include "PropertyObject.h"
#include "Store.h"

#include <QMutex>
#include <QMutexLocker>
#include <QMetaObject>
#include <QMetaProperty>
#include <QVariant>
#include <QHash>
#include <QMap>

#include "Debug.h"

namespace Dataquay {

static QString qtypePrefix = "http://breakfastquay.com/rdf/dataquay/qtype/";
static QString dqPrefix = "http://breakfastquay.com/rdf/dataquay/common/";

struct MakerBase {
    virtual QObject *make(QObject *) = 0;
};

template <typename T>
struct Maker0 : public MakerBase {
    virtual QObject *make(QObject *) { return new T(); }
};

template <typename T, typename P>
struct Maker1 : public MakerBase {
    virtual QObject *make(QObject *p) { return new T(dynamic_cast<P *>(p)); }
};

typedef QHash<QString, MakerBase *> MakerMap;

//!!! noooo.. need to register these from outside if we want them
static MakerMap
makers()
{
    static MakerMap map;
    static QMutex mutex;
    QMutexLocker locker(&mutex);

#ifdef ADD_WIDGETS
    //!!!
    if (map.empty()) {
#define MAP0(x) map[qtypePrefix + #x] = new Maker0<x>();
#define MAP1(x,y) map[qtypePrefix + #x] = new Maker1<x,y>();
	MAP0(QMainWindow);
	MAP1(QFrame, QWidget);
	MAP1(QLabel, QWidget);
	MAP1(QGridLayout, QWidget);
	MAP1(QVBoxLayout, QWidget);
	MAP1(QMenu, QWidget);
	MAP1(QMenuBar, QWidget);
	MAP1(QAction, QObject);
#undef MAP0
#undef MAP1
    }
#endif

    return map;
}

class QObjectMapper::D
{
public:
    typedef QHash<QUrl, QObject *> UriObjectMap;
    typedef QMap<QObject *, QUrl> ObjectUriMap;

    D(Store *s) : m_s(s) { }

    void loadProperties(QObject *o, QUrl uri) {
	PropertyObject po(m_s, qtypePrefix, uri);
	QStringList pl = po.getProperties();
	foreach (QString property, pl) {
	    QByteArray ba = property.toLocal8Bit();
	    QVariant value = po.getProperty(property);
	    if (!o->setProperty(ba.data(), value)) {
		// Not an error; could be a dynamic property
		DEBUG << "QObjectMapper::loadProperties: Property set failed for property " << ba.data() << " to value " << value << " on object " << uri << endl;
	    }
	}
    }

    void storeProperties(QObject *o, QUrl uri, PropertySelectionPolicy psp) {
	QString cname = o->metaObject()->className();
	QUrl type = m_s->expand(QString("%1%2").arg(qtypePrefix).arg(cname));
	MakerMap mm = makers(); //!!! wherefrom? throw exception is psp != AllProperties and no maker map available
	PropertyObject po(m_s, qtypePrefix, uri);
	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {
	    if (o->metaObject()->property(i).isStored()) {
		QString pname = o->metaObject()->property(i).name();
		QByteArray pnba = pname.toLocal8Bit();
		bool write = true;
		if (mm.contains(type.toString())) {
		    //!!! or without parent?
		    QObject *deft = mm[type.toString()]->make(o->parent());
		    if (o->property(pnba.data()) == deft->property(pnba.data())) {
			write = false;
		    }
		    delete deft;
		}
		if (write) {
		    po.setProperty(0, pname, o->property(pnba.data()));
		}
	    }
	}
    }

    QObject *loadObject(QUrl uri, QObject *parent) {
	return loadSingle(uri, parent, UriObjectMap());
    }

    QObject *loadObjects(QUrl root, QObject *parent) {
	return loadTree(root, parent, UriObjectMap());
    }

    QObject *loadAllObjects(QObject *parent) {
	//!!!

	// We would pile in and load all objects in the store in the
	// order in which we find them, recursing up to their parents
	// as appropriate.  But we've been given a parent, which (if
	// non-null) needs to be the parent of all in the forest.
	// Should we add this afterwards?
    }


private:
    Store *m_s;
	
    QObject *loadSingle(QUrl uri, QObject *parent, UriObjectMap &map) {

	if (map.contains(uri)) {
	    return map[uri];
	}

	//!!! how to configure prefix?
	PropertyObject pod(m_s, dqPrefix, uri);

	QString type = pod.getObjectType().toString();
	if (!type.startsWith(qtypePrefix)) {
	    std::cerr << "not a qtypePrefix property: " << type.toStdString() << std::endl;
	    //!!!
	    return 0;
	}

	MakerMap mm = makers();
	if (!mm.contains(type)) {
	    std::cerr << "not a known type: " << type.toStdString() << std::endl;
	    return 0;
	}
    
	std::cerr << "Making object <" << objectUri.toString().toStdString()
		  << "> of type <" << type.toStdString() << ">" << std::endl;

	QObject *o = mm[type]->make(parent);
	if (!o) {
	    std::cerr << "Failed to make object!" << std::endl;
	    return o;
	}
	
	loadProperties(o, uri);

	//!!! call back on registered property/arrangement setters
	return o;
    }

    QObject *loadTree(QUrl root, QObject *parent, UriObjectMap &map) {

	PropertyObject po(m_s, dqPrefix, uri);

	if (po.hasProperty("follows")) {
	    loadTree(pod.getProperty("follows").toUrl(), map);
	}

	return loadSingle(root, parent, map);
    }

    QObject *loadParent(QUrl uri, UriObjectMap &map) {

	PropertyObject po(m_s, dqPrefix, uri);

	if (po.hasProperty("parent")) {
	    return loadSingle(pod.getProperty("parent").toUrl(), map);
	}

	return 0;
    }
	
};

}


	
