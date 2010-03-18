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

#include "ObjectMapper.h"

#include "ObjectMapperExceptions.h"
#include "ObjectMapperForwarder.h"
#include "ObjectLoader.h"
#include "ObjectStorer.h"

#include "TypeMapping.h"

#include "TransactionalStore.h"
#include "Connection.h"

#include "../Debug.h"

#include <typeinfo>

#include <QMetaProperty>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QHash>

namespace Dataquay {
    
class
ObjectMapper::D : public QObject
{
    struct Network
    {
        ObjectLoader::NodeObjectMap nodeObjectMap;
        ObjectStorer::ObjectNodeMap objectNodeMap;
        
        Node getNodeForObject(QObject *o) {
            ObjectStorer::ObjectNodeMap::const_iterator i = objectNodeMap.find(o);
            if (i != objectNodeMap.end()) return i.value();
            return Node();
        }
        QObject *getObjectByNode(Node n) {
            ObjectLoader::NodeObjectMap::const_iterator i = nodeObjectMap.find(n);
            if (i != nodeObjectMap.end()) return i.value();
            return 0;
        }
    };

public:
    D(ObjectMapper *m, TransactionalStore *s) :
        m_m(m),
        m_s(s),
        m_c(s),
        m_mutex(QMutex::Recursive),
        m_inCommit(false),
        m_inReload(false)
    {
        m_loader = new ObjectLoader(&m_c);
        m_loader->setAbsentPropertyPolicy(ObjectLoader::ResetAbsentProperties);
        m_storer = new ObjectStorer(&m_c);
        m_storer->setPropertyStorePolicy(ObjectStorer::StoreIfChanged);
        m_storer->setBlankNodePolicy(ObjectStorer::NoBlankNodes); //!!!???
        connect(&m_c, SIGNAL(transactionCommitted(const ChangeSet &)),
                m_m, SLOT(transactionCommitted(const ChangeSet &)));
    }

    virtual ~D() {
        delete m_storer;
        delete m_loader;
    }
    
    TransactionalStore *getStore() {
        return m_s;
    }

    void setTypeMapping(const TypeMapping &tm) {
        m_tm = tm;
        m_loader->setTypeMapping(m_tm);
        m_storer->setTypeMapping(m_tm);
    }

    const TypeMapping &getTypeMapping() const {
        return m_tm;
    }

    Node getNodeForObject(QObject *o) {
        return m_n.getNodeForObject(o);
    }
    
    QObject *getObjectByNode(Node n) {
        return m_n.getObjectByNode(n);
    }

    void add(QObject *o) {
        try {
            manage(o);
        } catch (NoUriException) {
            // doesn't matter; it will be assigned one when mapped,
            // which will happen on the next commit because we are
            // adding it to the changed object map
        }
        m_changedObjects.insert(o);
    }

    void add(QObjectList ol) {
        foreach (QObject *o, ol) {
            add(o);
        }
    }

    void manage(QObject *o) {
        //!!! is object new? do we want to write it to store now, or
        //!!! just manage it?

        QMutexLocker locker(&m_mutex);
        
        if (m_n.objectNodeMap.contains(o)) {
            DEBUG << "ObjectMapper::manage: Object is already managed" << endl;
            return;
        }

        // The forwarder avoids us trying to connect potentially many,
        // many signals to the same mapper object -- which is slow.
        // Note the forwarder is a child of the ObjectMapper, so we
        // don't need to explicitly destroy it in dtor
        ObjectMapperForwarder *f = new ObjectMapperForwarder(m_m, o);
        m_forwarders.insert(o, f);

        Uri uri = o->property("uri").value<Uri>();
        if (uri == Uri()) {
            //!!! document this -- generally, document conditions for manage() to be used rather than add()
            throw NoUriException(o->objectName(), o->metaObject()->className());
        } else {
            m_n.objectNodeMap.insert(o, Node(uri));
            m_n.nodeObjectMap.insert(Node(uri), o);
        }

        //!!! also manage objects that are properties? and manage any new settings of those that appear when we commit?
    }

    void manage(QObjectList ol) {
        foreach (QObject *o, ol) {
            manage(o);
        }
    }

    void unmanage(QObject *) { } //!!!

    void unmanage(QObjectList ol) {
        foreach (QObject *o, ol) {
            unmanage(o);
        }
    }

//    void remap(QObject *) { } //!!! poor
//    void unmap(QObject *) { } //!!! poor

    void objectModified(QObject *o) {
        std::cerr << "objectModified(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        if (m_inReload) {
            // This signal must have been emitted by a modification
            // caused by our own transactionCommitted method (see
            // similar comment about m_inCommit in that method).
            std::cerr << "(by us, ignoring it)" << std::endl;
            return;
        }

        //!!! what if the thing that changed about the object was its URL???!!!

        m_changedObjects.insert(o);
        DEBUG << "ObjectMapper::objectModified done" << endl;
    }

    void objectDestroyed(QObject *o) {
        std::cerr << "objectDestroyed(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        if (m_inReload) {
            // This signal must have been emitted by a modification
            // caused by our own transactionCommitted method (see
            // similar comment about m_inCommit in that method).
            std::cerr << "(by us, ignoring it)" << std::endl;

            //!!! oo-er -- what if user removed the triples for a
            //!!! particular object from the store, thus causing us to
            //!!! delete the object from transactionCommitted, but
            //!!! that object was a parent of some other object(s) we
            //!!! manage, which Qt will automatically destroy when the
            //!!! parent is destroyed... so we will get our
            //!!! objectDestroyed when those children are destroyed,
            //!!! and unlike the parent's signal, we actually need to
            //!!! take note of those...!

            //!!! n.b. also that making a special case for children
            //!!! won't solve the problem generally, as any external
            //!!! code could be connected to an object's
            //!!! objectDestroyed signal, deleting other objects on
            //!!! receipt

            // ^^^ write a unit test for this!

            return;
        }
        m_changedObjects.remove(o);
        if (m_n.objectNodeMap.contains(o)) {
            m_deletedObjectNodes.insert(m_n.objectNodeMap[o]);
            m_n.objectNodeMap.remove(o);
        }
        if (m_forwarders.contains(o)) {
            delete m_forwarders.value(o);
            m_forwarders.remove(o);
        }
        DEBUG << "ObjectMapper::objectDestroyed done" << endl;
    }

    void transactionCommitted(const ChangeSet &cs) {
        std::cerr << "transactionCommitted" << std::endl;
        QMutexLocker locker(&m_mutex);
        if (m_inCommit) {
            // This signal must have been emitted by a commit invoked
            // from our own commit method.  We only set m_inCommit
            // true for a period in which our own mutex is held, so if
            // it is set here, we must be in a recursive call from
            // that mutex section (noting that m_mutex is a recursive
            // mutex, otherwise we would have deadlocked).  And we
            // don't want to update on the basis of our own commits,
            // only on the basis of commits happening elsewhere.
            std::cerr << "(by us, ignoring it)" << std::endl;
            return;
        }
        //!!! but now what?
        m_inReload = true;
        DEBUG << "ObjectMapper: Synchronising from " << cs.size()
              << " change(s) in transaction" << endl;
        //!!! reload objects

        //!!! if an object has been effectively deleted from the
        //!!! store, we can't know that without querying the store to
        //!!! discover whether any triples remain -- so we should let
        //!!! something like ObjectLoader::reload() handle deleting
        //!!! objects that have been removed from store

        // If an object's node appears as the "subject" of a
        // predicate, then we should reload that object.  What if it
        // appears as the "object"?  Do we ever need to reload then?
        // I don't think so...
        QSet<Node> toReload;
        foreach (const Change &c, cs) {
            toReload.insert(c.second.a);
        }
        Nodes nodes;
        foreach (const Node &n, toReload) {
            nodes.push_back(n);
        }
        m_loader->load(nodes, m_n.nodeObjectMap);

        // The load call will have updated m_n.nodeObjectMap; sync the
        // unchanged (since the last commit call) m_n.objectNodeMap
        // from it
        syncMap(m_n.objectNodeMap, m_n.nodeObjectMap);

        m_inReload = false;
        DEBUG << "ObjectMapper::transactionCommitted done" << endl;
    }

    void commit() { 

        QMutexLocker locker(&m_mutex);
        std::cerr << "ObjectMapper: Synchronising " << m_changedObjects.size()
                  << " changed and " << m_deletedObjectNodes.size()
                  << " deleted object(s)" << std::endl;
        //!!! if an object has been added as a new sibling of existing
        //!!! objects, then we presumably may have to rewrite our
        //!!! follows relationships?

        // What other objects can be affected by the addition or
        // modification of an object?

        // - Adding a new child -> affects nothing directly, as child
        // goes at end

        foreach (Node n, m_deletedObjectNodes) {
            m_storer->removeObject(n);
        }

        QObjectList ol;
        foreach (QObject *o, m_changedObjects) ol.push_back(o);
        m_storer->store(ol, m_n.objectNodeMap);

        m_inCommit = true;
        m_c.commit();
        m_inCommit = false;

        // The store call will have updated m_n.objectNodeMap; sync
        // the unchanged (since the last "external" transaction)
        // m_n.nodeObjectMap from it
        syncMap(m_n.nodeObjectMap, m_n.objectNodeMap);

        m_deletedObjectNodes.clear();
        m_changedObjects.clear();
        DEBUG << "ObjectMapper::commit done" << endl;
    }

private:
    ObjectMapper *m_m;
    TransactionalStore *m_s;
    Connection m_c;
    TypeMapping m_tm;
    Network m_n;

    QMutex m_mutex;
    QHash<QObject *, ObjectMapperForwarder *> m_forwarders;
    QSet<QObject *> m_changedObjects;
    QSet<Node> m_deletedObjectNodes;

    bool m_inCommit;
    bool m_inReload;

    ObjectLoader *m_loader;
    ObjectStorer *m_storer;

    class InternalMappingInconsistency : virtual public std::exception {
    public:
        InternalMappingInconsistency(QString a, QString b) throw() :
            m_a(a), m_b(b) { }
        virtual ~InternalMappingInconsistency() throw() { }
        virtual const char *what() const throw() {
            return QString("Internal inconsistency: internal map from %1 to %2 "
                           "contains different %2 from that found in map from "
                           "%2 to %1").arg(m_a).arg(m_b).toLocal8Bit().data();
        }
    protected:
        QString m_a;
        QString m_b;
    };

    template <typename A, typename B>
    void syncMap(QHash<B,A> &to, QHash<A,B> &from) {

        //!!! rather slow, I think this is not the best way to do this!

        // are we in fact doing the same thing as copying all elements
        // from "from" to a new reverse map and assigning that to
        // "to", plus additional consistency check?  would that be
        // quicker?  or would it be better to do the extra work to
        // keep tabs on exactly what has changed?  perhaps we should
        // wait until this function becomes a bottleneck and then
        // compare methods

        int updated = 0;
        QSet<B> found;
        for (typename QHash<A,B>::iterator i = from.begin();
             i != from.end(); ++i) {
            A a = i.key();
            B b = i.value();
            A otherA = to.value(b);
            if (otherA != A()) {
                if (otherA != a) {
                    throw InternalMappingInconsistency(typeid(B).name(),
                                                       typeid(A).name());
                }
            } else {
                to.insert(b, a);
                ++updated;
            }
            found.insert(b);
        }
        QSet<B> unfound;
        for (typename QHash<B,A>::iterator i = to.begin();
             i != to.end(); ++i) {
            B b = i.key();
            if (!found.contains(b)) {
                unfound.insert(b);
            }
        }
        for (typename QSet<B>::iterator i = unfound.begin();
             i != unfound.end(); ++i) {
            to.remove(*i);
        }
        std::cerr << "syncMap: Note: updated " << updated << " and removed "
                  << unfound.size() << " element(s) from target map" << std::endl;
    }
};

ObjectMapper::ObjectMapper(TransactionalStore *s) :
    m_d(new D(this, s))
{
}

ObjectMapper::~ObjectMapper()
{
    delete m_d;
}

TransactionalStore *
ObjectMapper::getStore()
{
    return m_d->getStore();
}

void
ObjectMapper::setTypeMapping(const TypeMapping &tm)
{
    m_d->setTypeMapping(tm);
}

const TypeMapping &
ObjectMapper::getTypeMapping() const
{
    return m_d->getTypeMapping();
}

Node
ObjectMapper::getNodeForObject(QObject *o) const
{
    return m_d->getNodeForObject(o);
}

QObject *
ObjectMapper::getObjectByNode(Node n) const
{
    return m_d->getObjectByNode(n);
}

void
ObjectMapper::add(QObject *o)
{
    m_d->add(o);
}

void
ObjectMapper::add(QObjectList ol)
{
    m_d->add(ol);
}

void
ObjectMapper::manage(QObject *o)
{
    m_d->manage(o);
}

void
ObjectMapper::manage(QObjectList ol)
{
    m_d->manage(ol);
}

void
ObjectMapper::unmanage(QObject *o)
{
    m_d->unmanage(o);
}

void
ObjectMapper::unmanage(QObjectList ol)
{
    m_d->unmanage(ol);
}
/*
void
ObjectMapper::remap(QObject *o)
{
    m_d->remap(o);
}

void
ObjectMapper::unmap(QObject *o)
{
    m_d->unmap(o);
}
*/
void
ObjectMapper::commit()
{
    m_d->commit();
}

void
ObjectMapper::objectModified(QObject *o)
{
    m_d->objectModified(o);
}

void
ObjectMapper::objectDestroyed(QObject *o)
{
    m_d->objectDestroyed(o);
}

void
ObjectMapper::transactionCommitted(const ChangeSet &cs)
{
    m_d->transactionCommitted(cs);
}

}


	
