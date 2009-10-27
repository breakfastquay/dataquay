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
static QString dqPrefix = "http://breakfastquay.com/rdf/dataquay/common/";

class ObjectMapper::D
{
public:
    typedef QHash<QString, QObject *> UriObjectMap;
    typedef QMap<QObject *, QUrl> ObjectUriMap;

    D(Store *s) :
        m_s(s),
        m_psp(SaveAlways),
        m_osp(SaveAllObjects)
    {
//!!! addPrefix not a part of Store -- what to do for the best?
//        m_s->addPrefix("qtype", qtypePrefix);
//        m_s->addPrefix("dq", dqPrefix);
    }

    void setPropertySavePolicy(PropertySavePolicy psp) {
        m_psp = psp; 
    }

    PropertySavePolicy getPropertySavePolicy() const {
        return m_psp;
    }

    void setObjectSavePolicy(ObjectSavePolicy osp) {
        m_osp = osp; 
    }

    ObjectSavePolicy getObjectSavePolicy() const {
        return m_osp;
    }

    void loadProperties(QObject *o, QUrl uri) {
	PropertyObject po(m_s, qtypePrefix, uri);
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
	PropertyObject po(m_s, qtypePrefix, uri);

        ObjectBuilder *builder = ObjectBuilder::getInstance();

	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            if (!o->metaObject()->property(i).isStored() ||
                !o->metaObject()->property(i).isReadable() ||
                !o->metaObject()->property(i).isWritable()) {
                continue;
            }

            QString pname = o->metaObject()->property(i).name();
            QByteArray pnba = pname.toLocal8Bit();

            if (m_psp == SaveIfChanged) {
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

            if (!typeUri.startsWith(qtypePrefix)) continue;
            QString type = typeUri.replace(qtypePrefix, "");

            if (!builder->knows(type)) {
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

    QUrl storeObject(QObject *o)
    {
        ObjectUriMap map;
        return storeSingle(o, map);
    }

    QUrl storeObjects(QObject *root)
    {
        ObjectUriMap map;
        return storeTree(root, map);
    }

private:
    Store *m_s;
    PropertySavePolicy m_psp;
    ObjectSavePolicy m_osp;
	
    //!!! should have exceptions on errors, not just returning 0

    QObject *loadSingle(QUrl uri, QObject *parent, UriObjectMap &map) {

	if (map.contains(uri.toString())) {
	    return map[uri.toString()];
	}

	//!!! how to configure prefix?
	PropertyObject pod(m_s, dqPrefix, uri);
	QString type = pod.getObjectType().toString();
        if (!type.startsWith(qtypePrefix)) throw UnknownTypeException(type);

        type = type.replace(qtypePrefix, "");
        ObjectBuilder *builder = ObjectBuilder::getInstance();
	if (!builder->knows(type)) throw UnknownTypeException(type);
    
        DEBUG << "Making object " << uri << " of type " << uri << endl;

	QObject *o = builder->build(type, parent);
	if (!o) throw ConstructionFailedException(type);
	
	loadProperties(o, uri);

        o->setProperty("uri", uri);
        map[uri.toString()] = o;

	//!!! call back on registered property/arrangement setters (qwidget, qlayout etc)
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

        QString cname = o->metaObject()->className();

        QUrl uri;
        QVariant uriVar = o->property("uri");

        if (uriVar != QVariant()) {
            uri = uriVar.toUrl();
        } else {
            uri = m_s->getUniqueUri
                (":" + cname.toLower().right(cname.length()-1) + "_");
            o->setProperty("uri", uri); //!!! document this
        }

        map[o] = uri;
        
        QUrl type = m_s->expand(QString("qtype:%1").arg(cname));
        m_s->add(Triple(uri, "a", type));

        if (o->parent() && map.contains(o->parent())) {
            m_s->add(Triple(uri, "dq:parent", map[o->parent()]));
        }

        storeProperties(o, uri);

        return uri;
    }

    QUrl storeTree(QObject *o, ObjectUriMap &map) {

        if (m_osp == SaveObjectsWithURIs) {
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
ObjectMapper::setPropertySavePolicy(PropertySavePolicy policy)
{
    m_d->setPropertySavePolicy(policy);
}

ObjectMapper::PropertySavePolicy
ObjectMapper::getPropertySavePolicy() const
{
    return m_d->getPropertySavePolicy();
}

void
ObjectMapper::setObjectSavePolicy(ObjectSavePolicy policy)
{
    m_d->setObjectSavePolicy(policy);
}

ObjectMapper::ObjectSavePolicy
ObjectMapper::getObjectSavePolicy() const
{
    return m_d->getObjectSavePolicy();
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

}


	