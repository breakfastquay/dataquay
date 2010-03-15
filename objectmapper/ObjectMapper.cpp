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

#include "ObjectLoader.h"
#include "ObjectStorer.h"

#include "TypeMapping.h"

#include "TransactionalStore.h"
#include "Connection.h"

#include "../Debug.h"

#include <QMetaProperty>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

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
        m_inCommit(false)
    {
        m_loader = new ObjectLoader(&m_c);
        m_storer = new ObjectStorer(&m_c);
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

    void manage(QObject *o) {
        //!!! is object new? do we want to write it to store now, or
        //!!! just manage it?
        
        for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {

            QMetaProperty property = o->metaObject()->property(i);

            if (!property.isStored() ||
                !property.isReadable() ||
                !property.isWritable()) {
                continue;
            }
            
            if (!property.hasNotifySignal()) {
                DEBUG << "ObjectMapper::manage: No notify signal for property " << property.name() << endl;
                continue;
            }

            // Signals can be connected to slots with fewer arguments,
            // so long as the arguments they do have match.  So we
            // connect the property notify signal to our universal
            // property-changed slot, and use the sender() of that to
            // discover which object's property has changed.
            // Unfortunately, we don't then know which property it was
            // that changed, as the identity of the signal that
            // activated the slot is not available to us.  The
            // annoying part of this is that Qt does actually store
            // the identity of the signal in the same structure as
            // used for the sender() object data; it just doesn't (at
            // least as of Qt 4.5) make it available through the
            // public API.

            QString sig = QString("%1%2").arg(QSIGNAL_CODE)
                .arg(property.notifySignal().signature());
            QByteArray ba = sig.toLocal8Bit();

            if (!connect(o, ba.data(), m_m, SLOT(objectModified()))) {
                std::cerr << "ObjectMapper::manage: Failed to connect notify signal" << std::endl;
            }

            connect(o, SIGNAL(destroyed(QObject *)),
                    m_m, SLOT(objectDestroyed(QObject *)));
        }

        //!!! also manage objects that are properties? and manage any new settings of those that appear when we commit?

        QMutexLocker locker(&m_mutex);
        m_changedObjects.insert(o);
    }

    void unmanage(QObject *) { } //!!!
    void remap(QObject *) { } //!!! poor
    void unmap(QObject *) { } //!!! poor
    void objectModified(QObject *o) {
        std::cerr << "objectModified(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        if (m_inReload) {
            // This signal must have been emitted by a modification
            // caused by our own transactionCommitted method (see
            // similar comment about m_inCommit in that method).
            return;
        }
        m_changedObjects.insert(o);
    }
    void objectDestroyed(QObject *o) {
        std::cerr << "objectDestroyed(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        if (m_inReload) {
            // This signal must have been emitted by a modification
            // caused by our own transactionCommitted method (see
            // similar comment about m_inCommit in that method).
            return;
        }
        m_changedObjects.remove(o);
        if (m_n.objectNodeMap.contains(o)) {
            m_deletedObjectNodes.insert(m_n.objectNodeMap[o]);
        }
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
            return;
        }
        //!!! but now what?
        m_inReload = true;
        //!!! reload objects
        foreach (const Change &c, cs) {

            //!!!
        }            
        m_inReload = false;
    }
    void commit() { 
        QMutexLocker locker(&m_mutex);
        //!!! if an object has been added as a new sibling of existing
        //!!! objects, then we presumably may have to rewrite our
        //!!! follows relationships
        foreach (Node n, m_deletedObjectNodes) {
            m_storer->removeObject(n);
        }
        QObjectList ol;
        foreach (QObject *o, m_changedObjects) ol.push_back(o);
        m_storer->store(ol, m_n.objectNodeMap);
        m_inCommit = true;
        m_c.commit();
        m_inCommit = false;
        m_deletedObjectNodes.clear();
        m_changedObjects.clear();
    }

private:
    ObjectMapper *m_m;
    TransactionalStore *m_s;
    Connection m_c;
    TypeMapping m_tm;
    Network m_n;

    QMutex m_mutex;
    QSet<QObject *> m_changedObjects;
    QSet<Node> m_deletedObjectNodes;

    bool m_inCommit;
    bool m_inReload;

    ObjectLoader *m_loader;
    ObjectStorer *m_storer;
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
ObjectMapper::manage(QObject *o)
{
    m_d->manage(o);
}

void
ObjectMapper::unmanage(QObject *o)
{
    m_d->unmanage(o);
}

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

void
ObjectMapper::commit()
{
    m_d->commit();
}

void
ObjectMapper::objectModified()
{
    m_d->objectModified(sender());
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


	
