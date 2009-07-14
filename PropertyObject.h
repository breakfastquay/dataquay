/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_PROPERTY_OBJECT_H_
#define _RDF_PROPERTY_OBJECT_H_

#include "Transaction.h"
#include "RDFException.h"

#include <map>

namespace Dataquay
{

class PropertyNotFound : virtual public std::exception //!!! lose this class
{
public:
    PropertyNotFound(QString uri, QString propuri, QString prop) throw() :
        m_uri(uri), m_propuri(propuri), m_prop(prop) { }
    virtual ~PropertyNotFound() throw() { }
    virtual const char *what() const throw();
    
private:
    QString m_uri;
    QString m_propuri;
    QString m_prop;
};

//!!! pull out implementations into cpp file for clarity

class PropertyObject
{
public:
    PropertyObject(Store *s, QUrl uri) :
        m_store(s), m_uri(uri) { }

    PropertyObject(Store *s, QString uri) :
        m_store(s), m_uri(s->expand(uri)) { }

    bool hasProperty(Transaction *tx, QString name) const
        throw (RDFException) {
        Store *s = getStore(tx);
        QUrl property = getPropertyUri(name);
        //!!! the most inefficient use of match(), but Store has no
        // contains()... is it worth correcting this?
        Triples r = s->match(Triple(m_uri, property, Node()));
        return !r.empty();
    }

    QVariant getProperty(Transaction *tx, QString name) const
        throw (PropertyNotFound, RDFException) {
        Store *s = getStore(tx);
        QUrl property = getPropertyUri(name);
        Triples r = s->match(Triple(m_uri, property, Node()));
        if (r.empty()) {
            // !!! best not to throw; null QVariant is informative enough
//!!!??? what is the right thing?            throw PropertyNotFound(m_uri.toString(), property.toString(), name);
            return QVariant();
        }
        return r[0].c.toVariant();
    }

    void setProperty(Transaction *tx, QString name, QVariant value)
        throw (RDFException) {
        Store *s = getStore(tx);
        QUrl property = getPropertyUri(name);
        Triple t(m_uri, property, Node());
        Triples r = s->match(t);
        for (int i = 0; i < r.size(); ++i) s->remove(r[i]);
        t.c = Node::fromVariant(value);
        s->add(t);
    }
/* handy function, but doesn't really fit here... most prospective
 * callers might rather be using sparql anyway

    /// Return all nodes in store with the given value for the given property
    Nodes match(Transaction *tx, QString name, QVariant value)
        throw (RDFException) {
        Store *s = getStore(tx);
        QUrl property = getPropertyUri(name);
        Node c = Node::fromVariant(value);
        Triples t = s->match(Triple(Node(), property, c));
        Nodes result;
        for (Triples::const_iterator i = t.begin(); i != t.end(); ++i) {
            result.push_back(i->a);
        }
        return result;
    }
*/
    
    void removeProperty(Transaction *tx, QString name)
        throw (RDFException) {
        Store *s = getStore(tx);
        QUrl property = getPropertyUri(name);
        Triple t(m_uri, property, Node());
        s->remove(t);
    }

    Store *getStore(Transaction *tx) const
        throw () {
        if (tx == NoTransaction) return m_store;
        else return tx;
    }

    QUrl getPropertyUri(QString name) const
        throw () {
        return m_store->expand("turboprop:" + name);
    }
    
private:
    Store *m_store;
    QUrl m_uri;
};

class CacheingPropertyObject
{
public:
    CacheingPropertyObject(Store *s, QUrl uri) :
        m_po(s, uri) { }

    CacheingPropertyObject(Store *s, QString uri) :
        m_po(s, uri) { }

    QVariant getProperty(Transaction *tx, QString name) const
        throw (PropertyNotFound, RDFException) {
        StringVariantMap::iterator i = m_cache.find(name);
        if (i == m_cache.end()) {
            QVariant value = m_po.getProperty(tx, name);
            m_cache[name] = value;
            return value;
        }
        return i->second;
    }

    void setProperty(Transaction *tx, QString name, QVariant value)
        throw (RDFException) {
        m_cache[name] = value;
        m_po.setProperty(tx, name, value);
    }

    Store *getStore(Transaction *tx) const 
        throw () {
        return m_po.getStore(tx);
    }

    QUrl getPropertyUri(QString name) const
        throw () {
        return m_po.getPropertyUri(name);
    }
    
/*
    Nodes match(Transaction *tx, QString name, QVariant value)
        throw (RDFException) {
        return m_po.match(tx, name, value);
    }
*/
private:
    PropertyObject m_po;
    typedef std::map<QString, QVariant> StringVariantMap;
    mutable StringVariantMap m_cache;
};

}

#endif
