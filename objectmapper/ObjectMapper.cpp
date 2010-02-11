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
#include "ContainerBuilder.h"
#include "PropertyObject.h"
#include "Store.h"

#include <QMetaObject>
#include <QMetaProperty>
#include <QVariant>
#include <QHash>
#include <QMap>

#include <memory> // auto_ptr

#include "Debug.h"

#include <iostream>

#include <cassert>


namespace Dataquay {

static QString defaultTypePrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/type/";
static QString defaultPropertyPrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/property/";
static QString defaultRelationshipPrefix = "http://breakfastquay.com/rdf/dataquay/objectmapper/relationship/";

class ObjectMapper::D
{
public:
    D(ObjectMapper *m, Store *s) :
        m_m(m),
        m_ob(ObjectBuilder::getInstance()),
        m_cb(ContainerBuilder::getInstance()),
        m_s(s),
        m_psp(StoreAlways),
        m_osp(StoreAllObjects),
        m_bp(BlankNodesAsNeeded),
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

    void addTypeMapping(QString className, QString uri) {
        addTypeMapping(className, m_s->expand(uri));
    }

    void addTypeMapping(QString className, QUrl uri) {
        m_typeMap[uri] = className;
        m_typeRMap[className] = uri;
    }

    void addTypeUriPrefixMapping(QString className, QString prefix) {
        m_typeUriPrefixMap[className] = prefix;
    }

    void addPropertyMapping(QString className, QString propertyName, QString uri) {
        addPropertyMapping(className, propertyName, m_s->expand(uri));
    }

    void addPropertyMapping(QString className, QString propertyName, QUrl uri) {
        m_propertyMap[className][uri] = propertyName;
        m_propertyRMap[className][propertyName] = uri;
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

    void setBlankNodePolicy(BlankNodePolicy bp) {
        m_bp = bp; 
    }

    BlankNodePolicy getBlankNodePolicy() const {
        return m_bp;
    }

    void loadProperties(QObject *o, QUrl uri) {
        NodeObjectMap map;
        loadProperties(o, uri, map, false);
    }

    void storeProperties(QObject *o, QUrl uri) {
        ObjectNodeMap map;
        storeProperties(o, uri, map, false);
    }

    QObject *loadObject(QUrl uri, QObject *parent) {
        NodeObjectMap map;
	return loadSingle(uri, parent, map, "", false);
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

        foreach (Triple t, candidates) {
            
            if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

            QUrl objectUri = t.a.value;
            QString typeUri = t.c.value;

            QString className;

            try {
                className = typeUriToClassName(typeUri);
            } catch (UnknownTypeException) {
                continue;
            }

            if (!m_ob->knows(className)) {
                DEBUG << "Can't construct type " << typeUri << endl;
                continue;
            }
            
            loadFrom(objectUri, map, className);
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
        return storeTree(root, map).value;
    }

    void storeAllObjects(QObjectList list) {
        ObjectNodeMap map;
        foreach (QObject *o, list) {
            map[o] = Node();
            foreach (QObject *oo, o->findChildren<QObject *>()) {
                map[oo] = Node();
            }
        }
        foreach (QObject *o, list) {
            storeTree(o, map);
        }
    }

    QObject *loadFrom(Node node, NodeObjectMap &map, QString classHint = "") {

	PropertyObject po(m_s, m_relationshipPrefix, node);

	if (po.hasProperty("follows")) {
            try {
                loadFrom(po.getPropertyNode("follows"), map);
            } catch (UnknownTypeException) { }
	}

        QObject *parent = 0;
	if (po.hasProperty("parent")) {
            try {
                parent = loadFrom(po.getPropertyNode("parent"), map);
            } catch (UnknownTypeException) {
                parent = 0;
            }
	}

        //!!! NB. as this stands, if the RDF is "wrong" containing the
        //!!! wrong type for a property of this, we will fail the
        //!!! whole thing with an UnknownTypeException -- is that the
        //!!! right thing to do? consider

        QObject *o = loadSingle(node, parent, map, classHint, true);
        return o;
    }

    Node store(QObject *o, ObjectNodeMap &map) {
        return storeSingle(o, map, true);
    }
    
    void addLoadCallback(LoadCallback *cb) {
        m_loadCallbacks.push_back(cb);
    }

    void addStoreCallback(StoreCallback *cb) {
        m_storeCallbacks.push_back(cb);
    }

private:
    ObjectMapper *m_m;
    ObjectBuilder *m_ob;
    ContainerBuilder *m_cb;
    Store *m_s;
    PropertyStorePolicy m_psp;
    ObjectStorePolicy m_osp;
    BlankNodePolicy m_bp;
    QString m_typePrefix;
    QString m_propertyPrefix;
    QString m_relationshipPrefix;
    QList<LoadCallback *> m_loadCallbacks;
    QList<StoreCallback *> m_storeCallbacks;
    QMap<QUrl, QString> m_typeMap;
    QHash<QString, QUrl> m_typeRMap;
    QHash<QString, QString> m_typeUriPrefixMap;
    QHash<QString, QMap<QUrl, QString> > m_propertyMap;
    QHash<QString, QHash<QString, QUrl> > m_propertyRMap;

    QString typeUriToClassName(QUrl typeUri) {
        if (m_typeMap.contains(typeUri)) {
            return m_typeMap[typeUri];
        }
        QString s = typeUri.toString();
        if (!s.startsWith(m_typePrefix)) {
            throw UnknownTypeException(s);
        }
        s = s.right(s.length() - m_typePrefix.length());
        s = s.replace("/", "::");
        return s;
    }

    QUrl classNameToTypeUri(QString className) {
        QUrl typeUri;
        if (m_typeRMap.contains(className)) {
            typeUri = m_typeRMap[className];
        } else {
            typeUri = QString(m_typePrefix + className).replace("::", "/");
        }
        return typeUri;
    }

    void loadProperties(QObject *o, Node node, NodeObjectMap &map, bool follow) {

	QString cname = o->metaObject()->className();
	PropertyObject po(m_s, m_propertyPrefix, node);

	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            QMetaProperty property = o->metaObject()->property(i);

            if (!property.isStored() ||
                !property.isReadable() ||
                !property.isWritable()) {
                continue;
            }

            QString pname = property.name();
            Nodes pnodes;

            if (m_propertyRMap[cname].contains(pname)) {
                QUrl purl = m_propertyRMap[cname][pname];
                Triples t = m_s->match(Triple(node, purl, Node()));
                for (int i = 0; i < t.size(); ++i) {
                    pnodes << t[i].c;
                }
            } else {
                if (!po.hasProperty(pname)) continue;
                pnodes = po.getPropertyNodeList(pname);
            }

            if (pnodes.empty()) continue;
        
            int type = property.type();
            int userType = property.userType();
            QString typeName;

            if (type == QVariant::UserType) {
                typeName = QMetaType::typeName(userType);
            } else {
                typeName = QMetaType::typeName(type);
            }

            QVariant value = propertyNodeListToVariant
                (typeName, pnodes, map, follow);

            if (value.isValid()) {
                QByteArray pnba = pname.toLocal8Bit();
                if (!o->setProperty(pnba.data(), value)) {
                    DEBUG << "ObjectMapper::loadProperties: Property set failed "
                          << "for property " << pname << " to value of type "
                          << value.type() << " and value " << value
                          << " from (first) node " << pnodes[0].value << endl;
                }
            }
        }
    }

    QVariant propertyNodeListToVariant(QString typeName, Nodes pnodes,
                                       NodeObjectMap &map, bool follow) {

        if (pnodes.empty()) return QVariant();

        Node firstNode = pnodes[0];

        DEBUG << "propertyNodeListToVariant: typeName = " << typeName << endl;

        if (typeName == "") {
            return firstNode.toVariant();
        }

        if (m_cb->canInjectContainer(typeName)) {

            QString inContainerType = m_cb->getTypeNameForContainer(typeName);
            ContainerBuilder::ContainerKind k = m_cb->getContainerKind(typeName);

            if (k == ContainerBuilder::SequenceKind) {
                QVariantList list =
                    propertyNodeToList(inContainerType, firstNode, map, follow);
                return m_cb->injectContainer(typeName, list);

            } else if (k == ContainerBuilder::SetKind) {
                QVariantList list;
                foreach (Node pnode, pnodes) {
                    Nodes sublist;
                    sublist << pnode;
                    list << propertyNodeListToVariant(inContainerType, sublist,
                                                      map,  follow);
                }
                return m_cb->injectContainer(typeName, list);

            } else {
                return QVariant();
            }

        } else if (m_ob->canInject(typeName)) {

            if (follow) {
                QObject *obj = propertyNodeToObject(typeName, firstNode, map);

            //!!! NB if the returned object from propertyNodeToObject
            // has the wrong type, inject will fail (producing a
            // null variant) and the object will be leaked.  Need
            // to fix this.  This is a peculiar situation anyway,
            // because we already know the object's intended type;
            // we arguably don't really need loadSingle to re-read
            // the type from the RDF.  Mostly at least that provides
            // a consistency check, but the situation is troublesome
            // where the object has no type at all in the RDF or a
            // type that cannot be loaded -- in both cases we will
            // fail with an UnknownTypeException, whereas in the
            // former case we should be loading anyway (using the
            // object type that we already know) and in the latter
            // we should at least recover and ignore the property.
            // Need to fix these

                return m_ob->inject(typeName, obj);

            } else {
                return QVariant();
            }

        } else if (QString(typeName).contains("*") ||
                   QString(typeName).endsWith("Star")) {
            // do not attempt to read binary pointers!
            return QVariant();

        } else {
            return firstNode.toVariant();
        }
    }

    QObject *propertyNodeToObject(QString typeName, Node pnode,
                                  NodeObjectMap &map) {

        QString classHint;
        if (typeName != "") {
            classHint = m_ob->getClassNameForPointerName(typeName);
            DEBUG << "typeName " << typeName << " -> classHint " << classHint << endl;
        }

        if (pnode.type == Node::URI || pnode.type == Node::Blank) {
            return loadFrom(pnode, map, classHint);
        }

        return 0;
    }

    QVariantList propertyNodeToList(QString typeName, Node pnode,
                                    NodeObjectMap &map, bool follow) {
        
        QVariantList list;
        Triple t;

        while ((t = m_s->matchFirst(Triple(pnode, "rdf:first", Node())))
               != Triple()) {

            Node fnode = t.c;

            DEBUG << "propertyNodeToList: pnode " << pnode << ", fnode "
                  << fnode << ", follow " << follow <<  endl;

            Nodes fnodes;
            fnodes << fnode;
            QVariant value = propertyNodeListToVariant
                (typeName, fnodes, map, follow);

            if (value.isValid()) {
                DEBUG << "Found value: " << value << endl;
                list.push_back(value);
            } else {
                DEBUG << "propertyNodeToList: Invalid value in list, skipping" << endl;
            }
        
            t = m_s->matchFirst(Triple(pnode, "rdf:rest", Node()));
            if (t == Triple()) break;

            pnode = t.c;
            if (pnode == m_s->expand("rdf:nil")) break;
        }

        DEBUG << "propertyNodeToList: list has " << list.size() << " items" << endl;
        return list;
    }

    void storeProperties(QObject *o, Node node, ObjectNodeMap &map, bool follow) {

	QString cname = o->metaObject()->className();
	PropertyObject po(m_s, m_propertyPrefix, node);

	for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            QMetaProperty property = o->metaObject()->property(i);

            if (!property.isStored() ||
                !property.isReadable() ||
                !property.isWritable()) {
                continue;
            }

            QString pname = property.name();
            QByteArray pnba = pname.toLocal8Bit();

            if (pname == "uri") continue;

            QVariant value = o->property(pnba.data());

            if (m_psp == StoreIfChanged) {
                if (m_ob->knows(cname)) {
                    std::auto_ptr<QObject> c(m_ob->build(cname, 0));
                    if (value == c->property(pnba.data())) {
                        continue;
                    }
                }
            }

            DEBUG << "For object " << node.value << " writing property " << pname << " of type " << property.type() << endl;

            // 

            //!!! n.b. removing the old property value is problematic if it was a blank node which is now no longer referred to by anything else -- we can end up with all sorts of "spare" blank nodes -- should we check for this and remove other triples with it as subject? <-- removeOldPropertyNodes does this now

            Nodes pnodes = variantToPropertyNodeList(value, map, follow);

            QUrl puri;
            if (m_propertyRMap[cname].contains(pname)) {
                //!!! could this mapping be done by PropertyObject?
                puri = m_propertyRMap[cname][pname];
            } else {
                puri = po.getPropertyUri(pname);
            }

            //!!! not sure we need a PropertyObject here any more, we
            //!!! aren't really using it (because it's quicker, when
            //!!! pruning blank nodes, to do it all ourselves) --
            //!!! though should we make PropertyObject capable of
            //!!! doing that too?

            removeOldPropertyNodes(node, puri);

            Triple t(node, puri, Node());
            for (int i = 0; i < pnodes.size(); ++i) {
                t.c = pnodes[i];
                m_s->add(t);
            }
        }
    }            
            
    void removeOldPropertyNodes(Node node, QUrl propertyUri) {
        //!!! make PropertyObject do this?
        Triple t(node, propertyUri, Node());
        Triples m(m_s->match(t));
        foreach (t, m) {
            if (t.c.type == Node::Blank) {
                Triple t1(Node(), Node(), t.c);
                Triples m1(m_s->match(t1));
                bool stillUsed = false;
                foreach (t1, m1) {
                    if (t1.a != t.a) {
                        stillUsed = true;
                        break;
                    }
                }
                if (!stillUsed) {
                    DEBUG << "removeOldPropertyNodes: Former target node " << t.c << " is not target for any other predicate, removing everything with it as subject" << endl;
                    m_s->remove(Triple(t.c, Node(), Node()));
                }
            }
            m_s->remove(t);
        }
    }

    Nodes variantToPropertyNodeList(QVariant v, ObjectNodeMap &map, bool follow) {

        const char *typeName = 0;

        if (v.type() == QVariant::UserType) {
            typeName = QMetaType::typeName(v.userType());
        } else {
            typeName = v.typeName();
        }

        Nodes nodes;

        if (!typeName) {
            DEBUG << "variantToPropertyNode: No type name?! Going ahead anyway" << endl;
            nodes << Node::fromVariant(v);
            return nodes;
        }

        DEBUG << "variantToPropertyNode: typeName = " << typeName << endl;
        
        if (m_cb->canExtractContainer(typeName)) {

            QVariantList list = m_cb->extractContainer(typeName, v);
            ContainerBuilder::ContainerKind k = m_cb->getContainerKind(typeName);

            if (k == ContainerBuilder::SequenceKind) {
                Node node = listToPropertyNode(list, map, follow);
                if (node != Node()) nodes << node;
                
            } else if (k == ContainerBuilder::SetKind) {
                foreach (QVariant member, list) {
                    //!!! this doesn't "feel" right -- what about sets of sets, etc? I suppose sets of sequences might work?
                    nodes += variantToPropertyNodeList(member, map, follow);
                }
            }
            
        } else if (m_ob->canExtract(typeName)) {
            QObject *obj = m_ob->extract(typeName, v);
            if (obj) {
                nodes << objectToPropertyNode(obj, map, follow);
            } else {
                DEBUG << "variantToPropertyNode: Note obtained NULL object" << endl;
            }
            
        } else if (QString(typeName).contains("*") ||
                   QString(typeName).endsWith("Star")) {
            // do not attempt to write binary pointers!
            return Nodes();

        } else {
            Node node = Node::fromVariant(v);
            if (node != Node()) nodes << node;
        }

        return nodes;
    }

    Node objectToPropertyNode(QObject *o, ObjectNodeMap &map, bool follow) {

        Node pnode;
        QVariant uriv = o->property("uri");
            
        if (uriv != QVariant()) {
            // If our object has a URI already, then we should use
            // that and not attempt to store the object again.  If the
            // user wanted the object to be stored in spite of its
            // having a URI already assigned, they needed to have
            // included it in a storeObjects tree or storeAllObjects
            // list.  If it was in such a list, then it will be (or
            // have been) stored anyway.

            //!!! No -- the above is not right.  We should store it if
            //!!! it is not in the map, even if it has a URI already.
            //!!! Otherwise we incorrectly store objects that are
            //!!! found as properties of other objects, that have not
            //!!! been stored, and that have an explicit URI -- the
            //!!! "document" object in our classical data test set is
            //!!! an example, since the uri is intrinsic there but we
            //!!! also want to store other stuff about it (e.g. its
            //!!! type)

            if (uriv.type() == QVariant::Url) {
                pnode = uriv.toUrl();
            } else {
                pnode = m_s->expand(uriv.toString());
            }
        } else {
            if (!map.contains(o)) {
                // If the object is not in the map, then it was not
                // among the objects passed to the store method.  That
                // means it should (if follow is true) be stored, and
                // as a blank node unless the blank node policy says
                // otherwise.
                if (follow) {
                    map[o] = storeSingle(o, map, true,
                                         (m_bp == BlankNodesAsNeeded)); //!!! crap api
                }
            } else if (map[o] == Node()) {
                // If it's in the map but with no node assigned, then
                // we haven't stored it yet.  If follow is true, we
                // can store it to obtain its URI or node ID.
                if (follow) {
                    map[o] = storeSingle(o, map, true, false); //!!! crap api
                }
            }
            if (map.contains(o) && map[o] != Node()) {
                pnode = map[o];
            }
        }

        return pnode;
    }

    Node listToPropertyNode(QVariantList list, ObjectNodeMap &map, bool follow) {

        DEBUG << "listToPropertyNode: have " << list.size() << " items" << endl;

        Node node, first, previous;

        foreach (QVariant v, list) {

            Nodes pnodes = variantToPropertyNodeList(v, map, follow);
            if (pnodes.empty()) {
                DEBUG << "listToPropertyNode: Obtained nil Node in list, skipping!" << endl;
                continue;
            } else if (pnodes.size() > 1) {
                DEBUG << "listToPropertyNode: Found set within sequence, can't handle this, using first element only" << endl; //!!!???
            }

            Node pnode = pnodes[0];

            node = m_s->addBlankNode();
            if (first == Node()) first = node;

            if (previous != Node()) {
                m_s->add(Triple(previous, "rdf:rest", node));
            }

            m_s->add(Triple(node, "rdf:first", pnode));
            previous = node;
        }

        if (node != Node()) {
            m_s->add(Triple(node, "rdf:rest", m_s->expand("rdf:nil")));
        }

        return first;
    }

    Node stringListToPropertyNode(QStringList list) {

        Node node, first, previous;

        foreach (QString s, list) {

            node = m_s->addBlankNode();
            if (first == Node()) first = node;

            if (previous != Node()) {
                m_s->add(Triple(previous, "rdf:rest", node));
            }

            m_s->add(Triple(node, "rdf:first", Node::fromVariant(s)));
            previous = node;
        }

        if (node != Node()) {
            m_s->add(Triple(node, "rdf:rest", m_s->expand("rdf:nil")));
        }

        return first;
    }

    QObject *loadSingle(Node node, QObject *parent, NodeObjectMap &map,
                        QString classHint, bool follow) {

	if (map.contains(node)) {
	    return map[node];
	}

        // The RDF may contain a more precise type specification than
        // we have here.  For example, if we have been called as part
        // of a process of loading something declared as container of
        // base-class pointers, but each individual object is actually
        // of a derived class, then the RDF should specify the derived
        // class but we will only have been passed the base class.  So
        // we always use the RDF type if available, and refer to the
        // passed-in type only if that fails.

        QString className;

        Triple t = m_s->matchFirst(Triple(node, "a", Node()));
        if (t.c.type == Node::URI) {
            try {
                className = typeUriToClassName(t.c.value);
            } catch (UnknownTypeException) {
                DEBUG << "loadSingle: Unknown type URI " << t.c.value << endl;
                if (classHint == "") throw;
                DEBUG << "(falling back to object class hint " << classHint << ")" << endl;
                className = classHint;
            }
        } else {
            DEBUG << "loadSingle: No type URI for " << node << endl;
            if (classHint == "") throw UnknownTypeException("");
            DEBUG << "(falling back to object class hint " << classHint << ")" << endl;
            className = classHint;
        }
        
        if (!m_ob->knows(className)) {
            DEBUG << "loadSingle: Unknown object class " << className << endl;
            throw UnknownTypeException(className);
        }
    
        DEBUG << "Making object " << node.value << " of type "
              << className << " with parent " << parent << endl;

	QObject *o = m_ob->build(className, parent);
	if (!o) throw ConstructionFailedException(className);
	
        if (node.type == Node::URI) {
            o->setProperty("uri", m_s->expand(node.value));
        }
        map[node] = o;

	loadProperties(o, node, map, follow);

        callLoadCallbacks(map, node, o);

	return o;
    }

    void callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o) {
        foreach (LoadCallback *cb, m_loadCallbacks) {
            //!!! this doesn't really work out -- the callback doesn't know whether we're loading a single object or a graph; it may load any number of other related objects into the map, and if we were only supposed to load a single object, we won't know what to do with them afterwards (at the moment we just leak them)

            cb->loaded(m_m, map, node, o);
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

    QObject *loadTree(Node node, QObject *parent, NodeObjectMap &map) {

        QObject *o;
        try {
            o = loadSingle(node, parent, map, "", true); //!!!??? or false?
        } catch (UnknownTypeException e) {
            o = 0;
        }

        if (o) {
            Triples childTriples = m_s->match
                (Triple(Node(), m_relationshipPrefix + "parent", node));
            foreach (Triple t, childTriples) {
                loadTree(t.a, o, map);
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
            if (uriVar.type() == QVariant::Url) {
                node = uriVar.toUrl();
            } else {
                node = m_s->expand(uriVar.toString());
            }
        } else if (blank) {
            node = m_s->addBlankNode();
        } else {
            QString prefix;
            if (m_typeUriPrefixMap.contains(className)) {
                prefix = m_typeUriPrefixMap[className];
            } else {
                QString tag = className.toLower() + "_";
                tag.replace("::", "_");
                prefix = ":" + tag;
            }
            QUrl uri = m_s->getUniqueUri(prefix);
            o->setProperty("uri", uri); //!!! document this
            node = uri;
        }

        map[o] = node;

        m_s->add(Triple(node, "a", classNameToTypeUri(className)));

        if (o->parent() &&
            map.contains(o->parent()) &&
            map[o->parent()] != Node()) {

            m_s->add(Triple(node, m_relationshipPrefix + "parent",
                            map[o->parent()]));
        }

        storeProperties(o, node, map, follow);

        callStoreCallbacks(map, o, node);

        return node;
    }

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node) {
        foreach (StoreCallback *cb, m_storeCallbacks) {
            cb->stored(m_m, map, o, node);
        }
    }

    Node storeTree(QObject *o, ObjectNodeMap &map) {

        if (m_osp == StoreObjectsWithURIs) {
            if (o->property("uri") == QVariant()) return QUrl();
        }

        Node me = storeSingle(o, map, true);

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
ObjectMapper::addTypeMapping(QString className, QString uri)
{
    m_d->addTypeMapping(className, uri);
}

void
ObjectMapper::addTypeMapping(QString className, QUrl uri)
{
    m_d->addTypeMapping(className, uri);
}

void
ObjectMapper::addTypeUriPrefixMapping(QString className, QString prefix)
{
    m_d->addTypeUriPrefixMapping(className, prefix);
}

void
ObjectMapper::addPropertyMapping(QString className, QString propertyName, QString uri)
{
    m_d->addPropertyMapping(className, propertyName, uri);
}

void
ObjectMapper::addPropertyMapping(QString className, QString propertyName, QUrl uri)
{
    m_d->addPropertyMapping(className, propertyName, uri);
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
ObjectMapper::setBlankNodePolicy(BlankNodePolicy policy)
{
    m_d->setBlankNodePolicy(policy);
}

ObjectMapper::BlankNodePolicy
ObjectMapper::getBlankNodePolicy() const
{
    return m_d->getBlankNodePolicy();
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
ObjectMapper::storeAllObjects(QObjectList list)
{
    m_d->storeAllObjects(list);
}

QObject *
ObjectMapper::loadFrom(Node source, NodeObjectMap &map)
{
    return m_d->loadFrom(source, map);
}

Node
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


	
