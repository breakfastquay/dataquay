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

    if (map.empty()) {
#define MAP0(x) map[qtypePrefix + #x] = new Maker0<x>();
#define MAP1(x,y) map[qtypePrefix + #x] = new Maker1<x,y>();
        MAP1(QObject, QObject);
#ifdef ADD_WIDGETS
	MAP0(QMainWindow);
	MAP1(QFrame, QWidget);
	MAP1(QLabel, QWidget);
	MAP1(QGridLayout, QWidget);
	MAP1(QVBoxLayout, QWidget);
	MAP1(QMenu, QWidget);
	MAP1(QMenuBar, QWidget);
	MAP1(QAction, QObject);
#endif
#undef MAP0
#undef MAP1
    }

    return map;
}

class QObjectMapper::D
{
public:
    typedef QHash<QString, QObject *> UriObjectMap;
    typedef QMap<QObject *, QUrl> ObjectUriMap;

    D(Store *s) : m_s(s), m_makers(makers()) {
//!!! addPrefix not a part of Store -- what to do for the best?
//        m_s->addPrefix("qtype", qtypePrefix);
//        m_s->addPrefix("dq", dqPrefix);
    }

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
	PropertyObject po(m_s, qtypePrefix, uri);
	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {
            bool write = true;
            if (psp != AllProperties) {
                write = o->metaObject()->property(i).isStored();
            }
            if (write) {
                QString pname = o->metaObject()->property(i).name();
                QByteArray pnba = pname.toLocal8Bit();
                bool compare =
                    ((psp == PropertiesChangedFromDefault ||
                      psp == PropertiesChangedFromUnparentedDefault) &&
                     m_makers.contains(type.toString())); 
		if (compare) {
                    QObject *deftParent = 0;
                    if (psp == PropertiesChangedFromDefault) {
                        deftParent = o->parent();
                    }
                    QObject *deft = m_makers[type.toString()]->make(deftParent);
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

        UriObjectMap map;

        Triples candidates = m_s->match(Triple(Node(), "a", Node()));

        foreach (Triple t, candidates) {
            
            if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

            QString objectUri = t.a.value;
            QString typeUri = t.c.value;
            if (!m_makers.contains(typeUri)) {
                DEBUG << "Can't construct type " << typeUri << endl;
                continue;
            }
            
            loadFrom(objectUri, map);
        }

        QObjectList rootObjects;
        foreach (QObject *o, map) {
            if (!o->parent()) {
                rootObjects.push_back(o);
            }
        }

        if (rootObjects.size() == 1) {
            return rootObjects[0];
        }

        QObject *superRoot = new QObject;
        foreach (QObject *o, rootObjects) {
            o->setParent(superRoot);
        }
        return superRoot;
    }

    QUrl storeObject(QObject *o,
                     URISourcePolicy sourcePolicy,
                     PropertySelectionPolicy propertyPolicy)
    {
        return storeSingle(o, ObjectUriMap(), sourcePolicy, propertyPolicy);
    }

    QUrl storeObjects(QObject *root,
                      URISourcePolicy sourcePolicy,
                      ObjectSelectionPolicy objectPolicy,
                      PropertySelectionPolicy propertyPolicy)
    {
        return storeTree(root, ObjectUriMap(),
                         sourcePolicy, objectPolicy, propertyPolicy);
    }

private:
    Store *m_s;
    MakerMap m_makers;
	
    //!!! should have exceptions on errors, not just returning 0

    QObject *loadSingle(QUrl uri, QObject *parent, UriObjectMap &map) {

	if (map.contains(uri.toString())) {
	    return map[uri.toString()];
	}

	//!!! how to configure prefix?
	PropertyObject pod(m_s, dqPrefix, uri);
	QString type = pod.getObjectType().toString();
	if (!m_makers.contains(type)) {
            DEBUG << "Can't construct type " << type << endl;
            return 0;
        }
    
        DEBUG << "Making object " << uri << " of type " << uri << endl;

	QObject *o = m_makers[type]->make(parent);
	if (!o) {
	    std::cerr << "Failed to make object!" << std::endl;
	    return o;
	}
	
	loadProperties(o, uri);

        o->setProperty("uri", uri);
        map[uri.toString()] = o;

	//!!! call back on registered property/arrangement setters
	return o;
    }

    QObject *loadTree(QUrl uri, QObject *parent, UriObjectMap &map) {

        QObject *o = loadSingle(uri, parent, map);

        Triples childTriples = m_s->match(Triple(Node(), "dq:parent", uri));
        foreach (Triple t, childTriples) {
            loadTree(t.a.value, o, map);
        }

        return o;
    }

    QObject *loadFrom(QUrl source, UriObjectMap &map) {

	PropertyObject po(m_s, dqPrefix, source);

	if (po.hasProperty("follows")) {
	    loadFrom(po.getProperty("follows").toUrl(), map);
	}

        QObject *parent = 0;
	if (po.hasProperty("parent")) {
	    parent = loadFrom(po.getProperty("parent").toUrl(), map);
	}

        QObject *o = loadSingle(source, parent, map);
        return o;
    }

    QUrl storeSingle(QObject *o, ObjectUriMap &map,
                     URISourcePolicy sourcePolicy,
                     PropertySelectionPolicy propertyPolicy) {

        if (map.contains(o)) return map[o];

        QString cname = o->metaObject()->className();

        QUrl uri;
        bool setUri = false;
        if (sourcePolicy != CreateNewURIs) {
            QVariant uriV = o->property("uri");
            if (uriV != QVariant()) uri = uriV.toUrl();
            else setUri = true;
        }
        if (uri == QUrl()) {
            uri = m_s->getUniqueUri
                (":" + cname.toLower().right(cname.length()-1) + "_");
        }
        
        map[o] = uri;
        if (setUri) o->setProperty("uri", uri); //!!! document this
        
        QUrl type = m_s->expand(QString("qtype:%1").arg(cname));
        m_s->add(Triple(uri, "a", type));

        if (o->parent() && map.contains(o->parent())) {
            m_s->add(Triple(uri, "dq:parent", map[o->parent()]));
        }

        storeProperties(o, uri, propertyPolicy);

        return uri;
    }

    QUrl storeTree(QObject *o, ObjectUriMap &map,
                   URISourcePolicy sourcePolicy,
                   ObjectSelectionPolicy objectPolicy,
                   PropertySelectionPolicy propertyPolicy) {

        if (objectPolicy == ObjectsWithURIs) {
            if (o->property("uri") == QVariant()) return QUrl();
        }

        QUrl me = storeSingle(o, map, sourcePolicy, propertyPolicy);

        foreach (QObject *c, o->children()) {
            storeTree(c, map, sourcePolicy, objectPolicy, propertyPolicy);
        }
    }

};

QObjectMapper::QObjectMapper(Store *s) :
    m_d(new D(s))
{ }

QObjectMapper::~QObjectMapper()
{
    delete m_d;
}

void
QObjectMapper::loadProperties(QObject *o, QUrl uri)
{
    m_d->loadProperties(o, uri);
}

void
QObjectMapper::storeProperties(QObject *o, QUrl uri, PropertySelectionPolicy psp)
{
    m_d->storeProperties(o, uri, psp);
}

QObject *
QObjectMapper::loadObject(QUrl uri, QObject *parent)
{
    return m_d->loadObject(uri, parent);
}

QObject *
QObjectMapper::loadObjects(QUrl rootUri, QObject *parent)
{
    return m_d->loadObjects(rootUri, parent);
}

QObject *
QObjectMapper::loadAllObjects(QObject *parent)
{
    return m_d->loadAllObjects(parent);
}

QUrl
QObjectMapper::storeObject(QObject *o, URISourcePolicy usp, PropertySelectionPolicy psp)
{
    return m_d->storeObject(o, usp, psp);
}

QUrl
QObjectMapper::storeObjects(QObject *root, URISourcePolicy usp, ObjectSelectionPolicy osp, PropertySelectionPolicy psp)
{
    return m_d->storeObjects(root, usp, osp, psp);
}

}


	
