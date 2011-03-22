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
#include "TypeMapping.h"

#include "ObjectMapperExceptions.h"

#include <QMetaProperty>
#include <QSet>

#include "../PropertyObject.h"
#include "../Store.h"

#include "../Debug.h"

namespace Dataquay {

/*
 * The various sets and maps that get passed around are:
 *
 * NodeObjectMap &map -- keeps track of node-object mappings for nodes
 * that are being loaded, and also for any other nodes that the
 * calling code is interested in (this map belongs to it, we just
 * update it).  We can refer to this to find out the object for a
 * given node, but it's reliable only when we know that we have just
 * loaded that node ourselves -- otherwise the value may be stale,
 * left over from previous work.  (However, we will still use it if we
 * need that node for an object property and FollowObjectProperties is
 * not set so we aren't loading afresh.)
 *
 * NodeSet &examined -- the set of all nodes we have seen and loaded
 * (or started to load) so far.  Used solely to avoid infinite
 * recursions.
 *
 * What we need:
 *
 * NodeObjectMap as above.
 *
 * Something to list which nodes we need to load. This is populated
 * from the Node or Nodes passed in, and then we traverse the tree
 * using the follow-properties adding nodes as appropriate.
 *
 * Something to list which nodes we have tried to load.  This is our
 * current examined set.  Or do we just remove from the to-load list?
 * 
 * Workflow: something like --
 * 
 * - Receive list A of the nodes the customer wants
 *
 * - Receive node-object map B from customer for updating (and for our
 *   reference, as soon as we know that a particular node has been
 *   updated)
 *
 * - Construct set C of nodes to load, initially empty
 *
 * - Traverse list A; for each node:
 *   - if node does not exist in store, set null in B and continue
 *   - add node to set C
 *   - push parent on end of A if FollowParent and parent property
 *     is present and parent is not in C
 *   - push prior sibling on end of A if FollowSiblings and follows 
 *     property is present and sibling is not in C
 *   - likewise for children
 *   - likewise for each property node if FollowObjectProperties
 *     and node is not in C
 *
 * Now we really need a version D of set C (or list A) which is in
 * tree traversal order -- roots first...
 *
 * - Traverse D; for each node:
 *   - load node, recursing to parent and siblings if necessary,
 *     but do not set any properties
 *   - put node value in B
 *
 * - Traverse D; for each node:
 *   - load properties, recursing if appropriate
 *   - call load callbacks
 * 
 * But we still need the examined set to avoid repeating ourselves
 * where an object is actually un-loadable?
 */

class ObjectLoader::D
{
    typedef QSet<Node> NodeSet;

public:
    struct LoadState {
        Nodes desired;
        Nodes candidates;
        NodeSet visited;
        NodeObjectMap map;
    };

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

    QObject *load(Node node) {
        LoadState state;
        state.desired << node;
        collect(state);
        load(state);
        return state.map.value(node);
    }
    
    void reload(Nodes nodes, NodeObjectMap &map) {
        
        LoadState state;
        state.desired = nodes;
        state.map = map;
        collect(state);
        load(state);        

/*
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
*/

        map = state.map;
    }

    QObjectList loadType(Uri type) {
        NodeObjectMap map;
        return loadType(type, map);
    }

    QObjectList loadType(Uri type, NodeObjectMap &map) {
        return loadType(Node(type), map);
    }

    QObjectList loadType(Node typeNode, NodeObjectMap &map) {

        Nodes nodes;
        
        Triples candidates = m_s->match(Triple(Node(), "a", typeNode));
        foreach (Triple t, candidates) {
            if (t.c.type != Node::URI) continue;
            nodes.push_back(t.a);
        }

        reload(nodes, map);

        QObjectList objects;
        foreach (Node n, nodes) {
            QObject *o = map.value(n);
            if (o) objects.push_back(o);
        }
        return objects;
    }

    QObjectList loadAll() {
        NodeObjectMap map;
        return loadAll(map);
    }

    QObjectList loadAll(NodeObjectMap &map) {
        return loadType(Node(), map);
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

    void collect(LoadState &state) {

        state.candidates = state.desired;

        NodeSet examined;

        // Avoid ever pushing the nil Node as a future candidate by
        // marking it as used already

        examined << Node();
        
        // Use counter to iterate, so that when additional elements
        // pushed onto the end of state.desired will be iterated over

        for (int i = 0; i < state.candidates.size(); ++i) {
        
            Node node = state.candidates[i];

            examined << node;

            if (!nodeHasTypeInStore(node)) {
                state.map.insert(node, 0);
                continue;
            }

            Nodes relatives;

            if (m_fp & FollowParent) {
                relatives << parentOf(node);
            }
            if (m_fp & FollowChildren) {
                relatives << childrenOf(node);
            }
            if (m_fp & FollowSiblings) {
                relatives << prevSiblingOf(node) << nextSiblingOf(node);
            }
            if (m_fp & FollowObjectProperties) {
                relatives << potentialPropertyNodesOf(node);
            }
                
            foreach (Node r, relatives) {
                if (!examined.contains(r)) {
                    state.candidates << r;
                }
            }
        }

        DEBUG << "ObjectLoader: collect: desired = "
              << state.desired.size() << ", candidates = "
              << state.candidates.size() << endl;
    }

    bool nodeHasTypeInStore(Node node) {
        Triple t = m_s->matchFirst(Triple(node, "a", Node()));
        return (t.c.type == Node::URI);
    }

    Node parentOf(Node node) {
        QString parentProp = m_tm.getRelationshipPrefix().toString() + "parent";
        Triple t = m_s->matchFirst(Triple(node, parentProp, Node()));
        if (t != Triple()) return t.c;
        else return Node();
    }

    Nodes childrenOf(Node node) {
        Nodes nn;
        QString parentProp = m_tm.getRelationshipPrefix().toString() + "parent";
        Triples tt = m_s->match(Triple(Node(), parentProp, node));
        foreach (Triple t, tt) nn << t.a;
        return nn;
    }

    Node prevSiblingOf(Node node) {
        QString followProp = m_tm.getRelationshipPrefix().toString() + "follows";
        Triple t = m_s->matchFirst(Triple(node, followProp, Node()));
        if (t != Triple()) return t.c;
        else return Node();
    }

    Node nextSiblingOf(Node node) {
        QString followProp = m_tm.getRelationshipPrefix().toString() + "follows";
        Triple t = m_s->matchFirst(Triple(Node(), followProp, node));
        if (t != Triple()) return t.a;
        else return Node();
    }

    Nodes potentialPropertyNodesOf(Node node) {
        Nodes nn;
        Triples tt = m_s->match(Triple(node, Node(), Node()));
        foreach (Triple t, tt) {
            if (nodeHasTypeInStore(t.a)) {
                nn << t.a;
            }
        }
        return nn;
    }

    void load(LoadState &state) {
        
        NodeSet examined;

        // Avoid ever pushing the nil Node as a future candidate by
        // marking it as used already

        examined << Node();
        
        // Use counter to iterate, so that when additional elements
        // pushed onto the end of state.desired will be iterated over

        for (int i = 0; i < state.candidates.size(); ++i) {

            Node node = state.candidates[i];

            QObject *parentObject = 0;
            Node parent = parentOf(node);
            if (m_fp & FollowParent) {
//                parentObject = loadSingle(state, parent);
            } else {
                //!!! no -- whether this is correct or not (or present or not) may depend on the order in which the nodes are listed in candidates -- we _must_ make sure they're listed in the "correct" (tree traversal) order -- no node having a parent dependency on a node that appears after it -- _and_ we must guarantee to have no interest in loading anything not in the candidates list
//                parentObject = state.map.value(parent);
            }
            
        }
    }


    QObject *load(NodeObjectMap &map, NodeSet &examined, const Node &n,
                  QString classHint = "");

    QString getClassNameForNode(Node node, QString classHint = "",
                                CacheingPropertyObject *po = 0);

    QObject *allocateObject(NodeObjectMap &map, Node node, QObject *parent,
                            QString classHint,
                            CacheingPropertyObject *po);

    QObject *loadSingle(NodeObjectMap &map, NodeSet &examined, Node node, QObject *parent,
                        QString classHint, CacheingPropertyObject *po);

    void callLoadCallbacks(NodeObjectMap &map, Node node, QObject *o);

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
                  << pname << ", type " << property.typeName()
                  << " of object " << node << endl;
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

    DEBUG << "propertyNodeToVariant: typeName = " << typeName << endl;

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
                DEBUG << "Not in examined set: loading now" << endl;
                shouldLoad = true;
            } else {
                DEBUG << "Found in examined set, not retrying" << endl;
            }
        } else if (map.contains(pnode)) { // we know map.value(pnode) == 0
            DEBUG << "In map with null value: loading now" << endl;
            shouldLoad = true;
        } else {
            DEBUG << "Not in map and FollowObjectProperties not set: not loading" << endl;
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
    } else {
        DEBUG << "Not an object node, ignoring" << endl;
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

QObject *
ObjectLoader::load(Node node)
{
    return m_d->load(node);
}

void
ObjectLoader::reload(Nodes nodes, NodeObjectMap &map)
{
    m_d->reload(nodes, map);
}

QObjectList
ObjectLoader::loadType(Uri type)
{
    return m_d->loadType(type);
}

QObjectList
ObjectLoader::loadType(Uri type, NodeObjectMap &map)
{
    return m_d->loadType(type, map);
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


	
