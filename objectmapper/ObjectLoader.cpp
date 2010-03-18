/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2010 Chris Cannam.
  
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

#include "ObjectLoader.h"

#include "ObjectBuilder.h"
#include "ContainerBuilder.h"
#include "PropertyObject.h"
#include "TypeMapping.h"
#include "Store.h"

#include "ObjectMapperExceptions.h"

#include <QMetaProperty>
#include <QSet>

#include "Debug.h"

namespace Dataquay {

class ObjectLoader::D
{
    typedef QSet<Node> NodeSet;

public:
    D(ObjectLoader *m, Store *s) :
        m_m(m),
        m_ob(ObjectBuilder::getInstance()),
        m_cb(ContainerBuilder::getInstance()),
        m_s(s),
        m_fp(FollowNone),
        m_ap(IgnoreAbsentProperties) {
    }

    Store *getStore() {
        return m_s;
    }

    void setTypeMapping(const TypeMapping &tm) {
	m_tm = tm;
    }

    const TypeMapping &getTypeMapping() const {
	return m_tm;
    }

    void setFollowPolicy(FollowPolicy fp) {
        m_fp = fp; 
    }

    FollowPolicy getFollowPolicy() const {
        return m_fp;
    }

    void setAbsentPropertyPolicy(AbsentPropertyPolicy ap) {
        m_ap = ap;
    }

    AbsentPropertyPolicy getAbsentPropertyPolicy() const {
        return m_ap;
    }

/*
    void loadProperties(QObject *o, Uri uri) {
        NodeObjectMap map;
        loadProperties(map, o, uri, false, 0);
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
            
            if (t.c.type != Node::URI) continue;

            Uri typeUri(t.c.value);

            QString className;

            try {
                className = m_tm.synthesiseClassForTypeUri(typeUri);
            } catch (UnknownTypeException) {
                continue;
            }

            if (!m_ob->knows(className)) {
                DEBUG << "Can't construct type " << typeUri << endl;
                continue;
            }
            
            loadFrom(map, t.a, className);
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
        if (!superRoot) {
            superRoot = new QObject;
            superRoot->setObjectName("Dataquay ObjectLoader synthetic parent");
        }
        foreach (QObject *o, rootObjects) {
            o->setParent(superRoot);
        }
        return superRoot;
    }

    QObject *loadFrom(NodeObjectMap &map, Node node, QString classHint = "") {

        DEBUG << "loadFrom: " << node << " (classHint " << classHint << ")" << endl;

        //!!! should we return here before traversing parent and siblings? or only return after loadSingle call as we did before?
        if (map.contains(node)) {
            DEBUG << "loadFrom: " << node << " already loaded" << endl;
            return map[node];
        }

        // We construct the property object with the property prefix
        // rather than relationship prefix, so that we can reuse it in
        // loadProperties (passed through as the final argument).
        // PropertyObjects can look up properties with any prefix if
        // we make them explicit (as we will do in a moment); they
        // just default to the one in the ctor

	//!!! would be nice to be able to skip the parent and siblings if so told
	
	CacheingPropertyObject po(m_s, m_tm.getPropertyPrefix().toString(), node);

	//!!! This doesn't seem to be written in ObjectStorer!  Write
	// a test case for it
        QString followsProp = m_tm.getRelationshipPrefix().toString() + "follows";
	if (po.hasProperty(followsProp)) {
            try {
                loadFrom(map, po.getPropertyNode(followsProp));
            } catch (UnknownTypeException) { }
	}

        QObject *parent = 0;
        QString parentProp = m_tm.getRelationshipPrefix().toString() + "parent";
	if (po.hasProperty(parentProp)) {
            DEBUG << "loadFrom: node " << node << " has parent, loading that first" << endl;
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
*/
    QObject *load(Node node) {
        NodeObjectMap map;
        NodeSet examined;
        map.insert(node, 0);
        QObject *o = load(map, examined, node);
        // do not catch UnknownTypeException
        //!!! leaks any other stuff in map! should not be following anything here!
        return o;
    }
    
    void load(Nodes nodes, NodeObjectMap &map) {
        
        NodeSet examined;

        foreach (Node n, nodes) {
            if (!map.contains(n)) {
                map.insert(n, 0);
            }
        }

        foreach (Node n, nodes) {
            QObject *o = 0;
            QObject *existing = map.value(n);
            try {
                if (existing) {
                    // If there is something in the map for this node
                    // already, we may want to delete it (if the
                    // corresponding data has gone from the store).
                    // But we won't be able to tell if that happens
                    // unless we explicitly test for the node class
                    // type now, because that test (which normally
                    // happens in allocateObject) is circumvented if
                    // there is an object already available in the
                    // map.  So call this here, purely so we can catch
                    // any UnknownTypeException that occurs
                    getClassNameForNode(n);
                }
                o = load(map, examined, n);
            } catch (UnknownTypeException) {
                o = 0;
            }
            if (!o) {
                if (existing) {
                    DEBUG << "load: Node " << n
                          << " no longer loadable, deleting object" << endl;
                    delete existing;
                }
                map.remove(n);
            }
        }
    }

    QObjectList loadAll() {
        NodeObjectMap map;
        return loadAll(map);
    }

    QObjectList loadAll(NodeObjectMap &map) {

        Nodes nodes;
        
        Triples candidates = m_s->match(Triple(Node(), "a", Node()));
        foreach (Triple t, candidates) {
            if (t.c.type != Node::URI) continue;
            nodes.push_back(t.a);
        }

        load(nodes, map);

        QObjectList objects;
        foreach (Node n, nodes) {
            QObject *o = map.value(n);
            if (o) objects.push_back(o);
        }
        return objects;
    }
    
    void addLoadCallback(LoadCallback *cb) {
        m_loadCallbacks.push_back(cb);
    }

private:
    ObjectLoader *m_m;
    ObjectBuilder *m_ob;
    ContainerBuilder *m_cb;
    Store *m_s;
    TypeMapping m_tm;
    FollowPolicy m_fp;
    AbsentPropertyPolicy m_ap;
    QList<LoadCallback *> m_loadCallbacks;

    QObject *load(NodeObjectMap &map, NodeSet &examined, const Node &n,
                  QString classHint = "");

    QString getClassNameForNode(Node node, QString classHint = "",
                                CacheingPropertyObject *po = 0);

    QObject *allocateObject(NodeObjectMap &map, Node node, QObject *parent,
                            QString classHint,
                            CacheingPropertyObject *po);

//    QObject *loadTree(NodeObjectMap &map, Node node, QObject *parent);
    QObject *loadSingle(NodeObjectMap &map, NodeSet &examined, Node node, QObject *parent,
                        QString classHint, CacheingPropertyObject *po);

    void callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o);

//    void loadConnections(NodeObjectMap &map);

    void loadProperties(NodeObjectMap &map, NodeSet &examined, QObject *o, Node node,
                        CacheingPropertyObject *po);
    QVariant propertyNodeListToVariant(NodeObjectMap &map, NodeSet &examined, QString typeName,
                                       Nodes pnodes);
    QObject *propertyNodeToObject(NodeObjectMap &map, NodeSet &examined, QString typeName,
                                  Node pnode);
    QVariant propertyNodeToVariant(NodeObjectMap &map, NodeSet &examined, QString typeName,
                                   Node pnode);
    QVariantList propertyNodeToList(NodeObjectMap &map, NodeSet &examined, QString typeName,
                                    Node pnode);
};

QObject *
ObjectLoader::D::load(NodeObjectMap &map, NodeSet &examined, const Node &n,
                      QString classHint)
{
    DEBUG << "load: Examining " << n << endl;

    if (m_fp != FollowNone) {
        examined.insert(n);
    }

    CacheingPropertyObject po(m_s, m_tm.getPropertyPrefix().toString(), n);

    if (m_fp & FollowSiblings) {
        //!!! actually, wouldn't this mean query all siblings and follow all of those? as on store? but do we _ever_ actually want this behaviour?
        QString followsProp = m_tm.getRelationshipPrefix().toString() + "follows";
        if (po.hasProperty(followsProp)) {
            Node fn = po.getPropertyNode(followsProp);
            //!!! highly inefficient if we are last of many siblings
            if (!examined.contains(fn)) {
                try {
                    load(map, examined, fn);
                } catch (UnknownTypeException) {
                    //!!!
                }
            }
        }
    }

    QObject *parent = 0;
    QString parentProp = m_tm.getRelationshipPrefix().toString() + "parent";
    if (po.hasProperty(parentProp)) {
        Node pn = po.getPropertyNode(parentProp);
        try {
            if (m_fp & FollowParent) {
                if (!examined.contains(pn)) {
                    DEBUG << "load: FollowParent is set, loading parent of " << n << endl;
                    load(map, examined, pn);
                }
            } else if (map.contains(pn) && map.value(pn) == 0) {
                DEBUG << "load: Parent of node " << n << " has not been loaded yet, loading it" << endl;
                load(map, examined, pn);
            }
        } catch (UnknownTypeException) {
            //!!!
        }
        parent = map.value(pn);
    }

    //!!! and FollowChildren... (cf loadTree)

    //!!! NB. as this stands, if the RDF is "wrong" containing the
    //!!! wrong type for a property of this, we will fail the whole
    //!!! thing with an UnknownTypeException -- is that the right
    //!!! thing to do? consider

    QObject *o = loadSingle(map, examined, n, parent, classHint, &po);
    return o;
}

void
ObjectLoader::D::loadProperties(NodeObjectMap &map, NodeSet &examined, QObject *o, Node node,
                                CacheingPropertyObject *po)
{
    QString cname = o->metaObject()->className();

    bool myPo = false;
    if (!po) {
        po = new CacheingPropertyObject(m_s, m_tm.getPropertyPrefix().toString(), node);
        myPo = true;
    }

    QObject *defaultObject = 0;

    for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

        QMetaProperty property = o->metaObject()->property(i);

        if (!property.isStored() ||
            !property.isReadable() ||
            !property.isWritable()) {
            continue;
        }

        QString pname = property.name(); // name to use when setting on QObject
        QString plookup(pname);          // name or URI for PropertyObject

	Uri puri;
	if (m_tm.getPropertyUri(cname, pname, puri)) {
	    plookup = puri.toString();
	}

        Nodes pnodes;
        bool haveProperty = po->hasProperty(plookup);
        if (haveProperty) {
            pnodes = po->getPropertyNodeList(plookup);
            if (pnodes.empty()) {
                haveProperty = false;
            }
        }

        if (!haveProperty && m_ap == IgnoreAbsentProperties) continue;

        QVariant value;
        QByteArray pnba = pname.toLocal8Bit();

        if (!haveProperty) {
            if (!defaultObject) {
                if (m_ob->knows(cname)) {
                    defaultObject = m_ob->build(cname, 0);
                } else {
                    DEBUG << "Can't reset absent property " << pname
                          << " of object " << node << ": object builder "
                          << "doesn't know type " << cname << " so cannot "
                          << "build default object" << endl;
                }
            }

            if (defaultObject) {
                value = defaultObject->property(pnba.data());
            }

        } else {
            QString typeName = property.typeName();
            value = propertyNodeListToVariant(map, examined, typeName, pnodes);
        }

        if (!value.isValid()) {
            DEBUG << "Ignoring invalid variant for value of property "
                  << pname << " of object " << node << endl;
            continue;
        }

        if (!o->setProperty(pnba.data(), value)) {
            DEBUG << "loadProperties: Property set failed "
                  << "for property " << pname << " of type "
                  << property.typeName() << " (" << property.userType()
                  << ") to value of type " << value.type() 
                  << " and value " << value
                  << " from (first) node " << pnodes[0].value
                  << endl;
            DEBUG << "loadProperties: If the RDF datatype is correct, check "
                  << "[1] that the datatype is known to Dataquay::Node for "
                  << "node-variant conversion"
                  << "(datatype is one of the standard set, "
                  << "or registered with Node::registerDatatype) and "
                  << "[2] that the Q_PROPERTY type declaration "
                  << property.typeName()
                  << " matches the name passed to qRegisterMetaType (including namespace)"
                  << endl;
            std::cerr << "ObjectLoader::loadProperties: Failed to set property on object, ignoring" << std::endl;
        }
    }

    delete defaultObject;
    if (myPo) delete po;
}

QVariant
ObjectLoader::D::propertyNodeListToVariant(NodeObjectMap &map, NodeSet &examined, 
                                           QString typeName, Nodes pnodes)
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
                propertyNodeToList(map, examined, inContainerType, firstNode);
            return m_cb->injectContainer(typeName, list);

        } else if (k == ContainerBuilder::SetKind) {
            QVariantList list;
            foreach (Node pnode, pnodes) {
                Nodes sublist;
                sublist << pnode;
                list << propertyNodeListToVariant(map, examined, inContainerType, sublist);
            }
            return m_cb->injectContainer(typeName, list);

        } else {
            return QVariant();
        }

    } else if (m_ob->canInject(typeName)) {

        QObject *obj = propertyNodeToObject(map, examined, typeName, firstNode);
        DEBUG << "Obtained object " << obj << " from propertyNodeToObject for node " << firstNode << " of type " << typeName << endl;
        DEBUG << "Object has name " << obj->objectName() << endl;
        DEBUG << "Object has meta-object " << obj->metaObject() << endl;
        DEBUG << "... and class " << obj->metaObject()->className() << endl;
        QVariant v;
        if (obj) {
            v = m_ob->inject(typeName, obj);
            if (v == QVariant()) {
                DEBUG << "propertyNodeListToVariant: "
                      << "Type of node " << firstNode
                      << " is incompatible with expected "
                      << typeName << endl;
                std::cerr << "ObjectLoader::propertyNodeListToVariant: "
                          << "Incompatible node type, ignoring" << std::endl;
                //!!! don't delete the object without removing it from the map!
                // but which to do -- remove it, or leave it?
//                delete obj;
            }
        }
        return v;

    } else if (QString(typeName).contains("*") ||
               QString(typeName).endsWith("Star")) {
        // do not attempt to read binary pointers!
        return QVariant();

    } else {

        return propertyNodeToVariant(map, examined, typeName, firstNode);
    }
}


QVariant
ObjectLoader::D::propertyNodeToVariant(NodeObjectMap &map, NodeSet &examined, 
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

        DEBUG << "ObjectLoader::propertyNodeListToVariant: "
              << "Non-URI property is target for RDF URI, converting" << endl;
        
        acceptDefault = false;

    } else if (pnode.type == Node::Literal &&
               pnode.datatype == Uri() && // no datatype
               typeName != QMetaType::typeName(QMetaType::QString)) {

        DEBUG << "ObjectLoader::propertyNodeListToVariant: "
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
ObjectLoader::D::propertyNodeToObject(NodeObjectMap &map, NodeSet &examined,
                                      QString typeName, Node pnode)
{
    QObject *o = map.value(pnode);
    if (o) {
        DEBUG << "propertyNodeToObject: found object in map for node "
              << pnode << endl;

        //!!! document the implications of this -- e.g. that if
        //!!! multiple objects have properties with the same node in
        //!!! the RDF, they will be given the same object instance --
        //!!! so it is generally a good idea to have "ownership" and
        //!!! lifetime of these objects managed by something other
        //!!! than the objects that have the properties

        return o;
    }

    DEBUG << "propertyNodeToObject: object for node "
          << pnode << " is not (yet) in map" << endl;

    if (pnode.type == Node::URI || pnode.type == Node::Blank) {
        bool shouldLoad = false;
        if (m_fp & FollowObjectProperties) {
            if (!examined.contains(pnode)) {
                shouldLoad = true;
            }
        } else if (map.contains(pnode)) { // we know map.value(pnode) == 0
            shouldLoad = true;
        }
        if (shouldLoad) {
            QString classHint;
            if (typeName != "") {
                classHint = m_ob->getClassNameForPointerName(typeName);
                DEBUG << "typeName " << typeName << " -> classHint " << classHint << endl;
            }
            load(map, examined, pnode, classHint);
            o = map.value(pnode);
        }
    }

    return o;
}

QVariantList
ObjectLoader::D::propertyNodeToList(NodeObjectMap &map, NodeSet &examined, 
                                    QString typeName, Node pnode)
{
    QVariantList list;
    Triple t;

    while ((t = m_s->matchFirst(Triple(pnode, "rdf:first", Node())))
           != Triple()) {

        Node fnode = t.c;

        DEBUG << "propertyNodeToList: pnode " << pnode << ", fnode "
              << fnode << endl;

        Nodes fnodes;
        fnodes << fnode;
        QVariant value = propertyNodeListToVariant(map, examined, typeName, fnodes);

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

QString
ObjectLoader::D::getClassNameForNode(Node node, QString classHint,
                                     CacheingPropertyObject *po)
{
    Uri typeUri;
    if (po) typeUri = po->getObjectType();
    else {
        Triple t = m_s->matchFirst(Triple(node, "a", Node()));
        if (t.c.type == Node::URI) typeUri = Uri(t.c.value);
    }

    QString className;
    if (typeUri != Uri()) {
        try {
            className = m_tm.synthesiseClassForTypeUri(typeUri);
        } catch (UnknownTypeException) {
            DEBUG << "getClassNameForNode: Unknown type URI " << typeUri << endl;
            if (classHint == "") throw;
            DEBUG << "(falling back to object class hint " << classHint << ")" << endl;
            className = classHint;
        }
    } else {
        DEBUG << "getClassNameForNode: No type URI for " << node << endl;
        if (classHint == "") throw UnknownTypeException("");
        DEBUG << "(falling back to object class hint " << classHint << ")" << endl;
        className = classHint;
    }
        
    if (!m_ob->knows(className)) {
        DEBUG << "ObjectLoader::getClassNameForNode: Unknown object class "
              << className << endl;
        throw UnknownTypeException(className);
    }

    return className;
}

QObject *
ObjectLoader::D::allocateObject(NodeObjectMap &map, Node node, QObject *parent,
                                QString classHint, CacheingPropertyObject *po)
{
    QObject *o = map.value(node);
    if (o) {
        DEBUG << "allocateObject: " << node << " already allocated" << endl;
        return o;
    }

    // The RDF may contain a more precise type specification than
    // we have here.  For example, if we have been called as part
    // of a process of loading something declared as container of
    // base-class pointers, but each individual object is actually
    // of a derived class, then the RDF should specify the derived
    // class but we will only have been passed the base class.  So
    // we always use the RDF type if available, and refer to the
    // passed-in type only if that fails.

    QString className = getClassNameForNode(node, classHint, po);
    
    DEBUG << "Making object " << node.value << " of type "
          << className << " with parent " << parent << endl;

    o = m_ob->build(className, parent);
    if (!o) throw ConstructionFailedException(className);
	
    if (node.type == Node::URI) {
        o->setProperty("uri", QVariant::fromValue(m_s->expand(node.value)));
    }

    map.insert(node, o);
    return o;
}

QObject *
ObjectLoader::D::loadSingle(NodeObjectMap &map, NodeSet &examined,
                            Node node, QObject *parent,
                            QString classHint, CacheingPropertyObject *po)
{
    DEBUG << "loadSingle: " << node << endl;

    QObject *o = map.value(node);
    if (!o) {
        o = allocateObject(map, node, parent, classHint, po);
    }

    loadProperties(map, examined, o, node, po);

    callLoadCallbacks(map, node, o);

    return o;
}

void
ObjectLoader::D::callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o)
{
    foreach (LoadCallback *cb, m_loadCallbacks) {
        //!!! this doesn't really work out -- the callback doesn't know whether we're loading a single object or a graph; it may load any number of other related objects into the map, and if we were only supposed to load a single object, we won't know what to do with them afterwards (at the moment we just leak them)

        cb->loaded(m_m, map, node, o);
    }
}
/*
void
ObjectLoader::D::loadConnections(NodeObjectMap &map)
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
          " } ").arg(m_tm.getRelationshipPrefix().toString()));

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
ObjectLoader::D::loadTree(NodeObjectMap &map, Node node, QObject *parent)
{

    QObject *o;
    try {
        o = loadSingle(map, node, parent, "", true, 0); //!!!??? or false?
    } catch (UnknownTypeException e) {
        o = 0;
    }

    if (o) {
        Triples childTriples = m_s->match
            (Triple(Node(),
		    m_tm.getRelationshipPrefix().toString() + "parent", node));
        foreach (Triple t, childTriples) {
            loadTree(map, t.a, o);
        }
    }

    return o;
}
*/
ObjectLoader::ObjectLoader(Store *s) :
    m_d(new D(this, s))
{ }

ObjectLoader::~ObjectLoader()
{
    delete m_d;
}

Store *
ObjectLoader::getStore()
{
    return m_d->getStore();
}

void
ObjectLoader::setTypeMapping(const TypeMapping &tm)
{
    m_d->setTypeMapping(tm);
}

const TypeMapping &
ObjectLoader::getTypeMapping() const
{
    return m_d->getTypeMapping();
}

void
ObjectLoader::setFollowPolicy(FollowPolicy policy)
{
    m_d->setFollowPolicy(policy);
}

ObjectLoader::FollowPolicy
ObjectLoader::getFollowPolicy() const
{
    return m_d->getFollowPolicy();
}

void
ObjectLoader::setAbsentPropertyPolicy(AbsentPropertyPolicy policy)
{
    m_d->setAbsentPropertyPolicy(policy);
}

ObjectLoader::AbsentPropertyPolicy
ObjectLoader::getAbsentPropertyPolicy() const
{
    return m_d->getAbsentPropertyPolicy();
}

/*void
ObjectLoader::loadProperties(QObject *o, Uri uri)
{
    m_d->loadProperties(o, uri);
}

QObject *
ObjectLoader::loadObject(Uri uri, QObject *parent)
{
    return m_d->loadObject(uri, parent);
}

QObject *
ObjectLoader::loadObjectTree(Uri rootUri, QObject *parent)
{
    return m_d->loadObjectTree(rootUri, parent);
}

QObject *
ObjectLoader::loadAllObjects(QObject *parent)
{
    return m_d->loadAllObjects(parent);
}

QObject *
ObjectLoader::loadFrom(NodeObjectMap &map, Node source)
{
    return m_d->loadFrom(map, source);
}
*/

QObject *
ObjectLoader::load(Node node)
{
    return m_d->load(node);
}

void
ObjectLoader::load(Nodes nodes, NodeObjectMap &map)
{
    m_d->load(nodes, map);
}

QObjectList
ObjectLoader::loadAll()
{
    return m_d->loadAll();
}

QObjectList
ObjectLoader::loadAll(NodeObjectMap &map)
{
    return m_d->loadAll(map);
}

void
ObjectLoader::addLoadCallback(LoadCallback *cb)
{
    m_d->addLoadCallback(cb);
}

}


	
