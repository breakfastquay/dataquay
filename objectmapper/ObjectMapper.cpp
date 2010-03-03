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

#include "../Debug.h"

#include <QMetaProperty>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

namespace Dataquay {

class
ObjectMapper::Network::D
{
public:
    ObjectLoader::NodeObjectMap nodeObjectMap;
    ObjectStorer::ObjectNodeMap objectNodeMap;
};

ObjectMapper::Network::Network() :
    m_d(new D())
{
}

ObjectMapper::Network::Network(const Network &n) :
    m_d(new D(*n.m_d))
{
}

ObjectMapper::Network &
ObjectMapper::Network::operator=(const Network &n)
{
    if (&n != this) {
        delete m_d;
        m_d = new D(*n.m_d);
    }
    return *this;
}

ObjectMapper::Network::~Network()
{
    delete m_d;
}

class
ObjectMapper::D : public QObject
{
public:
    D(ObjectMapper *m, Store *s) :
        m_m(m),
        m_s(s)
    {
        m_loader = new ObjectLoader(m_s);
        m_storer = new ObjectStorer(m_s);
    }

    virtual ~D() { }
    
    Store *getStore() {
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

    const Network &getNetwork() const {
        return m_n;
    }

    void addToNetwork(QObject *o) {
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
                DEBUG << "ObjectMapper::addToNetwork: No notify signal for property " << property.name() << endl;
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

            if (!connect(o, ba.data(), m_m, SLOT(propertyChanged()))) {
                std::cerr << "ObjectMapper::addToNetwork: Failed to connect notify signal" << std::endl;
            }

            connect(o, SIGNAL(destroyed(QObject *)),
                    m_m, SLOT(objectDestroyed(QObject *)));
        }
    }

    void removeFromNetwork(QObject *) { }
    void remap(QObject *) { } //!!! poor
    void unmap(QObject *) { } //!!! poor
    void propertyChangedOn(QObject *o) {
        std::cerr << "propertyChangedOn(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        m_changedObjects.insert(o);
    }
    void objectDestroyed(QObject *o) {
        std::cerr << "objectDestroyed(" << o << ")" << std::endl;
        QMutexLocker locker(&m_mutex);
        m_changedObjects.remove(o);
    }
    void commit() { 
        QMutexLocker locker(&m_mutex);
        foreach (QObject *o, m_changedObjects) {
            m_storer->store(m_n.m_d->objectNodeMap, o);
        }
    }

private:
    ObjectMapper *m_m;
    Store *m_s;
    TypeMapping m_tm;
    Network m_n;

    QMutex m_mutex;
    QSet<QObject *> m_changedObjects;

    ObjectLoader *m_loader;
    ObjectStorer *m_storer;
};

ObjectMapper::ObjectMapper(Store *s) :
    m_d(new D(this, s))
{
}

ObjectMapper::~ObjectMapper()
{
    delete m_d;
}

Store *
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

const ObjectMapper::Network &
ObjectMapper::getNetwork() const
{
    return m_d->getNetwork();
}

void
ObjectMapper::addToNetwork(QObject *o)
{
    m_d->addToNetwork(o);
}

void
ObjectMapper::removeFromNetwork(QObject *o)
{
    m_d->removeFromNetwork(o);
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
ObjectMapper::propertyChanged()
{
    m_d->propertyChangedOn(sender());
}

void
ObjectMapper::objectDestroyed(QObject *o)
{
    m_d->objectDestroyed(o);
}

}


	
