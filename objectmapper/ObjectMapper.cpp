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

static Uri defaultTypePrefix("http://breakfastquay.com/rdf/dataquay/objectmapper/type/");
static Uri defaultPropertyPrefix("http://breakfastquay.com/rdf/dataquay/objectmapper/property/");
static Uri defaultRelationshipPrefix("http://breakfastquay.com/rdf/dataquay/objectmapper/relationship/");

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

    Uri getObjectTypePrefix() const {
        return m_typePrefix;
    }

    void setObjectTypePrefix(Uri prefix) {
        m_typePrefix = prefix;
    }

    Uri getPropertyPrefix() const {
        return m_propertyPrefix;
    }

    void setPropertyPrefix(Uri prefix) { 
        m_propertyPrefix = prefix;
    }

    Uri getRelationshipPrefix() const {
        return m_relationshipPrefix;
    }

    void setRelationshipPrefix(Uri prefix) { 
        m_relationshipPrefix = prefix;
    }

    void addTypeMapping(QString className, QString uri) {
        addTypeMapping(className, m_s->expand(uri));
    }

    void addTypeMapping(QString className, Uri uri) {
        m_typeMap[uri] = className;
        m_typeRMap[className] = uri;
    }

    void addTypeUriPrefixMapping(QString className, QString prefix) {
        addTypeUriPrefixMapping(className, m_s->expand(prefix));
    }

    void addTypeUriPrefixMapping(QString className, Uri prefix) {
        m_typeUriPrefixMap[className] = prefix;
    }

    void addPropertyMapping(QString className, QString propertyName, QString uri) {
        addPropertyMapping(className, propertyName, m_s->expand(uri));
    }

    void addPropertyMapping(QString className, QString propertyName, Uri uri) {
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

    void loadProperties(QObject *o, Uri uri) {
        NodeObjectMap map;
        loadProperties(map, o, uri, false, 0);
    }

    void storeProperties(QObject *o, Uri uri) {
        ObjectNodeMap map;
        storeProperties(map, o, uri, false);
    }

    QObject *loadObject(Uri uri, QObject *parent) {
        NodeObjectMap map;
	return loadSingle(map, uri, parent, "", false, 0);
    }

    QObject *loadObjectTree(Uri root, QObject *parent) {
        NodeObjectMap map;
        QObject *rv = loadTree(map, root, parent);
        loadConnections(map);
        return rv;
    }

    QObject *loadAllObjects(QObject *parent) {

        NodeObjectMap map;

        Triples candidates = m_s->match(Triple(Node(), "a", Node()));

        foreach (Triple t, candidates) {
            
            if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

            Uri objectUri(t.a.value);
            Uri typeUri(t.c.value);

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
            
            loadFrom(map, objectUri, className);
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

    Uri storeObject(QObject *o) {
        ObjectNodeMap map;
        return Uri(storeSingle(map, o, false).value);
    }

    Uri storeObjectTree(QObject *root) {
        ObjectNodeMap map;
        foreach (QObject *o, root->findChildren<QObject *>()) {
            map[o] = Node();
        }
        return Uri(storeTree(map, root).value);
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
            storeTree(map, o);
        }
    }

    QObject *loadFrom(NodeObjectMap &map, Node node, QString classHint = "") {

        // We construct the property object with m_propertyPrefix
        // rather than m_relationshipPrefix, so that we can reuse it
        // in loadProperties (passed through as the final argument).
        // PropertyObjects can look up properties with any prefix if
        // we make them explicit (as we will do in a moment); they
        // just default to the one in the ctor

	CacheingPropertyObject po(m_s, m_propertyPrefix.toString(), node);

        QString followsProp = m_relationshipPrefix.toString() + "follows";
	if (po.hasProperty(followsProp)) {
            try {
                loadFrom(map, po.getPropertyNode(followsProp));
            } catch (UnknownTypeException) { }
	}

        QObject *parent = 0;
        QString parentProp = m_relationshipPrefix.toString() + "parent";
	if (po.hasProperty(parentProp)) {
            try {
                parent = loadFrom(map, po.getPropertyNode(parentProp));
            } catch (UnknownTypeException) {
                parent = 0;
            }
	}

        //!!! NB. as this stands, if the RDF is "wrong" containing the
        //!!! wrong type for a property of this, we will fail the
        //!!! whole thing with an UnknownTypeException -- is that the
        //!!! right thing to do? consider

        QObject *o = loadSingle(map, node, parent, classHint, true, &po);
        return o;
    }

    Node store(ObjectNodeMap &map, QObject *o) {
        return storeSingle(map, o, true);
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
    Uri m_typePrefix;
    Uri m_propertyPrefix;
    Uri m_relationshipPrefix;
    QList<LoadCallback *> m_loadCallbacks;
    QList<StoreCallback *> m_storeCallbacks;
    QMap<Uri, QString> m_typeMap;
    QHash<QString, Uri> m_typeRMap;
    QHash<QString, Uri> m_typeUriPrefixMap;
    QHash<QString, QHash<Uri, QString> > m_propertyMap;
    QHash<QString, QHash<QString, Uri> > m_propertyRMap;

    QString typeUriToClassName(Uri typeUri);
    Uri classNameToTypeUri(QString className);

    QObject *loadTree(NodeObjectMap &map, Node node, QObject *parent);
    QObject *loadSingle(NodeObjectMap &map, Node node, QObject *parent,
                        QString classHint, bool follow,
                        CacheingPropertyObject *po);

    void callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o);

    void loadConnections(NodeObjectMap &map);

    void loadProperties(NodeObjectMap &map, QObject *o, Node node, bool follow,
                        CacheingPropertyObject *po);
    QVariant propertyNodeListToVariant(NodeObjectMap &map, QString typeName,
                                       Nodes pnodes, bool follow);
    QObject *propertyNodeToObject(NodeObjectMap &map, QString typeName,
                                  Node pnode);
    QVariant propertyNodeToVariant(NodeObjectMap &map, QString typeName,
                                   Node pnode);
    QVariantList propertyNodeToList(NodeObjectMap &map, QString typeName,
                                    Node pnode, bool follow);

    Node storeTree(ObjectNodeMap &map, QObject *o);
    Node storeSingle(ObjectNodeMap &map, QObject *o, bool follow, bool blank = false);

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node);

    void storeProperties(ObjectNodeMap &map, QObject *o, Node node, bool follow);            
    void removeOldPropertyNodes(Node node, Uri propertyUri);
    Nodes variantToPropertyNodeList(ObjectNodeMap &map, QVariant v, bool follow);
    Node objectToPropertyNode(ObjectNodeMap &map, QObject *o, bool follow);
    Node listToPropertyNode(ObjectNodeMap &map, QVariantList list, bool follow);
};

QString
ObjectMapper::D::typeUriToClassName(Uri typeUri)
{
    if (m_typeMap.contains(typeUri)) {
        return m_typeMap[typeUri];
    }
    QString s = typeUri.toString();
    if (!s.startsWith(m_typePrefix.toString())) {
        throw UnknownTypeException(s);
    }
    s = s.right(s.length() - m_typePrefix.length());
    s = s.replace("/", "::");
    return s;
}

Uri
ObjectMapper::D::classNameToTypeUri(QString className)
{
    Uri typeUri;
    if (m_typeRMap.contains(className)) {
        typeUri = m_typeRMap[className];
    } else {
        typeUri = Uri(QString(m_typePrefix.toString() + className).replace("::", "/"));
    }
    return typeUri;
}

void
ObjectMapper::D::loadProperties(NodeObjectMap &map, QObject *o, Node node,
                                bool follow, CacheingPropertyObject *po)
{
    QString cname = o->metaObject()->className();

    bool myPo = false;
    if (!po) {
        po = new CacheingPropertyObject(m_s, m_propertyPrefix.toString(), node);
        myPo = true;
    }

    for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

        QMetaProperty property = o->metaObject()->property(i);

        if (!property.isStored() ||
            !property.isReadable() ||
            !property.isWritable()) {
            continue;
        }

        QString pname = property.name(); // name to use when setting on QObject
        QString plookup(pname);          // name or URI for PropertyObject
        Nodes pnodes;
        
        if (m_propertyRMap[cname].contains(pname)) {
            plookup = m_propertyRMap[cname][pname].toString();
        }

        if (!po->hasProperty(plookup)) continue;
        pnodes = po->getPropertyNodeList(plookup);
        if (pnodes.empty()) continue;
        
        QString typeName = property.typeName();
        QVariant value = propertyNodeListToVariant
            (map, typeName, pnodes, follow);

        if (!value.isValid()) continue;

        QByteArray pnba = pname.toLocal8Bit();
        if (!o->setProperty(pnba.data(), value)) {
            DEBUG << "loadProperties: Property set failed "
                  << "for property " << pname << " of type "
                  << typeName << " (" << property.userType()
                  << ") to value of type " << value.type() 
                  << " and value " << value
                  << " from (first) node " << pnodes[0].value
                  << endl;
            DEBUG << "loadProperties: If the RDF datatype is correct, check "
                  << "[1] that the datatype is known to Dataquay::Node for "
                  << "node-variant conversion"
                  << "(datatype is one of the standard set, "
                  << "or registered with Node::registerDatatype) and "
                  << "[2] that the Q_PROPERTY type declaration " << property.typeName()
                  << " matches the name passed to qRegisterMetaType (including namespace)"
                  << endl;
            std::cerr << "ObjectMapper::loadProperties: Failed to set property on object, ignoring" << std::endl;
        }
    }

    if (myPo) delete po;
}

QVariant
ObjectMapper::D::propertyNodeListToVariant(NodeObjectMap &map, 
                                           QString typeName, Nodes pnodes,
                                           bool follow)
{
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
                propertyNodeToList(map, inContainerType, firstNode, follow);
            return m_cb->injectContainer(typeName, list);

        } else if (k == ContainerBuilder::SetKind) {
            QVariantList list;
            foreach (Node pnode, pnodes) {
                Nodes sublist;
                sublist << pnode;
                list << propertyNodeListToVariant(map, inContainerType, sublist,
                                                  follow);
            }
            return m_cb->injectContainer(typeName, list);

        } else {
            return QVariant();
        }

    } else if (m_ob->canInject(typeName)) {

        if (follow) {
            QObject *obj = propertyNodeToObject(map, typeName, firstNode);
            QVariant v;
            if (obj) {
                v = m_ob->inject(typeName, obj);
                if (v == QVariant()) {
                    DEBUG << "propertyNodeListToVariant: "
                          << "Type of node " << firstNode
                          << " is incompatible with expected "
                          << typeName << endl;
                    std::cerr << "ObjectMapper::propertyNodeListToVariant: "
                              << "Incompatible node type, ignoring" << std::endl;
                    delete obj;
                }
            }
            return v;
        } else {
            return QVariant();
        }

    } else if (QString(typeName).contains("*") ||
               QString(typeName).endsWith("Star")) {
        // do not attempt to read binary pointers!
        return QVariant();

    } else {

        return propertyNodeToVariant(map, typeName, firstNode);
    }
}


QVariant
ObjectMapper::D::propertyNodeToVariant(NodeObjectMap &map, 
                                       QString typeName, Node pnode)
{
    // Usually we can take the default conversion from node to
    // QVariant.  But in two cases this will fail in ways we need to
    // correct:
    // 
    // - The node is an untyped literal and the property is known to
    // have a type other than string (default conversion would produce
    // a string)
    // 
    // - The node is a URI and the property is known to have a type
    // other than Uri (several legitimate property types could
    // reasonably receive data from a URI node)
    //
    // One case should not be corrected:
    //
    // - The node has a type but it doesn't match that of the property
    //
    // We have to fail that one, because we're not in any position to
    // do general type conversion here.

    if (typeName == "") {
        return pnode.toVariant();
    }

    bool acceptDefault = true;

    if (pnode.type == Node::URI && typeName != Uri::metaTypeName()) {

        DEBUG << "ObjectMapper::propertyNodeListToVariant: "
              << "Non-URI property is target for RDF URI, converting" << endl;
        
        acceptDefault = false;

    } else if (pnode.type == Node::Literal &&
               pnode.datatype == Uri() && // no datatype
               typeName != QMetaType::typeName(QMetaType::QString)) {

        DEBUG << "ObjectMapper::propertyNodeListToVariant: "
              << "No datatype for RDF literal, deducing from typename \""
              << typeName << "\"" << endl;

        acceptDefault = false;
    }

    if (acceptDefault) {
        return pnode.toVariant();
    }

    QByteArray ba = typeName.toLocal8Bit();
    int metatype = QMetaType::type(ba.data());
    if (metatype != 0) return pnode.toVariant(metatype);
    else return pnode.toVariant();
}

QObject *
ObjectMapper::D::propertyNodeToObject(NodeObjectMap &map, QString typeName, Node pnode)
{
    QString classHint;
    if (typeName != "") {
        classHint = m_ob->getClassNameForPointerName(typeName);
        DEBUG << "typeName " << typeName << " -> classHint " << classHint << endl;
    }

    if (pnode.type == Node::URI || pnode.type == Node::Blank) {
        return loadFrom(map, pnode, classHint);
    }

    return 0;
}

QVariantList
ObjectMapper::D::propertyNodeToList(NodeObjectMap &map, 
                                    QString typeName, Node pnode,
                                    bool follow)
{
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
            (map, typeName, fnodes, follow);

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

void
ObjectMapper::D::storeProperties(ObjectNodeMap &map, QObject *o,
                                 Node node, bool follow)
{
    QString cname = o->metaObject()->className();
    PropertyObject po(m_s, m_propertyPrefix.toString(), node);

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

        Nodes pnodes = variantToPropertyNodeList(map, value, follow);

        Uri puri;
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
            
void
ObjectMapper::D::removeOldPropertyNodes(Node node, Uri propertyUri) 
{
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

Nodes
ObjectMapper::D::variantToPropertyNodeList(ObjectNodeMap &map, QVariant v, bool follow)
{
    const char *typeName = 0;

//!!! unnecessary    if (v.type() == QVariant::UserType) {
    typeName = QMetaType::typeName(v.userType());
//    } else {
//        typeName = v.typeName();
//    }

    Nodes nodes;

    if (!typeName) {
        std::cerr << "ObjectMapper::variantToPropertyNodeList: No type name?! Going ahead anyway" << std::endl;
        nodes << Node::fromVariant(v);
        return nodes;
    }

    DEBUG << "variantToPropertyNodeList: typeName = " << typeName << endl;
        
    if (m_cb->canExtractContainer(typeName)) {

        QVariantList list = m_cb->extractContainer(typeName, v);
        ContainerBuilder::ContainerKind k = m_cb->getContainerKind(typeName);

        if (k == ContainerBuilder::SequenceKind) {
            Node node = listToPropertyNode(map, list, follow);
            if (node != Node()) nodes << node;
                
        } else if (k == ContainerBuilder::SetKind) {
            foreach (QVariant member, list) {
                //!!! this doesn't "feel" right -- what about sets of sets, etc? I suppose sets of sequences might work?
                nodes += variantToPropertyNodeList(map, member, follow);
            }
        }
            
    } else if (m_ob->canExtract(typeName)) {
        QObject *obj = m_ob->extract(typeName, v);
        if (obj) {
            Node n = objectToPropertyNode(map, obj, follow);
            if (n != Node()) nodes << n;
        } else {
            DEBUG << "variantToPropertyNodeList: Note: obtained NULL object from variant" << endl;
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

Node
ObjectMapper::D::objectToPropertyNode(ObjectNodeMap &map, QObject *o, bool follow)
{
    Node pnode;

    DEBUG << "objectToPropertyNode: " << o << ", follow = " << follow << endl;

    //!!! "follow" argument is not very helpful, better to have a
    //!!! separate function for storing object-as-property vs
    //!!! object-as-object?

    if (follow) {
        // Always (at least try to) store the object if it is not
        // in our map already, or if it is in the map but with no
        // assigned node -- i.e. if it has not already been
        // stored.  Even if it already has a URI, we should store
        // it if it has not already been stored because it may be
        // an object for which the URI is an intrinsic property.

        if (!map.contains(o)) {
            DEBUG << "objectToPropertyNode: Object is not in map" << endl;
            // If the object is not in the map, then it was not
            // among the objects passed to the store method.  That
            // means it should (if follow is true) be stored, and
            // as a blank node unless the blank node policy says
            // otherwise.
            map[o] = storeSingle(map, o, true,
                                 (m_bp == BlankNodesAsNeeded)); //!!! crap api
        } else if (map[o] == Node()) {
            DEBUG << "objectToPropertyNode: Object has no node in map" << endl;
            // If it's in the map but with no node assigned, then
            // we haven't stored it yet.  If follow is true, we
            // can store it to obtain its URI or node ID.
            map[o] = storeSingle(map, o, true, false); //!!! crap api
        }
    }

    if (map.contains(o) && map[o] != Node()) {
        pnode = map[o];
    }

    return pnode;
}

Node
ObjectMapper::D::listToPropertyNode(ObjectNodeMap &map, QVariantList list, bool follow)
{
    DEBUG << "listToPropertyNode: have " << list.size() << " items" << endl;

    Node node, first, previous;

    foreach (QVariant v, list) {

        Nodes pnodes = variantToPropertyNodeList(map, v, follow);
        if (pnodes.empty()) {
            DEBUG << "listToPropertyNode: Obtained nil Node in list, skipping!" << endl;
            continue;
        } else if (pnodes.size() > 1) {
            std::cerr << "ObjectMapper::listToPropertyNode: Found set within sequence, can't handle this, using first element only" << std::endl; //!!!???
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

QObject *
ObjectMapper::D::loadSingle(NodeObjectMap &map, Node node, QObject *parent,
                            QString classHint, bool follow,
                            CacheingPropertyObject *po)
{
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
            className = typeUriToClassName(Uri(t.c.value));
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
        std::cerr << "ObjectMapper::loadSingle: Unknown object class "
                  << className.toStdString() << std::endl;
        throw UnknownTypeException(className);
    }
    
    DEBUG << "Making object " << node.value << " of type "
          << className << " with parent " << parent << endl;

    QObject *o = m_ob->build(className, parent);
    if (!o) throw ConstructionFailedException(className);
	
    if (node.type == Node::URI) { // not a blank node
        o->setProperty("uri", QVariant::fromValue(m_s->expand(node.value)));
    }

    map[node] = o;

    loadProperties(map, o, node, follow, po);

    callLoadCallbacks(map, node, o);

    return o;
}

void
ObjectMapper::D::callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o)
{
    foreach (LoadCallback *cb, m_loadCallbacks) {
        //!!! this doesn't really work out -- the callback doesn't know whether we're loading a single object or a graph; it may load any number of other related objects into the map, and if we were only supposed to load a single object, we won't know what to do with them afterwards (at the moment we just leak them)

        cb->loaded(m_m, map, node, o);
    }
}

void
ObjectMapper::D::loadConnections(NodeObjectMap &map)
{
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
          " } ").arg(m_relationshipPrefix.toString()));

    foreach (Dictionary d, rs) {

        Uri sourceUri(d["sobj"].value);
        Uri targetUri(d["tobj"].value);
        if (!map.contains(sourceUri) || !map.contains(targetUri)) continue;

        QString sourceSignal = signalTemplate.replace("xxx", d["ssig"].value);
        QString targetSlot = slotTemplate.replace("xxx", d["tslot"].value);

        QByteArray sigba = sourceSignal.toLocal8Bit();
        QByteArray slotba = targetSlot.toLocal8Bit();
                
        QObject::connect(map[sourceUri], sigba.data(),
                         map[targetUri], slotba.data());
    }
}

QObject *
ObjectMapper::D::loadTree(NodeObjectMap &map, Node node, QObject *parent)
{

    QObject *o;
    try {
        o = loadSingle(map, node, parent, "", true, 0); //!!!??? or false?
    } catch (UnknownTypeException e) {
        o = 0;
    }

    if (o) {
        Triples childTriples = m_s->match
            (Triple(Node(), m_relationshipPrefix.toString() + "parent", node));
        foreach (Triple t, childTriples) {
            loadTree(map, t.a, o);
        }
    }

    return o;
}

Node
ObjectMapper::D::storeSingle(ObjectNodeMap &map, QObject *o, bool follow, bool blank)
{ //!!! crap api
    DEBUG << "storeSingle: blank = " << blank << endl;

    if (map.contains(o) && map[o] != Node()) return map[o];

    QString className = o->metaObject()->className();

    Node node;
    QVariant uriVar = o->property("uri");

    if (uriVar != QVariant()) {
        if (Uri::isUri(uriVar)) {
            node = uriVar.value<Uri>();
        } else {
            node = m_s->expand(uriVar.toString());
        }
    } else if (blank) {
        node = m_s->addBlankNode();
    } else {
        QString prefix;
        if (m_typeUriPrefixMap.contains(className)) {
            prefix = m_typeUriPrefixMap[className].toString();
        } else {
            QString tag = className.toLower() + "_";
            tag.replace("::", "_");
            prefix = ":" + tag;
        }
        Uri uri = m_s->getUniqueUri(prefix);
        o->setProperty("uri", QVariant::fromValue(uri)); //!!! document this
        node = uri;
    }

    map[o] = node;

    m_s->add(Triple(node, "a", classNameToTypeUri(className)));

    if (o->parent() &&
        map.contains(o->parent()) &&
        map[o->parent()] != Node()) {

        m_s->add(Triple(node, m_relationshipPrefix.toString() + "parent",
                        map[o->parent()]));
    }

    storeProperties(map, o, node, follow);

    callStoreCallbacks(map, o, node);

    return node;
}

void
ObjectMapper::D::callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node)
{
    foreach (StoreCallback *cb, m_storeCallbacks) {
        cb->stored(m_m, map, o, node);
    }
}

Node
ObjectMapper::D::storeTree(ObjectNodeMap &map, QObject *o)
{
    if (m_osp == StoreObjectsWithURIs) {
        if (o->property("uri") == QVariant()) return Uri();
    }

    Node me = storeSingle(map, o, true);

    foreach (QObject *c, o->children()) {
        storeTree(map, c);
    }
        
    return me;
}

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

Uri
ObjectMapper::getObjectTypePrefix() const
{
    return m_d->getObjectTypePrefix();
}

void
ObjectMapper::setObjectTypePrefix(Uri prefix)
{
    m_d->setObjectTypePrefix(prefix);
}

Uri
ObjectMapper::getPropertyPrefix() const
{
    return m_d->getPropertyPrefix();
}

void
ObjectMapper::setPropertyPrefix(Uri prefix)
{
    m_d->setPropertyPrefix(prefix);
}

Uri
ObjectMapper::getRelationshipPrefix() const
{
    return m_d->getRelationshipPrefix();
}

void
ObjectMapper::setRelationshipPrefix(Uri prefix)
{
    m_d->setRelationshipPrefix(prefix);
}

void
ObjectMapper::addTypeMapping(QString className, QString uri)
{
    m_d->addTypeMapping(className, uri);
}

void
ObjectMapper::addTypeMapping(QString className, Uri uri)
{
    m_d->addTypeMapping(className, uri);
}

void
ObjectMapper::addTypeUriPrefixMapping(QString className, QString prefix)
{
    m_d->addTypeUriPrefixMapping(className, prefix);
}

void
ObjectMapper::addTypeUriPrefixMapping(QString className, Uri prefix)
{
    m_d->addTypeUriPrefixMapping(className, prefix);
}

void
ObjectMapper::addPropertyMapping(QString className, QString propertyName, QString uri)
{
    m_d->addPropertyMapping(className, propertyName, uri);
}

void
ObjectMapper::addPropertyMapping(QString className, QString propertyName, Uri uri)
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
ObjectMapper::loadProperties(QObject *o, Uri uri)
{
    m_d->loadProperties(o, uri);
}

void
ObjectMapper::storeProperties(QObject *o, Uri uri)
{
    m_d->storeProperties(o, uri);
}

QObject *
ObjectMapper::loadObject(Uri uri, QObject *parent)
{
    return m_d->loadObject(uri, parent);
}

QObject *
ObjectMapper::loadObjectTree(Uri rootUri, QObject *parent)
{
    return m_d->loadObjectTree(rootUri, parent);
}

QObject *
ObjectMapper::loadAllObjects(QObject *parent)
{
    return m_d->loadAllObjects(parent);
}

Uri
ObjectMapper::storeObject(QObject *o)
{
    return m_d->storeObject(o);
}

Uri
ObjectMapper::storeObjectTree(QObject *root)
{
    return m_d->storeObjectTree(root);
}

void
ObjectMapper::storeAllObjects(QObjectList list)
{
    m_d->storeAllObjects(list);
}

QObject *
ObjectMapper::loadFrom(NodeObjectMap &map, Node source)
{
    return m_d->loadFrom(map, source);
}

Node
ObjectMapper::store(ObjectNodeMap &map, QObject *o)
{
    return m_d->store(map, o);
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


	
