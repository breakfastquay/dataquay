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
        //!!! expunge every reference, forward and back?  what about
        // propertyies that are blank nodes not referred to elsewhere,
        // a la removeOldPropertyNodes?

        //!!! also handle removing lists (rdf:first/rdf:rest/rdf:nil)

        m_s->remove(Triple(n, Node(), Node()));
        m_s->remove(Triple(Node(), Node(), n));
    }

    Uri store(QObject *o, ObjectNodeMap &map) {
        ObjectSet examined;
        if (!map.contains(o)) {
            // ensure blank node not used for this object            
            map[o] = Node();
        }
        Node node = store(map, examined, o);
        if (node.type != Node::URI) {
            DEBUG << "ObjectStorer::store: Stored object node "
                  << node << " is not a URI node" << endl;
            std::cerr << "WARNING: ObjectStorer::store: No URI for stored object!" << std::endl;
            return Uri();
        } else {
            return Uri(node.value);
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

    Node storeSingle(ObjectNodeMap &map, ObjectSet &examined, QObject *o);

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node);

    void storeProperties(ObjectNodeMap &map, ObjectSet &examined, QObject *o, Node node);
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
                          << node.value << " is changed from default value "
                          << c->property(pnba.data()) << ", writing" << endl;
                }
            }
        }

        DEBUG << "For object " << node.value << " writing property " << pname << " of type " << property.type() << endl;

        Nodes pnodes = variantToPropertyNodeList(map, examined, value);

        Uri puri;
	if (!m_tm.getPropertyUri(cname, pname, puri)) {
            puri = po.getPropertyUri(pname);
        }

        removeOldPropertyNodes(node, puri);

        Triple t(node, puri, Node());
        for (int i = 0; i < pnodes.size(); ++i) {
            t.c = pnodes[i];
            m_s->add(t);
        }
    }
}            
            
void
ObjectStorer::D::removeOldPropertyNodes(Node node, Uri propertyUri) 
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
        //!!! also handle removing lists (rdf:first/rdf:rest/rdf:nil)
        //!!! and write test case for this
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
                //!!! this doesn't "feel" right -- what about sets of sets, etc? I suppose sets of sequences might work?
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
        if (!examined.contains(o)) {
            store(map, examined, o);
        }
    } else if (map.contains(o) && map.value(o) == Node()) {
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
            DEBUG << "listToPropertyNode: Obtained nil Node in list, skipping!" << endl;
            continue;
        } else if (pnodes.size() > 1) {
            std::cerr << "ObjectStorer::listToPropertyNode: Found set within sequence, can't handle this, using first element only" << std::endl; //!!!???
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
    if (m_fp != FollowNone) {
        examined.insert(o);
    }

    Node node = storeSingle(map, examined, o);
    
    QObject *parent = o->parent();
    Uri parentUri(m_tm.getRelationshipPrefix().toString() + "parent");
    Uri followsUri(m_tm.getRelationshipPrefix().toString() + "follows");

    if (parent) {
        if (m_fp & FollowParent) {
            DEBUG << "storeSingle: FollowParent is set, writing parent of " << node << endl;
            if (!examined.contains(parent)) {
                store(map, examined, parent);
            }
        } else if (map.contains(parent) && map.value(parent) == Node()) {
            // parent is to be written at some point: bring it forward
            // so we have a uri to refer to now
            DEBUG << "storeSingle: Parent of " << node << " has not been written yet, writing it" << endl;
            store(map, examined, parent);
        }
        Node pn = map.value(parent);
        if (pn != Node()) {
            m_s->remove(Triple(node, parentUri, Node()));
            m_s->add(Triple(node, parentUri, pn));
        }
    } else {
        // no parent
        m_s->remove(Triple(node, parentUri, Node()));
    }

    if (m_fp & FollowChildren) {
        foreach (QObject *c, o->children()) {
            if (!examined.contains(c)) {
                store(map, examined, c);
            }
        }
    }

    if (parent) {

        // write (references to) siblings

        QObject *previous = 0;
        QObjectList siblings = parent->children();
        for (int i = 0; i < siblings.size(); ++i) {
            if (siblings[i] == o) {
                if (i > 0) {
                    previous = siblings[i-1];
                    if (!(m_fp & FollowSiblings)) {
                        break;
                    }
                }
            } else {
                if (m_fp & FollowSiblings) {
                    if (!examined.contains(siblings[i])) {
                        DEBUG << "storeSingle: FollowSiblings is set, writing sibling of " << node << endl;
                        store(map, examined, siblings[i]);
                    }
                }
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
                    DEBUG << "storeSingle: Previous sibling of " << node << " has not been written yet, writing it" << endl;
                    store(map, examined, previous);
                }
            }

            Node sn = map.value(previous);
            if (sn != Node()) {
                m_s->remove(Triple(node, followsUri, Node()));
                m_s->add(Triple(node, followsUri, sn));
            }

        } else {
            // no previous sibling
            m_s->remove(Triple(node, followsUri, Node()));
        }
    }

    return node;
}

Node
ObjectStorer::D::storeSingle(ObjectNodeMap &map, ObjectSet &examined, QObject *o)
{
    // This function should only be called when we know we want to
    // store an object -- all conditions have been satisfied.  After
    // this has been called, there is guaranteed to be something
    // meaningful in the store and the ObjectNodeMap for this object

    QString className = o->metaObject()->className();

    Node node;

    QVariant uriVar = o->property("uri");

    if (uriVar != QVariant()) {
        if (Uri::isUri(uriVar)) {
            node = uriVar.value<Uri>();
        } else {
            node = m_s->expand(uriVar.toString());
        }
    } else if (!map.contains(o) && m_bp == BlankNodesAsNeeded) { //!!! I don't much like that name

        node = m_s->addBlankNode();

    } else {
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

    map[o] = node;

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
/*
void
ObjectStorer::storeProperties(QObject *o, Uri uri)
{
    m_d->storeProperties(o, uri);
}

Uri
ObjectStorer::storeObject(QObject *o)
{
    return m_d->storeObject(o);
}

Uri
ObjectStorer::storeObjectTree(QObject *root)
{
    return m_d->storeObjectTree(root);
}

void
ObjectStorer::storeAllObjects(QObjectList list)
{
    m_d->storeAllObjects(list);
}
*/
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
ObjectStorer::addStoreCallback(StoreCallback *cb)
{
    m_d->addStoreCallback(cb);
}

}

