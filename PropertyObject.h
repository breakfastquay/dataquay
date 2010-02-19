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

#ifndef _DATAQUAY_PROPERTY_OBJECT_H_
#define _DATAQUAY_PROPERTY_OBJECT_H_

#include <QString>
#include <QStringList>
#include <QUrl>
#include <QHash>
#include <QVariant>
#include <QVariantList>

#include "Node.h"

namespace Dataquay
{

class Transaction;
class Store;

/**
 * \class PropertyObject PropertyObject.h <dataquay/PropertyObject.h>
 *
 * Helper class for managing properties of an object URI -- that is,
 * triples that share a common subject and possibly a common prefix
 * for the predicate, and that are unique (having a one-to-one
 * relationship between (subject,predicate) and object).  This class
 * provides set and get methods that act directly upon the backing
 * datastore, with (optional but recommended) transaction support.
 * See CacheingPropertyObject for a cacheing alternative.
 *
 * PropertyObject is constructed using a "property prefix" (a string)
 * and "my URI" (a URI).  The URI is used by the property object as
 * the subject for all RDF triples.
 * 
 * All the property handling methods then also take a "property name",
 * which is a string.  If this name contains no ':' character, it will
 * be prefixed with the property prefix that was supplied to the
 * PropertyObject constructor before being subjected to prefix
 * expansion in the RDF store.  The result is then used as the
 * predicate for the RDF triple.  If the name does contain a ':', it
 * is passed for expansion directly (the prefix is not prepended
 * first).  As an exception, if the prefix is the special name "a",
 * it will be expanded (by the store) as "rdf:type".
 *
 * Example: If the property prefix is "myprops:" and the property name
 * passed to getProperty is "some_property", the returned value from
 * getProperty will be the result of matching on the triple (myUri,
 * "myprops:some_property", ()).  Hopefully, the RDF store will have
 * already been told about the "myprops" prefix and will know how to
 * expand it.
 *
 * Example: If the property prefix is "http://example.com/property/"
 * and the property name passed to getProperty is "some_property", the
 * returned value from getProperty will be the result of matching on
 * the triple (myUri, "http://example.com/property/some_property", ()).
 *
 * Example: If the property prefix is "myprops:" and the property name
 * passed to getProperty is "yourprops:some_property", the returned
 * value from getProperty will be the result of matching on the triple
 * (myUri, "yourprops:some_property", ()).  The property prefix is not
 * used at all in this example because the property name contains ':'.
 */
class PropertyObject
{
public:
    typedef QHash<QString, Nodes> Properties;

    /**
     * Construct a PropertyObject acting on the given Store, with the
     * default prefix for properties taken from the global default
     * (see setDefaultPropertyPrefix) and the given "subject" URI.
     */
    PropertyObject(Store *s, QUrl myUri);

    /**
     * Construct a PropertyObject acting on the given Store, with the
     * default prefix for properties taken from the global default
     * (see setDefaultPropertyPrefix) and the given "subject" URI
     * (which will be prefix expanded).
     */
    PropertyObject(Store *s, QString myUri);

    /**
     * Construct a PropertyObject acting on the given Store, with the
     * given default prefix for properties and the given "subject"
     * URI.
     */
    PropertyObject(Store *s, QString propertyPrefix, QUrl myUri);

    /**
     * Construct a PropertyObject acting on the given Store, with the
     * given default prefix for properties and the given "subject"
     * URI (which will be prefix expanded).
     */
    PropertyObject(Store *s, QString propertyPrefix, QString myUri);

    /**
     * Construct a PropertyObject acting on the given Store, with the
     * given default prefix for properties and the given node as its
     * subject.  This is provided so as to permit querying the
     * properties of blank nodes.
     */
    PropertyObject(Store *s, QString propertyPrefix, Node myUri);

    ~PropertyObject();

    /**
     * Return the rdf:type of my URI, if any.  If more than one is
     * defined, return the first one found.
     */
    QUrl getObjectType() const;

    /**
     * Return the rdf:type of my URI, if any, querying through the
     * given transaction.  If more than one is defined, return the
     * first one found.
     */
    QUrl getObjectType(Transaction *tx) const;

    /**
     * Return true if the property object has the given property.  That
     * is, if the store contains at least one triple whose subject and
     * predicate match those for my URI and the expansion of the given
     * property name.
     */
    bool hasProperty(QString name) const;

    /**
     * Return true if the property object has the given property,
     * querying through the given transaction.  That is, if the store
     * contains at least one triple whose subject and predicate match
     * those for my URI and the expansion of the given property name.
     */
    bool hasProperty(Transaction *tx, QString name) const;

    /**
     * Get the value of the given property.  That is, if the store
     * contains at least one triple whose subject and predicate match
     * those for my URI and the expansion of the given property name,
     * convert the object part of the first such matching triple to a
     * QVariant via Node::toVariant and return that value.  If there
     * is no such match, return QVariant().
     */
    QVariant getProperty(QString name) const;

    /**
     * Get the value of the given property, querying through the given
     * transaction.  That is, if the store contains at least one
     * triple whose subject and predicate match those for my URI and
     * the expansion of the given property name, convert the object
     * part of the first such matching triple to a QVariant via
     * Node::toVariant and return that value.  If there is no such
     * match, return QVariant().
     */
    QVariant getProperty(Transaction *tx, QString name) const;

    /**
     * Get the value of the given property as a list.  That is, if the
     * store contains at least one triple whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, convert the object parts of all such matching triples to
     * QVariant via Node::toVariant and return a list of the resulting
     * values.  If there is no such match, return an empty list.
     *
     * Note that the order of variants in the returned list is
     * arbitrary and may change from one call to the next.
     */
    QVariantList getPropertyList(QString name) const;

    /**
     * Get the value of the given property as a list, querying through
     * the given transaction.  That is, if the store contains at least
     * one triple whose subject and predicate match those for my URI
     * and the expansion of the given property name, convert the
     * object parts of all such matching triples to QVariant via
     * Node::toVariant and return a list of the resulting values.  If
     * there is no such match, return QVariant().
     *
     * Note that the order of variants in the returned list is
     * arbitrary and may change from one call to the next.
     */
    QVariantList getPropertyList(Transaction *tx, QString name) const;

    /**
     * Get the node for the given property.  That is, if the store
     * contains at least one triple whose subject and predicate match
     * those for my URI and the expansion of the given property name,
     * return the object part of the first such matching triple.  If
     * there is no such match, return Node().
     */
    Node getPropertyNode(QString name) const;
    
    //!!! shall we get rid of the Transaction versions of these? after all, Transaction is a Store and we construct with a Store... though it's nice to be able to keep these hanging around persistently
    Node getPropertyNode(Transaction *tx, QString name) const;

    /**
     * Get the nodes for the given property.  That is, if the store
     * contains at least one triple whose subject and predicate match
     * those for my URI and the expansion of the given property name,
     * return the object parts of all such matching triples.  If there
     * is no such match, return an empty list.
     *
     * Note that the order of nodes in the returned list is arbitrary
     * and may change from one call to the next.
     */
    Nodes getPropertyNodeList(QString name) const;

    //!!!doc
    Nodes getPropertyNodeList(Transaction *tx, QString name) const;

    /**
     * Get the names of this object's properties.  That is, find all
     * triples in the store whose subject matches my URI and whose
     * predicate begins with our property prefix, and return a list of
     * the remainder of their predicate URIs following the property
     * prefix.
     */
    QStringList getPropertyNames() const;

    /**
     * Get the names of this object's properties, querying through the
     * given transaction.  That is, find all triples in the store
     * whose subject matches my URI and whose predicate begins with
     * our property prefix, and return a list of the remainder of
     * their predicate URIs following the property prefix.
     */
    QStringList getPropertyNames(Transaction *tx) const;

    /**
     * Get all property names and nodes.
     */
    Properties getAllProperties() const;

    /**
     * Get all property names and nodes.
     */
    Properties getAllProperties(Transaction *tx) const;

    /**
     * Set the given property to the given value.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple whose object part is the result
     * of converting the given variant to a node via
     * Node::fromVariant.
     */
    void setProperty(QString name, QVariant value);

    /**
     * Set the given property to the given URI.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple whose object part is the URI.
     */
    void setProperty(QString name, QUrl uri);

    /**
     * Set the given property to the given node.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple whose object part is the node.
     */
    void setProperty(QString name, Node node);

    /**
     * Set the given property to the given value through the given
     * transaction.  That is, first remove from the store any triples
     * whose subject and predicate match those for my URI and the
     * expansion of the given property name, then insert a new triple
     * whose object part is the result of converting the given variant
     * to a node via Node::fromVariant.
     */
    void setProperty(Transaction *tx, QString name, QVariant value);

    /**
     * Set the given property to the given URI through the given
     * transaction.  That is, first remove from the store any triples
     * whose subject and predicate match those for my URI and the
     * expansion of the given property name, then insert a new triple
     * whose object part is the URI.
     */
    void setProperty(Transaction *tx, QString name, QUrl uri);

    /**
     * Set the given property to the given node through the given
     * transaction.  That is, first remove from the store any triples
     * whose subject and predicate match those for my URI and the
     * expansion of the given property name, then insert a new triple
     * whose object part is the node.
     */
    void setProperty(Transaction *tx, QString name, Node node);

    /**
     * Set the given property to the given values.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple for each variant in the value
     * list, whose object part is the result of converting that
     * variant to a node via Node::fromVariant.
     */
    void setPropertyList(QString name, QVariantList values);

    /**
     * Set the given property to the given values through the given
     * transaction.  That is, first remove from the store any triples
     * whose subject and predicate match those for my URI and the
     * expansion of the given property name, then insert a new triple
     * for each variant in the value list, whose object part is the
     * result of converting that variant to a node via
     * Node::fromVariant.
     */
    void setPropertyList(Transaction *tx, QString name, QVariantList values);

    /**
     * Set the given property to the given nodes.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple for each node in the list, whose
     * object part is that node.
     */
    void setPropertyList(QString name, Nodes nodes);

    /**
     * Set the given property to the given nodes through the given
     * transaction.  That is, first remove from the store any triples
     * whose subject and predicate match those for my URI and the
     * expansion of the given property name, then insert a new triple
     * for each node in the list, whose object part is that node.
     */
    void setPropertyList(Transaction *tx, QString name, Nodes nodes);

    /**
     * Remove the given property.  That is, remove from the store any
     * triples whose subject and predicate match those for my URI and
     * the expansion of the given property name.
     */
    void removeProperty(QString name);

    /**
     * Remove the given property.  That is, remove from the store any
     * triples whose subject and predicate match those for my URI and
     * the expansion of the given property name.
     */
    void removeProperty(Transaction *tx, QString name);

    /**
     * Return the Store object that will be used for modifications in
     * the given transaction.  If the transaction is not
     * NoTransaction, then the returned Store will simply be the
     * transaction itself; otherwise it will be the store that was
     * passed to the constructor.
     */
    Store *getStore(Transaction *tx) const;

    /**
     * Return the URI used for the "predicate" part of any triple
     * referring to the given property name.  See the general
     * PropertyObject documentation for details of how these names are
     * expanded.
     */
    QUrl getPropertyUri(QString name) const;

    /**
     * Set the global default property prefix.  This will be used as
     * the prefix for all PropertyObjects subsequently constructed
     * using the two-argument (prefixless) constructors.
     */
    static void setDefaultPropertyPrefix(QString prefix);
    
private:
    Store *m_store;
    QString m_pfx;
    QUrl m_upfx;
    Node m_node;
    static QString m_defaultPrefix;
};

/**
 * \class CacheingPropertyObject CacheingPropertyObject.h <dataquay/PropertyObject.h>
 *
 * Helper class for managing properties of an object URI -- that is,
 * triples that share a common subject and possibly a common prefix
 * for the predicate -- with a demand-populated cache.  This class is
 * usually faster than PropertyObject as it avoids datastore access,
 * but it can only be used in contexts where it is known that no other
 * agent may be modifying the same set of properties.
 
 *!!! needs to be updated with newer PropertyObject functions, and its
      cache is not very good (e.g. hasProperty followed by getProperty
      is slow)

 */
class CacheingPropertyObject
{
public:
    CacheingPropertyObject(Store *s, QUrl myUri);
    CacheingPropertyObject(Store *s, QString myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, QUrl myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, QString myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, Node myUri);

    QUrl getObjectType() const;

    bool hasProperty(QString name) const;

    QVariant getProperty(QString name) const;
    QVariantList getPropertyList(QString name) const;
    Node getPropertyNode(QString name) const;
    Nodes getPropertyNodeList(QString name) const;
    QStringList getPropertyNames() const;
    PropertyObject::Properties getAllProperties() const;

    void setProperty(QString name, QVariant value);
    void setProperty(QString name, QUrl value);
    void setProperty(QString name, Node node);
    void setPropertyList(QString name, QVariantList values);
    void setPropertyList(QString name, Nodes nodes);

    //!!!???
    void setProperty(Transaction *tx, QString name, QVariant value);

    void removeProperty(QString name);

    Store *getStore(Transaction *tx) const;
    QUrl getPropertyUri(QString name) const;

private:
    PropertyObject m_po;
    mutable QUrl m_type;
    mutable PropertyObject::Properties m_cache; // note: value is never empty
    mutable bool m_cached;
    void encache() const;
    void encacheType() const;
};

}

#endif
