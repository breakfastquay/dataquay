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
#include "Node.h"

namespace Dataquay
{

QString
PropertyObject::m_defaultPrefix = "property:";

PropertyObject::PropertyObject(Store *s, QUrl uri) :
    m_store(s), m_pfx(m_defaultPrefix), m_node(new Node(uri))
{
}

PropertyObject::PropertyObject(Store *s, QString uri) :
    m_store(s), m_pfx(m_defaultPrefix), m_node(new Node(s->expand(uri)))
{
}

PropertyObject::PropertyObject(Store *s, QString pfx, QUrl uri) :
    m_store(s), m_pfx(pfx), m_node(new Node(uri))
{
}

PropertyObject::PropertyObject(Store *s, QString pfx, QString uri) :
    m_store(s), m_pfx(pfx), m_node(new Node(s->expand(uri)))
{
}

PropertyObject::PropertyObject(Store *s, QString pfx, Node node) :
    m_store(s), m_pfx(pfx), m_node(new Node(node))
{
}

PropertyObject::~PropertyObject()
{
    delete m_node;
}

QUrl
PropertyObject::getObjectType() const
{
    Triple t = m_store->matchFirst(Triple(*m_node, "a", Node()));
    if (t != Triple()) {
        return QUrl(t.c.value);
    } else {
        return QUrl();
    }
}

QUrl
PropertyObject::getObjectType(Transaction *tx) const
{
    Store *s = getStore(tx);
    Triple t = s->matchFirst(Triple(*m_node, "a", Node()));
    if (t != Triple()) {
        return QUrl(t.c.value);
    } else {
        return QUrl();
    }
}

bool
PropertyObject::hasProperty(QString name) const
{
    QUrl property = getPropertyUri(name);
    Triple r = m_store->matchFirst(Triple(*m_node, property, Node()));
    return (r != Triple());
}

bool
PropertyObject::hasProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple r = s->matchFirst(Triple(*m_node, property, Node()));
    return (r != Triple());
}

QVariant
PropertyObject::getProperty(QString name) const
{
    QUrl property = getPropertyUri(name);
    Triple r = m_store->matchFirst(Triple(*m_node, property, Node()));
    if (r == Triple()) return QVariant();
    return r.c.toVariant();
}

QVariant
PropertyObject::getProperty(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple r = s->matchFirst(Triple(*m_node, property, Node()));
    if (r == Triple()) return QVariant();
    return r.c.toVariant();
}

QVariantList
PropertyObject::getPropertyList(QString name) const
{
    QUrl property = getPropertyUri(name);
    Triples r = m_store->match(Triple(*m_node, property, Node()));
    QVariantList vl;
    for (int i = 0; i < r.size(); ++i) {
        vl.push_back(r[i].c.toVariant());
    }
    return vl;
}

QVariantList
PropertyObject::getPropertyList(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triples r = s->match(Triple(*m_node, property, Node()));
    QVariantList vl;
    for (int i = 0; i < r.size(); ++i) {
        vl.push_back(r[i].c.toVariant());
    }
    return vl;
}

Node
PropertyObject::getPropertyNode(QString name) const
{
    QUrl property = getPropertyUri(name);
    Triple r = m_store->matchFirst(Triple(*m_node, property, Node()));
    return r.c;
}

Node
PropertyObject::getPropertyNode(Transaction *tx, QString name) const
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple r = s->matchFirst(Triple(*m_node, property, Node()));
    return r.c;
}

QStringList
PropertyObject::getPropertyNames() const
{
    QStringList properties;
    QString expfx = m_store->expand(m_pfx).toString();
    Triples ts = m_store->match(Triple(*m_node, Node(), Node()));
    for (int i = 0; i < ts.size(); ++i) {
        QString propertyUri = ts[i].b.value;
        if (propertyUri.startsWith(expfx)) {
            properties.push_back(propertyUri.replace(expfx, ""));
        }
    }
    return properties;
}

QStringList
PropertyObject::getPropertyNames(Transaction *tx) const
{
    Store *s = getStore(tx);
    QStringList properties;
    QString expfx = m_store->expand(m_pfx).toString();
    Triples ts = s->match(Triple(*m_node, Node(), Node()));
    for (int i = 0; i < ts.size(); ++i) {
        QString propertyUri = ts[i].b.value;
        if (propertyUri.startsWith(expfx)) {
            properties.push_back(propertyUri.replace(expfx, ""));
        }
    }
    return properties;
}

void
PropertyObject::setProperty(QString name, QVariant value)
{
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    m_store->remove(t); // remove all matching triples
    t.c = Node::fromVariant(value);
    m_store->add(t);
}

void
PropertyObject::setProperty(QString name, QUrl uri)
{
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    m_store->remove(t); // remove all matching triples
    t.c = Node(uri);
    m_store->add(t);
}

void
PropertyObject::setProperty(QString name, Node node)
{
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    m_store->remove(t); // remove all matching triples
    t.c = node;
    m_store->add(t);
}

void
PropertyObject::setProperty(Transaction *tx, QString name, QVariant value)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    s->remove(t); // remove all matching triples
    t.c = Node::fromVariant(value);
    s->add(t);
}

void
PropertyObject::setProperty(Transaction *tx, QString name, QUrl uri)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    s->remove(t); // remove all matching triples
    t.c = Node(uri);
    s->add(t);
}

void
PropertyObject::setProperty(Transaction *tx, QString name, Node node)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
    s->remove(t); // remove all matching triples
    t.c = node;
    s->add(t);
}
    
void
PropertyObject::removeProperty(Transaction *tx, QString name)
{
    Store *s = getStore(tx);
    QUrl property = getPropertyUri(name);
    Triple t(*m_node, property, Node());
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
    return m_store->expand(m_pfx + name);
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

QUrl
CacheingPropertyObject::getObjectType() const
{
    //!!! too slow
    return m_po.getObjectType();
}

QUrl
CacheingPropertyObject::getObjectType(Transaction *tx) const
{
    //!!! too slow
    return m_po.getObjectType(tx);
}

bool
CacheingPropertyObject::hasProperty(QString name) const
{
    //!!! too slow
    return m_po.hasProperty(name);
}

bool
CacheingPropertyObject::hasProperty(Transaction *tx, QString name) const
{
    //!!! too slow
    return m_po.hasProperty(tx, name);
}

QVariant
CacheingPropertyObject::getProperty(QString name) const
{
    if (!m_cache.contains(name)) {
        QVariant value = m_po.getProperty(name);
        m_cache[name] = value;
        return value;
    }
    return m_cache[name];
}

QVariant
CacheingPropertyObject::getProperty(Transaction *tx, QString name) const
{
    if (!m_cache.contains(name)) {
        QVariant value = m_po.getProperty(tx, name);
        m_cache[name] = value;
        return value;
    }
    return m_cache[name];
}

void
CacheingPropertyObject::setProperty(QString name, QVariant value)
{
    m_po.setProperty(name, value);
    m_cache[name] = value;
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
    m_cache.remove(name);
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

