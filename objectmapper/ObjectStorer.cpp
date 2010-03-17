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

#include "ObjectStorer.h"
#include "ObjectBuilder.h"
#include "ContainerBuilder.h"
#include "TypeMapping.h"

#include "../PropertyObject.h"
#include "../Store.h"
#include "../Debug.h"

#include <memory>

#include <QMetaProperty>
#include <QSet>

namespace Dataquay
{

class ObjectStorer::D
{
    typedef QSet<QObject *> ObjectSet;

public:
    D(ObjectStorer *m, Store *s) :
        m_m(m),
        m_ob(ObjectBuilder::getInstance()),
        m_cb(ContainerBuilder::getInstance()),
        m_s(s),
        m_psp(StoreAlways),
        m_bp(BlankNodesAsNeeded),
        m_fp(FollowNone) {
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

    void setPropertyStorePolicy(PropertyStorePolicy psp) {
        m_psp = psp; 
    }

    PropertyStorePolicy getPropertyStorePolicy() const {
        return m_psp;
    }

    void setBlankNodePolicy(BlankNodePolicy bp) {
        m_bp = bp; 
    }

    BlankNodePolicy getBlankNodePolicy() const {
        return m_bp;
    }

    void setFollowPolicy(FollowPolicy fp) {
        m_fp = fp; 
    }

    FollowPolicy getFollowPolicy() const {
        return m_fp;
    }

    void removeObject(Node n) {
        Triples triples = m_s->match(Triple(n, Node(), Node()));
        foreach (Triple t, triples) {
            if (t.b.type == Node::URI) {
                removeOldPropertyNodes(n, Uri(t.b.value));
            }
        }
        m_s->remove(Triple(Node(), Node(), n));
    }

    Uri store(QObject *o, ObjectNodeMap &map) {
        ObjectSet examined;
        if (!map.contains(o)) {
            // ensure blank node not used for this object       
            map.insert(o, Node());
        }
        Node node = store(map, examined, o);
        if (node.type != Node::URI) {
            // This shouldn't happen (see above)
            DEBUG << "ObjectStorer::store: Stored object node "
                  << node << " is not a URI node" << endl;
            std::cerr << "WARNING: ObjectStorer::store: No URI for stored object!" << std::endl;
            return Uri();
        } else {
            return Uri(node.value);
        }
    }

    void store(QObjectList ol, ObjectNodeMap &map) {
        ObjectSet examined;
        int n = ol.size(), i = 0;
        foreach (QObject *o, ol) {
            if (!map.contains(o)) {
                // ensure blank node not used for this object            
                map.insert(o, Node());
            }
        }
        foreach (QObject *o, ol) {
            store(map, examined, o);
        }
    }

    Node store(ObjectNodeMap &map, ObjectSet &examined, QObject *o);

    void addStoreCallback(StoreCallback *cb) {
        m_storeCallbacks.push_back(cb);
    }

private:
    ObjectStorer *m_m;
    ObjectBuilder *m_ob;
    ContainerBuilder *m_cb;
    Store *m_s;
    TypeMapping m_tm;
    PropertyStorePolicy m_psp;
    BlankNodePolicy m_bp;
    FollowPolicy m_fp;
    QList<StoreCallback *> m_storeCallbacks;

    Node allocateNode(ObjectNodeMap &map, QObject *o);
    Node storeSingle(ObjectNodeMap &map, ObjectSet &examined, QObject *o);

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node);

    void storeProperties(ObjectNodeMap &map, ObjectSet &examined, QObject *o, Node node);
    void removeUnusedBlankNode(Node node);
    void removeOldPropertyNodes(Node node, Uri propertyUri);
    Nodes variantToPropertyNodeList(ObjectNodeMap &map, ObjectSet &examined, QVariant v);
    Node objectToPropertyNode(ObjectNodeMap &map, ObjectSet &examined, QObject *o);
    Node listToPropertyNode(ObjectNodeMap &map, ObjectSet &examined, QVariantList list);
};

void
ObjectStorer::D::storeProperties(ObjectNodeMap &map, ObjectSet &examined, QObject *o, Node node)
{
    QString cname = o->metaObject()->className();
    PropertyObject po(m_s, m_tm.getPropertyPrefix().toString(), node);

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
                } else {
                    DEBUG << "Property " << pname << " of object "
                          << node << " is changed from default value "
                          << c->property(pnba.data()) << ", writing" << endl;
                }
            } else {
                DEBUG << "Can't check property " << pname << " of object "
                      << node << " for change from default value: "
                      << "object builder doesn't know type " << cname
                      << " so cannot build default object" << endl;
            }
        }

        DEBUG << "For object " << node.value << " (" << o << ") writing property " << pname << " of type " << property.userType() << endl;

        Uri puri;
	if (!m_tm.getPropertyUri(cname, pname, puri)) {
            puri = po.getPropertyUri(pname);
        }

        //!!! at this point we are committed to removing any old properties, but we have not yet decided for sure whether to write the new ones -- is that right?  also, we do not remove lists yet?

        removeOldPropertyNodes(node, puri);

        Nodes pnodes = variantToPropertyNodeList(map, examined, value);

        Triple t(node, puri, Node());
        for (int j = 0; j < pnodes.size(); ++j) {
            t.c = pnodes[j];
            m_s->add(t);
        }
    }
}            

void
ObjectStorer::D::removeUnusedBlankNode(Node node)
{
    // The node is known to be a blank node.  If it is not referred to
    // by anything else, then we want to remove everything it refers
    // to.  If it happens to be a list node, then we also want to
    // recurse to its tail.

    if (m_s->matchFirst(Triple(Node(), Node(), node)) != Triple()) {
        // The node is still a target of some predicate, leave it alone
        return;
    }

    DEBUG << "removeUnusedBlankNode: Blank node " << node
          << " is not target for any other predicate" << endl;

    // check for a list tail (query first, but then delete our own
    // triples first so that the existence of this rdf:rest
    // relationship isn't seen as a reason not to delete the tail)
    Triples tails(m_s->match(Triple(node, "rdf:rest", Node())));

    DEBUG << "... removing everything with it as subject" << endl;
    m_s->remove(Triple(node, Node(), Node()));

    foreach (Triple t, tails) {
        DEBUG << "... recursing to list tail" << endl;
        removeUnusedBlankNode(t.c);
    }
}
            
void
ObjectStorer::D::removeOldPropertyNodes(Node node, Uri propertyUri) 
{
    Triple t(node, propertyUri, Node());
    Triples m(m_s->match(t));
    foreach (t, m) {
        m_s->remove(t);
        if (t.c.type == Node::Blank && t.c != node) {
            removeUnusedBlankNode(t.c);
        }
    }
}

Nodes
ObjectStorer::D::variantToPropertyNodeList(ObjectNodeMap &map, ObjectSet &examined, QVariant v)
{
    const char *typeName = QMetaType::typeName(v.userType());
    Nodes nodes;
    if (!typeName) {
        std::cerr << "ObjectStorer::variantToPropertyNodeList: No type name?! Going ahead anyway" << std::endl;
        nodes << Node::fromVariant(v);
        return nodes;
    }

    DEBUG << "variantToPropertyNodeList: typeName = " << typeName << endl;
        
    if (m_cb->canExtractContainer(typeName)) {

        QVariantList list = m_cb->extractContainer(typeName, v);
        ContainerBuilder::ContainerKind k = m_cb->getContainerKind(typeName);

        if (k == ContainerBuilder::SequenceKind) {
            Node node = listToPropertyNode(map, examined, list);
            if (node != Node()) nodes << node;
                
        } else if (k == ContainerBuilder::SetKind) {
            foreach (QVariant member, list) {
                nodes += variantToPropertyNodeList(map, examined, member);
            }
        }
            
    } else if (m_ob->canExtract(typeName)) {

        QObject *obj = m_ob->extract(typeName, v);
        if (obj) {
            Node n = objectToPropertyNode(map, examined, obj);
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
ObjectStorer::D::objectToPropertyNode(ObjectNodeMap &map, ObjectSet &examined, QObject *o)
{
    Node pnode;

    DEBUG << "objectToPropertyNode: " << o << ", follow = " << (m_fp & FollowObjectProperties) << endl;

    // -> our policy for parent is:
    //  - if we have FollowParent set, write parent always
    //  - otherwise, if the parent is in the map but with no node
    //    assigned, write it
    //  - otherwise, if there is a node for the parent in the map,
    //    write the reference to it (using that node) without rewriting
    //  - otherwise, do not write a reference
    // -> This seems sensible for properties too?

    if (m_fp & FollowObjectProperties) {
        DEBUG << "objectToPropertyNode: FollowObjectProperties is set, writing object" << endl;
        if (!examined.contains(o)) {
            store(map, examined, o);
        }
    } else if (map.contains(o) && map.value(o) == Node()) {
        DEBUG << "objectToPropertyNode: Object has not been written yet, writing it" << endl;
        store(map, examined, o);
    }

    pnode = map.value(o);
    return pnode;
}

Node
ObjectStorer::D::listToPropertyNode(ObjectNodeMap &map, ObjectSet &examined, QVariantList list)
{
    DEBUG << "listToPropertyNode: have " << list.size() << " items" << endl;

    Node node, first, previous;

    foreach (QVariant v, list) {

        Nodes pnodes = variantToPropertyNodeList(map, examined, v);
        if (pnodes.empty()) {
            std::cerr << "WARNING: ObjectStorer::listToPropertyNode: Obtained nil Node in list" << std::endl;
            continue;
        } else if (pnodes.size() > 1) {
            std::cerr << "WARNING: ObjectStorer::listToPropertyNode: Found set within sequence, can't handle this, using first element only" << std::endl;
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

Node
ObjectStorer::D::store(ObjectNodeMap &map, ObjectSet &examined, QObject *o)
{
    DEBUG << "store: Examining " << o << endl;

    if (m_fp != FollowNone) {
        examined.insert(o);
    }

    Node node = storeSingle(map, examined, o);
    
    QObject *parent = o->parent();
    Uri parentUri(m_tm.getRelationshipPrefix().toString() + "parent");

    if (!parent) {

        m_s->remove(Triple(node, parentUri, Node()));

    } else {

        if (m_fp & FollowParent) {
            if (!examined.contains(parent)) {
                DEBUG << "store: FollowParent is set, writing parent of " << node << endl;
                store(map, examined, parent);
            }
        } else if (map.contains(parent) && map.value(parent) == Node()) {
            // parent is to be written at some point: bring it forward
            // so we have a uri to refer to now
            DEBUG << "store: Parent of " << node << " has not been written yet, writing it" << endl;
            store(map, examined, parent);
        }

        Node pn = map.value(parent);
        if (pn != Node()) {

            m_s->remove(Triple(node, parentUri, Node()));
            m_s->add(Triple(node, parentUri, pn));

            Uri followsUri(m_tm.getRelationshipPrefix().toString() + "follows");

            // write (references to) siblings (they wouldn't be
            // meaningful if the parent was absent)

            QObjectList siblings = parent->children();

            if (m_fp & FollowSiblings) {
                // Mark the siblings we are about to recurse to as
                // examined, so that they don't then attempt to
                // recurse to one another (which could run us out of
                // stack).  Also allocate nodes for them now in case
                // they need to be referred to before we get around to
                // storing them (e.g. if a later sibling is required
                // as a parent of a property of an earlier one).
                // Store in toFollow the ones we will need to recurse
                // to.
                QObjectList toFollow; // list, not set: order matters
                foreach (QObject *s, siblings) {
                    if (!examined.contains(s)) {
                        toFollow.push_back(s);
                        allocateNode(map, s);
                        examined.insert(s);
                    }
                }
                foreach (QObject *s, toFollow) {
                    DEBUG << "store: FollowSiblings is set, writing sibling of " << node << endl;
                    store(map, examined, s);
                }
            }
        
            // find previous sibling

            QObject *previous = 0;
            for (int i = 0; i < siblings.size(); ++i) {
                if (siblings[i] == o) {
                    if (i > 0) {
                        previous = siblings[i-1];
                    }
                    break;
                }
            }

            if (previous) {
                // we have to write a reference to the previous sibling, but
                // we don't necessarily have to write the sibling itself -- if
                // FollowSiblings is set, it will have been written in the
                // all-siblings loop above

                if (!(m_fp & FollowSiblings)) {
                    if (map.contains(previous) && map.value(previous) == Node()) {
                        // previous is to be written at some point: bring it
                        // forward so we have a uri to refer to now
                        DEBUG << "store: Previous sibling of " << node << " has not been written yet, writing it" << endl;
                        store(map, examined, previous);
                    }
                }

                Node sn = map.value(previous);
                if (sn != Node()) {
                    m_s->remove(Triple(node, followsUri, Node()));
                    m_s->add(Triple(node, followsUri, sn));
                } else {
                    if (m_fp & FollowSiblings) {
                        std::cerr << "Internal error: FollowSiblings set, but previous sibling has not been written" << std::endl;
                    }
                }

            } else {
                // no previous sibling
                m_s->remove(Triple(node, followsUri, Node()));
            }
        } else {
            // no parent node
            if (m_fp & FollowParent) {
                std::cerr << "Internal error: FollowParent set, but parent has not been written" << std::endl;
            }
        }
    }

    if (m_fp & FollowChildren) {
        // Mark the children we are about to recurse to as examined,
        // so that they don't then attempt to recurse to one another
        // as siblings (which could run us out of stack).  Also
        // allocate nodes for them now in case they need to be
        // referred to before we get around to storing them (e.g. if a
        // later child is required as a parent of a property of an
        // earlier one).  Store in toFollow the ones we will need to
        // recurse to.
        QObjectList children = o->children();
        QObjectList toFollow; // list, not set: order matters
        foreach (QObject *c, children) {
            if (!examined.contains(c)) {
                toFollow.push_back(c);
                allocateNode(map, c);
                examined.insert(c);
            }
        }
        foreach (QObject *c, toFollow) {
            DEBUG << "store: FollowChildren is set, writing child of " << node << endl;
            store(map, examined, c);
        }
    }

    DEBUG << "store: Finished with " << o << endl;

    return node;
}

Node
ObjectStorer::D::allocateNode(ObjectNodeMap &map, QObject *o)
{
    Node node;

    if (map.contains(o) && map.value(o) != Node()) {

        node = map.value(o);

    } else {

        QVariant uriVar = o->property("uri");

        if (uriVar != QVariant()) {

            if (Uri::isUri(uriVar)) {
                node = uriVar.value<Uri>();
            } else {
                node = m_s->expand(uriVar.toString());
            }

        } else if (!map.contains(o) && m_bp == BlankNodesAsNeeded) { //!!! I don't much like that name

            //!!! document the conditions under which blank nodes are
            //!!! used, and the consequences (e.g. blank node id cannot be
            //!!! recorded in the object property, so if you want the
            //!!! object to have the same node when stored again, you will
            //!!! need to preserve the objectNodeMap)

            node = m_s->addBlankNode();

        } else {
            QString className = o->metaObject()->className();
            Uri prefix;
            if (!m_tm.getUriPrefixForClass(className, prefix)) {
                //!!! put this in TypeMapping?
                QString tag = className.toLower() + "_";
                tag.replace("::", "_");
                prefix = m_s->expand(":" + tag);
            }
            Uri uri = m_s->getUniqueUri(prefix.toString());
            o->setProperty("uri", QVariant::fromValue(uri)); //!!! document this
            node = uri;
        }

        map.insert(o, node);
    }

    return node;
}

Node
ObjectStorer::D::storeSingle(ObjectNodeMap &map, ObjectSet &examined, QObject *o)
{
    Node node;
    
    // This function should only be called when we know we want to
    // store an object -- all conditions have been satisfied.

    if (!map.contains(o) || map.value(o) == Node()) {
        node = allocateNode(map, o);
    } else {
        node = map.value(o);
    }

    QString className = o->metaObject()->className();
    m_s->add(Triple(node, "a", m_tm.synthesiseTypeUriForClass(className)));

    storeProperties(map, examined, o, node);

    callStoreCallbacks(map, o, node);

    return node;
}

void
ObjectStorer::D::callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node)
{
    foreach (StoreCallback *cb, m_storeCallbacks) {
        cb->stored(m_m, map, o, node);
    }
}

ObjectStorer::ObjectStorer(Store *s) :
    m_d(new D(this, s))
{ }

ObjectStorer::~ObjectStorer()
{
    delete m_d;
}

Store *
ObjectStorer::getStore()
{
    return m_d->getStore();
}

void
ObjectStorer::setTypeMapping(const TypeMapping &tm)
{
    m_d->setTypeMapping(tm);
}

const TypeMapping &
ObjectStorer::getTypeMapping() const
{
    return m_d->getTypeMapping();
}

void
ObjectStorer::setPropertyStorePolicy(PropertyStorePolicy policy)
{
    m_d->setPropertyStorePolicy(policy);
}

ObjectStorer::PropertyStorePolicy
ObjectStorer::getPropertyStorePolicy() const
{
    return m_d->getPropertyStorePolicy();
}

void
ObjectStorer::setBlankNodePolicy(BlankNodePolicy policy)
{
    m_d->setBlankNodePolicy(policy);
}

ObjectStorer::BlankNodePolicy
ObjectStorer::getBlankNodePolicy() const
{
    return m_d->getBlankNodePolicy();
}

void
ObjectStorer::setFollowPolicy(FollowPolicy policy)
{
    m_d->setFollowPolicy(policy);
}

ObjectStorer::FollowPolicy
ObjectStorer::getFollowPolicy() const
{
    return m_d->getFollowPolicy();
}

void
ObjectStorer::removeObject(Node n)
{
    m_d->removeObject(n);
}

Uri
ObjectStorer::store(QObject *o)
{
    ObjectNodeMap map;
    return m_d->store(o, map);
}

Uri
ObjectStorer::store(QObject *o, ObjectNodeMap &map)
{
    return m_d->store(o, map);
}

void
ObjectStorer::store(QObjectList o)
{
    ObjectNodeMap map;
    m_d->store(o, map);
}

void
ObjectStorer::store(QObjectList o, ObjectNodeMap &map)
{
    m_d->store(o, map);
}

void
ObjectStorer::addStoreCallback(StoreCallback *cb)
{
    m_d->addStoreCallback(cb);
}

}

