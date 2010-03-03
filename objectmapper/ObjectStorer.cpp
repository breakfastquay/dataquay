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

#include "ObjectStorer.h"
#include "ObjectBuilder.h"
#include "ContainerBuilder.h"
#include "TypeMapping.h"

#include "../PropertyObject.h"
#include "../Store.h"
#include "../Debug.h"

#include <memory>

#include <QMetaProperty>

namespace Dataquay
{

class ObjectStorer::D
{
public:
    D(ObjectStorer *m, Store *s) :
        m_m(m),
        m_ob(ObjectBuilder::getInstance()),
        m_cb(ContainerBuilder::getInstance()),
        m_s(s),
        m_psp(StoreAlways),
        m_osp(StoreAllObjects),
        m_bp(BlankNodesAsNeeded) {
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

    void storeProperties(QObject *o, Uri uri) {
        ObjectNodeMap map;
        storeProperties(map, o, uri, false);
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

    Node store(ObjectNodeMap &map, QObject *o) {
        return storeSingle(map, o, true);
    }

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
    ObjectStorePolicy m_osp;
    BlankNodePolicy m_bp;
    QList<StoreCallback *> m_storeCallbacks;

    Node storeTree(ObjectNodeMap &map, QObject *o);
    Node storeSingle(ObjectNodeMap &map, QObject *o, bool follow, bool blank = false);

    void callStoreCallbacks(ObjectNodeMap &map, QObject *o, Node node);

    void storeProperties(ObjectNodeMap &map, QObject *o, Node node, bool follow);            
    void removeOldPropertyNodes(Node node, Uri propertyUri);
    Nodes variantToPropertyNodeList(ObjectNodeMap &map, QVariant v, bool follow);
    Node objectToPropertyNode(ObjectNodeMap &map, QObject *o, bool follow);
    Node listToPropertyNode(ObjectNodeMap &map, QVariantList list, bool follow);
};

void
ObjectStorer::D::storeProperties(ObjectNodeMap &map, QObject *o,
                                 Node node, bool follow)
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
                }
            }
        }

        DEBUG << "For object " << node.value << " writing property " << pname << " of type " << property.type() << endl;

        // 

        //!!! n.b. removing the old property value is problematic if it was a blank node which is now no longer referred to by anything else -- we can end up with all sorts of "spare" blank nodes -- should we check for this and remove other triples with it as subject? <-- removeOldPropertyNodes does this now

        Nodes pnodes = variantToPropertyNodeList(map, value, follow);

        Uri puri;
	if (!m_tm.getPropertyUri(cname, pname, puri)) {
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
    }
}

Nodes
ObjectStorer::D::variantToPropertyNodeList(ObjectNodeMap &map, QVariant v, bool follow)
{
    const char *typeName = 0;

//!!! unnecessary    if (v.type() == QVariant::UserType) {
    typeName = QMetaType::typeName(v.userType());
//    } else {
//        typeName = v.typeName();
//    }

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
ObjectStorer::D::objectToPropertyNode(ObjectNodeMap &map, QObject *o, bool follow)
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
ObjectStorer::D::listToPropertyNode(ObjectNodeMap &map, QVariantList list, bool follow)
{
    DEBUG << "listToPropertyNode: have " << list.size() << " items" << endl;

    Node node, first, previous;

    foreach (QVariant v, list) {

        Nodes pnodes = variantToPropertyNodeList(map, v, follow);
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
ObjectStorer::D::storeSingle(ObjectNodeMap &map, QObject *o, bool follow, bool blank)
{ //!!! crap api
    DEBUG << "storeSingle: blank = " << blank << endl;

    //!!! This doesn't seem right; if this function has been called
    //!!! (externally) for an object, then the object is clearly
    //!!! intended to be written again.  We should be simply not
    //!!! calling the function in the first place if it is not to be
    //!!! written
//    if (map.contains(o) && map[o] != Node()) return map[o];

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
    
    QObject *parent = o->parent();
    if (parent && map.contains(parent)) {
        
        // if the parent is in the map (i.e. wants to be written) but
        // has not been written yet, write it now
        if (map[parent] == Node()) {
            if (follow) {
                DEBUG << "storeSingle: Parent of " << node << " has not been written yet, writing it" << endl;
                storeSingle(map, parent, follow, blank);
            }
        }
        
        if (map[parent] != Node()) {
            m_s->add(Triple(node,
                            m_tm.getRelationshipPrefix().toString() + "parent",
                            map[o->parent()]));
        }

    } else if (parent) {
        DEBUG << "storeSingle: Not writing parent " << parent << " (it is not in map)" << endl;
    }

    storeProperties(map, o, node, follow);

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

Node
ObjectStorer::D::storeTree(ObjectNodeMap &map, QObject *o)
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
ObjectStorer::setObjectStorePolicy(ObjectStorePolicy policy)
{
    m_d->setObjectStorePolicy(policy);
}

ObjectStorer::ObjectStorePolicy
ObjectStorer::getObjectStorePolicy() const
{
    return m_d->getObjectStorePolicy();
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

Node
ObjectStorer::store(ObjectNodeMap &map, QObject *o)
{
    return m_d->store(map, o);
}

void
ObjectStorer::addStoreCallback(StoreCallback *cb)
{
    m_d->addStoreCallback(cb);
}

}

