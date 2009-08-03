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

#include "PropertyObject.h"

#include "Transaction.h"

namespace Dataquay
{

QString
PropertyObject::m_defaultPrefix = "property";

PropertyObject::PropertyObject(Store *s, QUrl uri) :
    m_store(s), m_pfx(m_defaultPrefix), m_uri(uri)
{
}

PropertyObject::PropertyObject(Store *s, QString uri) :
    m_store(s), m_pfx(m_defaultPrefix), m_uri(s->expand(uri))
{
}

PropertyObject::PropertyObject(Store *s, QString pfx, QUrl uri) :
    m_store(s), m_pfx(pfx), m_uri(uri)
{
}

PropertyObject::PropertyObject(Store *s, QString pfx, QString uri) :
    m_store(s), m_pfx(pfx), m_uri(s->expand(uri))
{
}

bool
PropertyObject::hasProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple r = s->matchFirst(Triple(m_uri, property, Node()));
    return (r != Triple());
}

QVariant
PropertyObject::getProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple r = s->matchFirst(Triple(m_uri, property, Node()));
    if (r == Triple()) return QVariant();
    return r.c.toVariant();
}

void
PropertyObject::setProperty(Transaction *tx, QString name, QVariant value)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(m_uri, property, Node());
    s->remove(t); // remove all matching triples
    t.c = Node::fromVariant(value);
    s->add(t);
}
    
void
PropertyObject::removeProperty(Transaction *tx, QString name)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(m_uri, property, Node());
    s->remove(t); // remove all matching triples
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
    if (name == "a") return m_store->expand(name);
    if (name.contains(':')) return m_store->expand(name);
    return m_store->expand(m_pfx + ":" + name);
}

void
PropertyObject::setDefaultPropertyPrefix(QString prefix)
{
    m_defaultPrefix = prefix;
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QUrl uri) :
    m_po(s, uri)
{
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QString uri) :
    m_po(s, uri)
{
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QString pfx, QUrl uri) :
    m_po(s, pfx, uri)
{
}

CacheingPropertyObject::CacheingPropertyObject(Store *s, QString pfx, QString uri) :
    m_po(s, pfx, uri)
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
    m_po.setProperty(tx, name, value);
    m_cache[name] = value;
}

void
CacheingPropertyObject::removeProperty(Transaction *tx, QString name)
{
    m_po.removeProperty(tx, name);
    m_cache.erase(name);
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

