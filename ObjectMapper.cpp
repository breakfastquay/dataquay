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

static QString defaultTypePrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/type/";
static QString defaultPropertyPrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/property/";
static QString defaultRelationshipPrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/relationship/";

class ObjectMapper::D
{
public:
    D(ObjectMapper *m, Store *s) :
        m_m(m),
        m_s(s),
        m_psp(StoreAlways),
        m_osp(StoreAllObjects),
        m_typePrefix(defaultTypePrefix),
        m_propertyPrefix(defaultPropertyPrefix),
        m_relationshipPrefix(defaultRelationshipPrefix) {
    }

    Store *getStore() {
        return m_s;
    }

    QString getObjectTypePrefix() const {
        return m_typePrefix;
    }

    void setObjectTypePrefix(QString prefix) {
        m_typePrefix = prefix;
    }

    QString getPropertyPrefix() const {
        return m_propertyPrefix;
    }

    void setPropertyPrefix(QString prefix) { 
        m_propertyPrefix = prefix;
    }

    QString getRelationshipPrefix() const {
        return m_relationshipPrefix;
    }

    void setRelationshipPrefix(QString prefix) { 
        m_relationshipPrefix = prefix;
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
        NodeObjectMap map;
        loadProperties(o, uri, map);
    }

    void storeProperties(QObject *o, QUrl uri) {
        ObjectNodeMap map;
        storeProperties(o, uri, map, false);
    }

    QObject *loadObject(QUrl uri, QObject *parent) {
        NodeObjectMap map;
	return loadSingle(uri, parent, map);
    }

    QObject *loadObjects(QUrl root, QObject *parent) {
        NodeObjectMap map;
        QObject *rv = loadTree(root, parent, map);
        loadConnections(map);
        return rv;
    }

    QObject *loadAllObjects(QObject *parent) {

        NodeObjectMap map;

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
                className = className.replace("/", "::");
            }

            if (!builder->knows(className)) {
                DEBUG << "Can't construct type " << typeUri << endl;
                continue;
            }
            
            loadFrom(objectUri, map);
        }

        loadConnections(map);

        QObjectList rootObjects;
        foreach (QObject *o, map) {
            if (!o->parent()) {
                rootObjects.push_back(o);
            }
        }

        if (!parent && (rootObjects.size() == 1)) {
            return rootObjects[0];
        }

        QObject *superRoot = parent;
        if (!superRoot) superRoot = new QObject;
        foreach (QObject *o, rootObjects) {
            o->setParent(superRoot);
        }
        return superRoot;
    }

    QUrl storeObject(QObject *o) {
        ObjectNodeMap map;
        return storeSingle(o, map, false).value;
    }

    QUrl storeObjects(QObject *root) {
        ObjectNodeMap map;
        foreach (QObject *o, root->findChildren<QObject *>()) {
            map[o] = Node();
        }
        return storeTree(root, map);
    }

    QObject *loadFrom(QUrl source, NodeObjectMap &map) {

	PropertyObject po(m_s, m_relationshipPrefix, source);

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

    QUrl store(QObject *o, ObjectNodeMap &map) {
        return storeSingle(o, map, true).value;
    }
    
    void addLoadCallback(LoadCallback *cb) {
        m_loadCallbacks.push_back(cb);
    }

    void addStoreCallback(StoreCallback *cb) {
        m_storeCallbacks.push_back(cb);
    }

private:
    ObjectMapper *m_m;
    Store *m_s;
    PropertyStorePolicy m_psp;
    ObjectStorePolicy m_osp;
    QString m_typePrefix;
    QString m_propertyPrefix;
    QString m_relationshipPrefix;
    QList<LoadCallback *> m_loadCallbacks;
    QList<StoreCallback *> m_storeCallbacks;
    QMap<QUrl, QString> m_typeMap;
    QHash<QString, QUrl> m_typeRMap;

    void loadProperties(QObject *o, QUrl uri, NodeObjectMap &map) {

	PropertyObject po(m_s, m_propertyPrefix, uri);
	foreach (QString property, po.getProperties()) {

	    QByteArray ba = property.toLocal8Bit();
	    QVariant value = po.getProperty(property);

            //!!! handle QObjectStar, UserType etc as below

            //!!! and blank node, whee

	    if (!o->setProperty(ba.data(), value)) {
		// Not an error; could be a dynamic property
		DEBUG << "ObjectMapper::loadProperties: Property set failed "
                      << "for property " << ba.data() << " to value "
                      << value << " on object " << uri << endl;
	    }
	}
    }

    void storeProperties(QObject *o, Node node, ObjectNodeMap &map, bool follow) {

	QString cname = o->metaObject()->className();
	PropertyObject po(m_s, m_propertyPrefix, node);

        ObjectBuilder *builder = ObjectBuilder::getInstance();

	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            QMetaProperty property = o->metaObject()->property(i);

            if (!property.isStored() ||
                !property.isReadable() ||
                !property.isWritable()) {
                continue;
            }

            QString pname = property.name();
            QByteArray pnba = pname.toLocal8Bit();

            QVariant value = o->property(pnba.data());
            Node pnode;

            if (m_psp == StoreIfChanged) {
                if (builder->knows(cname)) {
                    std::auto_ptr<QObject> c(builder->build(cname, 0));
                    if (value == c->property(pnba.data())) {
                        continue;
                    }
                }
            }

            DEBUG << "For object " << node.value << " writing property " << pname << endl;

            int type = property.type();
            int userType = property.userType();

            if (type == QMetaType::QObjectStar ||
                type == QMetaType::QWidgetStar ||
                type == QVariant::UserType) {
                QObject *ref = 0;
                if (type == QVariant::UserType) {
                    const char *refname = QMetaType::typeName(userType);
                    if (refname) {
                        ref = builder->extract(refname, value);
                        DEBUG << "Object is " << ref << endl;
                    }
                    DEBUG << "Property " << pname << " is of user type " << refname << ", object (if present) is " << ref << endl;
                } else {
                    ref = value.value<QObject *>();
                    DEBUG << "Property " << pname << " is of object type" << endl;
                }
                if (ref) {
                    if (ref->property("uri") != QVariant()) {
                        value = ref->property("uri");
                        DEBUG << "Writing property from object's URI property " << value << endl;
                    } else {
                        bool knownObject = map.contains(ref);
                        if (!knownObject || map[ref] == Node()) {
                            if (follow) {
                                if (!knownObject) {
                                    DEBUG << "Object is not in map, writing as blank node" << endl;
                                } else {
                                    DEBUG << "Object is in map but with no node yet, writing with URI" << endl;
                                }
                                map[ref] = storeSingle(ref, map, true,
                                                       //!!! crap api
                                                       !knownObject);
                            }
                        }
                        if (map[ref].type == Node::URI) {
                            value = QUrl(map[ref].value);
                            DEBUG << "Object is in map with URI node" << endl;
                        } else if (map[ref].type == Node::Blank) {
                            pnode = map[ref];
                            DEBUG << "Object is in map with blank node" << endl;
                        }
                    }
                }
            }

            if (pnode != Node()) {
                po.setProperty(0, pname, pnode);
            } else {
                po.setProperty(0, pname, value);
            }
	}
    }

    QObject *loadSingle(QUrl uri, QObject *parent, NodeObjectMap &map) {

	if (map.contains(uri)) {
	    return map[uri];
	}

	PropertyObject pod(m_s, m_relationshipPrefix, uri);

	QString typeUri = pod.getObjectType().toString();
        QString className;

        if (m_typeMap.contains(typeUri)) {
            className = m_typeMap[typeUri];
        } else {
            if (!typeUri.startsWith(m_typePrefix)) {
                throw UnknownTypeException(typeUri);
            }
            className = typeUri.right(typeUri.length() - m_typePrefix.length());
            className = className.replace("/", "::");
        }

        ObjectBuilder *builder = ObjectBuilder::getInstance();
	if (!builder->knows(className)) throw UnknownTypeException(className);
    
        DEBUG << "Making object " << uri << " of type " << uri << " with parent " << parent << endl;

	QObject *o = builder->build(className, parent);
	if (!o) throw ConstructionFailedException(typeUri);
	
	loadProperties(o, uri, map);

        o->setProperty("uri", uri);
        map[uri] = o;

        callLoadCallbacks(map, uri, o);

	return o;
    }

    void callLoadCallbacks(NodeObjectMap &map, QUrl uri, QObject *o) {
        foreach (LoadCallback *cb, m_loadCallbacks) {
            //!!! this doesn't really work out -- the callback doesn't know whether we're loading a single object or a graph; it may load any number of other related objects into the map, and if we were only supposed to load a single object, we won't know what to do with them afterwards (at the moment we just leak them)

            cb->loaded(m_m, map, uri, o);
        }
    }

    void loadConnections(NodeObjectMap &map) {
        
        QString slotTemplate = SLOT(xxx());
        QString signalTemplate = SIGNAL(xxx());

        // The store does not necessarily know m_relationshipPrefix

        ResultSet rs = m_s->query
            (QString
             (" PREFIX rel: <%1> "
              " SELECT ?sobj ?ssig ?tobj ?tslot WHERE { "
              " ?conn a rel:Connection; rel:source ?s; rel:target ?t. "
              " ?s rel:object ?sobj; rel:signal ?ssig. "
              " ?t rel:object ?tobj; rel:slot ?tslot. "
              " } ").arg(m_relationshipPrefix));

        foreach (Dictionary d, rs) {

            QUrl sourceUri = d["sobj"].value;
            QUrl targetUri = d["tobj"].value;
            if (!map.contains(sourceUri) || !map.contains(targetUri)) continue;

            QString sourceSignal = signalTemplate.replace("xxx", d["ssig"].value);
            QString targetSlot = slotTemplate.replace("xxx", d["tslot"].value);

            QByteArray sigba = sourceSignal.toLocal8Bit();
            QByteArray slotba = targetSlot.toLocal8Bit();
                
            QObject::connect(map[sourceUri], sigba.data(),
                             map[targetUri], slotba.data());
        }
    }

    QObject *loadTree(QUrl uri, QObject *parent, NodeObjectMap &map) {

        QObject *o;
        try {
            o = loadSingle(uri, parent, map);
        } catch (UnknownTypeException e) {
            o = 0;
        }

        if (o) {
            Triples childTriples = m_s->match
                (Triple(Node(), m_relationshipPrefix + "parent", uri));
            foreach (Triple t, childTriples) {
                loadTree(t.a.value, o, map);
            }
        }

        return o;
    }

    Node storeSingle(QObject *o, ObjectNodeMap &map, bool follow, bool blank = false) { //!!! crap api

        DEBUG << "storeSingle: blank = " << blank << endl;

        if (map.contains(o) && map[o] != Node()) return map[o];

        QString className = o->metaObject()->className();

        Node node;
        QVariant uriVar = o->property("uri");

        if (uriVar != QVariant()) {
            node = uriVar.toUrl();
        } else if (blank) {
            node = m_s->addBlankNode();
        } else {
            QString tag = className.toLower() + "_";
            tag.replace("::", "_");
            QUrl uri = m_s->getUniqueUri(":" + tag);
            o->setProperty("uri", uri); //!!! document this
            node = uri;
        }

        map[o] = node;

        QUrl typeUri;
        if (m_typeRMap.contains(className)) {
            typeUri = m_typeRMap[className];
        } else {
            typeUri = QString(m_typePrefix + className).replace("::", "/");
        }
        m_s->add(Triple(node, "a", typeUri));

        if (o->parent() && map.contains(o->parent()) && map[o->parent()] != Node()) {
            m_s->add(Triple(node, m_relationshipPrefix + "parent",
                            map[o->parent()]));
        }

        storeProperties(o, node, map, follow);

        callStoreCallbacks(map, o, node.value);//!!! wrongo, should pass node

        return node;
    }

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, QUrl uri) {
        foreach (StoreCallback *cb, m_storeCallbacks) {
            cb->stored(m_m, map, o, uri);
        }
    }

    QUrl storeTree(QObject *o, ObjectNodeMap &map) {

        if (m_osp == StoreObjectsWithURIs) {
            if (o->property("uri") == QVariant()) return QUrl();
        }

        QUrl me = storeSingle(o, map, true).value; //!!! no, rv was Node

        foreach (QObject *c, o->children()) {
            storeTree(c, map);
        }
        
        return me;
    }

};

ObjectMapper::ObjectMapper(Store *s) :
    m_d(new D(this, s))
{ }

ObjectMapper::~ObjectMapper()
{
    delete m_d;
}

Store *
ObjectMapper::getStore()
{
    return m_d->getStore();
}

QString
ObjectMapper::getObjectTypePrefix() const
{
    return m_d->getObjectTypePrefix();
}

void
ObjectMapper::setObjectTypePrefix(QString prefix)
{
    m_d->setObjectTypePrefix(prefix);
}

QString
ObjectMapper::getPropertyPrefix() const
{
    return m_d->getPropertyPrefix();
}

void
ObjectMapper::setPropertyPrefix(QString prefix)
{
    m_d->setPropertyPrefix(prefix);
}

QString
ObjectMapper::getRelationshipPrefix() const
{
    return m_d->getRelationshipPrefix();
}

void
ObjectMapper::setRelationshipPrefix(QString prefix)
{
    m_d->setRelationshipPrefix(prefix);
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

QObject *
ObjectMapper::loadFrom(QUrl sourceUri, NodeObjectMap &map)
{
    return m_d->loadFrom(sourceUri, map);
}

QUrl
ObjectMapper::store(QObject *o, ObjectNodeMap &map)
{
    return m_d->store(o, map);
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


	
