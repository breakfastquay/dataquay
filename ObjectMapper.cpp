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

#include "ObjectMapper.h"
#include "ObjectBuilder.h"
#include "PropertyObject.h"
#include "Store.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QVariant>
#include <QHash>
#include <QMap>

#include <memory> // auto_ptr

#include "Debug.h"

namespace Dataquay {

static QString qtypePrefix = "http://breakfastquay.com/rdf/dataquay/qtype/";
static QString dqPrefix = "http://breakfastquay.com/rdf/dataquay/common/"; //???

class ObjectMapper::D
{
public:
    D(Store *s) :
        m_s(s),
        m_psp(StoreAlways),
        m_osp(StoreAllObjects),
        m_typePrefix(qtypePrefix),
        m_propertyPrefix(qtypePrefix) {
    }

    void setObjectTypePrefix(QString prefix) {
        m_typePrefix = prefix;
    }

    void setPropertyPrefix(QString prefix) { 
        m_propertyPrefix = prefix;
    }

    void addTypeMapping(QUrl uri, QString className) {
        m_typeMap[uri] = className;
        m_typeRMap[className] = uri;
    }

    void setPropertyStorePolicy(PropertyStorePolicy psp) {
        m_psp = psp; 
    }

    PropertyStorePolicy getPropertyStorePolicy() const {
        return m_psp;
    }

    void setObjectStorePolicy(ObjectStorePolicy osp) {
        m_osp = osp; 
    }

    ObjectStorePolicy getObjectStorePolicy() const {
        return m_osp;
    }

    void loadProperties(QObject *o, QUrl uri) {
	PropertyObject po(m_s, m_propertyPrefix, uri);
	QStringList pl = po.getProperties();
	foreach (QString property, pl) {
	    QByteArray ba = property.toLocal8Bit();
	    QVariant value = po.getProperty(property);
	    if (!o->setProperty(ba.data(), value)) {
		// Not an error; could be a dynamic property
		DEBUG << "ObjectMapper::loadProperties: Property set failed for property " << ba.data() << " to value " << value << " on object " << uri << endl;
	    }
	}
    }

    void storeProperties(QObject *o, QUrl uri) {

	QString cname = o->metaObject()->className();
	PropertyObject po(m_s, m_propertyPrefix, uri);

        ObjectBuilder *builder = ObjectBuilder::getInstance();

	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            if (!o->metaObject()->property(i).isStored() ||
                !o->metaObject()->property(i).isReadable() ||
                !o->metaObject()->property(i).isWritable()) {
                continue;
            }

            QString pname = o->metaObject()->property(i).name();
            QByteArray pnba = pname.toLocal8Bit();

            if (m_psp == StoreIfChanged) {
                if (builder->knows(cname)) {
                    std::auto_ptr<QObject> c(builder->build(cname, 0));
                    if (o->property(pnba.data()) == c->property(pnba.data())) {
                        continue;
                    }
                }
            }

            po.setProperty(0, pname, o->property(pnba.data()));
	}
    }

    QObject *loadObject(QUrl uri, QObject *parent) {
        UriObjectMap map;
	return loadSingle(uri, parent, map);
    }

    QObject *loadObjects(QUrl root, QObject *parent) {
        UriObjectMap map;
	return loadTree(root, parent, map);
    }

    QObject *loadAllObjects(QObject *parent) {

        UriObjectMap map;

        Triples candidates = m_s->match(Triple(Node(), "a", Node()));

        ObjectBuilder *builder = ObjectBuilder::getInstance();

        foreach (Triple t, candidates) {
            
            if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

            QString objectUri = t.a.value;
            QString typeUri = t.c.value;

            QString className;
            if (m_typeMap.contains(typeUri)) {
                className = m_typeMap[typeUri];
            } else {
                if (!typeUri.startsWith(m_typePrefix)) continue;
                className = typeUri.right(typeUri.length() - m_typePrefix.length());
            }

            if (!builder->knows(className)) {
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

    QUrl storeObject(QObject *o) {
        ObjectUriMap map;
        return storeSingle(o, map);
    }

    QUrl storeObjects(QObject *root) {
        ObjectUriMap map;
        return storeTree(root, map);
    }

    void addLoadCallback(LoadCallback *cb) {
        m_loadCallbacks.push_back(cb);
    }

    void addStoreCallback(StoreCallback *cb) {
        m_storeCallbacks.push_back(cb);
    }

private:
    Store *m_s;
    PropertyStorePolicy m_psp;
    ObjectStorePolicy m_osp;
    QString m_typePrefix;
    QString m_propertyPrefix;
    QList<LoadCallback *> m_loadCallbacks;
    QList<StoreCallback *> m_storeCallbacks;
    QMap<QUrl, QString> m_typeMap;
    QHash<QString, QUrl> m_typeRMap;

    QObject *loadSingle(QUrl uri, QObject *parent, UriObjectMap &map) {

	if (map.contains(uri.toString())) {
	    return map[uri.toString()];
	}

	//!!! how to configure prefix?
	PropertyObject pod(m_s, dqPrefix, uri);

	QString typeUri = pod.getObjectType().toString();
        QString className;

        if (m_typeMap.contains(typeUri)) {
            className = m_typeMap[typeUri];
        } else {
            if (!typeUri.startsWith(m_typePrefix)) {
                throw UnknownTypeException(typeUri);
            }
            className = typeUri.right(typeUri.length() - m_typePrefix.length());
        }

        ObjectBuilder *builder = ObjectBuilder::getInstance();
	if (!builder->knows(className)) throw UnknownTypeException(className);
    
        DEBUG << "Making object " << uri << " of type " << uri << endl;

	QObject *o = builder->build(className, parent);
	if (!o) throw ConstructionFailedException(typeUri);
	
	loadProperties(o, uri);

        o->setProperty("uri", uri);
        map[uri.toString()] = o;

        callLoadCallbacks(map, uri, o);

	return o;
    }

    void callLoadCallbacks(UriObjectMap &map, QUrl uri, QObject *o) {
        foreach (LoadCallback *cb, m_loadCallbacks) {
            cb->loaded(m_s, map, uri, o);
        }
    }

    QObject *loadTree(QUrl uri, QObject *parent, UriObjectMap &map) {

        QObject *o;
        try {
            o = loadSingle(uri, parent, map);
        } catch (UnknownTypeException e) {
            o = 0;
        }

        if (o) {
            Triples childTriples = m_s->match(Triple(Node(), "dq:parent", uri));
            foreach (Triple t, childTriples) {
                loadTree(t.a.value, o, map);
            }
        }

        return o;
    }

    QObject *loadFrom(QUrl source, UriObjectMap &map) {

	PropertyObject po(m_s, dqPrefix, source);

	if (po.hasProperty("follows")) {
            try {
                loadFrom(po.getProperty("follows").toUrl(), map);
            } catch (UnknownTypeException) { }
	}

        QObject *parent = 0;
	if (po.hasProperty("parent")) {
            try {
                parent = loadFrom(po.getProperty("parent").toUrl(), map);
            } catch (UnknownTypeException) {
                parent = 0;
            }
	}

        QObject *o = loadSingle(source, parent, map);
        return o;
    }

    QUrl storeSingle(QObject *o, ObjectUriMap &map) {

        if (map.contains(o)) return map[o];

        QString className = o->metaObject()->className();

        QUrl uri;
        QVariant uriVar = o->property("uri");

        if (uriVar != QVariant()) {
            uri = uriVar.toUrl();
        } else {
            uri = m_s->getUniqueUri
                (":" + className.toLower().right(className.length()-1) + "_");
            o->setProperty("uri", uri); //!!! document this
        }

        map[o] = uri;

        QUrl typeUri;
        if (m_typeRMap.contains(className)) {
            typeUri = m_typeRMap[className];
        } else {
            typeUri = m_typePrefix + className;
        }
        m_s->add(Triple(uri, "a", typeUri));

        if (o->parent() && map.contains(o->parent())) {
            m_s->add(Triple(uri, "dq:parent", map[o->parent()]));
        }

        storeProperties(o, uri);

        callStoreCallbacks(map, o, uri);

        return uri;
    }

    void callStoreCallbacks(ObjectUriMap &map, QObject *o, QUrl uri) {
        foreach (StoreCallback *cb, m_storeCallbacks) {
            cb->stored(m_s, map, o, uri);
        }
    }

    QUrl storeTree(QObject *o, ObjectUriMap &map) {

        if (m_osp == StoreObjectsWithURIs) {
            if (o->property("uri") == QVariant()) return QUrl();
        }

        QUrl me = storeSingle(o, map);

        foreach (QObject *c, o->children()) {
            storeTree(c, map);
        }
        
        return me;
    }

};

ObjectMapper::ObjectMapper(Store *s) :
    m_d(new D(s))
{ }

ObjectMapper::~ObjectMapper()
{
    delete m_d;
}

void
ObjectMapper::setObjectTypePrefix(QString prefix)
{
    m_d->setObjectTypePrefix(prefix);
}

void
ObjectMapper::setPropertyPrefix(QString prefix)
{
    m_d->setPropertyPrefix(prefix);
}

void
ObjectMapper::addTypeMapping(QUrl uri, QString className)
{
    m_d->addTypeMapping(uri, className);
}

void
ObjectMapper::setPropertyStorePolicy(PropertyStorePolicy policy)
{
    m_d->setPropertyStorePolicy(policy);
}

ObjectMapper::PropertyStorePolicy
ObjectMapper::getPropertyStorePolicy() const
{
    return m_d->getPropertyStorePolicy();
}

void
ObjectMapper::setObjectStorePolicy(ObjectStorePolicy policy)
{
    m_d->setObjectStorePolicy(policy);
}

ObjectMapper::ObjectStorePolicy
ObjectMapper::getObjectStorePolicy() const
{
    return m_d->getObjectStorePolicy();
}

void
ObjectMapper::loadProperties(QObject *o, QUrl uri)
{
    m_d->loadProperties(o, uri);
}

void
ObjectMapper::storeProperties(QObject *o, QUrl uri)
{
    m_d->storeProperties(o, uri);
}

QObject *
ObjectMapper::loadObject(QUrl uri, QObject *parent)
{
    return m_d->loadObject(uri, parent);
}

QObject *
ObjectMapper::loadObjects(QUrl rootUri, QObject *parent)
{
    return m_d->loadObjects(rootUri, parent);
}

QObject *
ObjectMapper::loadAllObjects(QObject *parent)
{
    return m_d->loadAllObjects(parent);
}

QUrl
ObjectMapper::storeObject(QObject *o)
{
    return m_d->storeObject(o);
}

QUrl
ObjectMapper::storeObjects(QObject *root)
{
    return m_d->storeObjects(root);
}

void
ObjectMapper::addLoadCallback(LoadCallback *cb)
{
    m_d->addLoadCallback(cb);
}

void
ObjectMapper::addStoreCallback(StoreCallback *cb)
{
    m_d->addStoreCallback(cb);
}

}


	
