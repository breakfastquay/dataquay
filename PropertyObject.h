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
#include <QUrl>
#include <map>

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
 * Example: If the property prefix is "myprops" and the property name
 * passed to getProperty is "some_property", the returned value from
 * getProperty will be the result of matching on the triple (myUri,
 * "myprops:some_property", ()).  Hopefully, the RDF store will have
 * already been told about the "myprops" prefix and will know how to
 * expand it.
 *
 * Example: If the property prefix is "myprops" and the property name
 * passed to getProperty is "yourprops:some_property", the returned
 * value from getProperty will be the result of matching on the triple
 * (myUri, "yourprops:some_property", ()).  The property prefix is not
 * used at all in this example because the property name contains ':'.
 */
class PropertyObject
{
public:
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
     * Set the given property to the given value.  That is, first
     * remove from the store any triples whose subject and predicate
     * match those for my URI and the expansion of the given property
     * name, then insert a new triple whose object part is the result
     * of converting the given variant to a node via
     * Node::fromVariant.
     */
    void setProperty(Transaction *tx, QString name, QVariant value);

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
    QUrl m_uri;
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
 */
class CacheingPropertyObject
{
public:
    CacheingPropertyObject(Store *s, QUrl myUri);
    CacheingPropertyObject(Store *s, QString myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, QUrl myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, QString myUri);

    bool hasProperty(QString name) const;
    bool hasProperty(Transaction *tx, QString name) const;
    QVariant getProperty(QString name) const;
    QVariant getProperty(Transaction *tx, QString name) const;
    void setProperty(Transaction *tx, QString name, QVariant value);
    void removeProperty(Transaction *tx, QString name);

    Store *getStore(Transaction *tx) const;
    QUrl getPropertyUri(QString name) const;

private:
    PropertyObject m_po;
    typedef std::map<QString, QVariant> StringVariantMap;
    mutable StringVariantMap m_cache;
};

}

#endif
