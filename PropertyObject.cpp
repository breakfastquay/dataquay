/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "PropertyObject.h"

namespace Dataquay
{

PropertyObject::PropertyObject(Store *s, QUrl uri) :
    m_store(s), m_uri(uri)
{
}

PropertyObject::PropertyObject(Store *s, QString uri) :
    m_store(s), m_uri(s->expand(uri))
{
}

bool
PropertyObject::hasProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    // a highly inefficient use of match(), but Store has no
    // wildcarded contains()...
    Triples r = s->match(Triple(m_uri, property, Node()));
    return !r.empty();
}

QVariant
PropertyObject::getProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triples r = s->match(Triple(m_uri, property, Node()));
    if (r.empty()) {
        return QVariant();
    }
    return r[0].c.toVariant();
}

void
PropertyObject::setProperty(Transaction *tx, QString name, QVariant value)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(m_uri, property, Node());
    Triples r = s->match(t);
    for (int i = 0; i < r.size(); ++i) s->remove(r[i]);
    t.c = Node::fromVariant(value);
    s->add(t);
}
    
void
PropertyObject::removeProperty(Transaction *tx, QString name)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(m_uri, property, Node());
    s->remove(t);
}

Store *
PropertyObject::getStore(Transaction *tx) const
{
    if (tx == NoTransaction) return m_store;
    else return tx;
}

QUrl
PropertyObject::getPropertyUri(QString name) const
{
    return m_store->expand("turboprop:" + name);
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QUrl uri) :
    m_po(s, uri)
{
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QString uri) :
    m_po(s, uri)
{
}

bool
CacheingPropertyObject::hasProperty(Transaction *tx, QString name) const
{
    return m_po.hasProperty(tx, name);
}

QVariant
CacheingPropertyObject::getProperty(Transaction *tx, QString name) const
{
    StringVariantMap::iterator i = m_cache.find(name);
    if (i == m_cache.end()) {
        QVariant value = m_po.getProperty(tx, name);
        m_cache[name] = value;
        return value;
    }
    return i->second;
}

void
CacheingPropertyObject::setProperty(Transaction *tx, QString name, QVariant value)
{
    m_cache[name] = value;
    m_po.setProperty(tx, name, value);
}

void
CacheingPropertyObject::removeProperty(Transaction *tx, QString name)
{
    m_cache.erase(name);
    m_po.removeProperty(tx, name);
}

Store *
CacheingPropertyObject::getStore(Transaction *tx) const 
{
    return m_po.getStore(tx);
}

QUrl
CacheingPropertyObject::getPropertyUri(QString name) const
{
    return m_po.getPropertyUri(name);
}

}

