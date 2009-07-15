/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_PROPERTY_OBJECT_H_
#define _RDF_PROPERTY_OBJECT_H_

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
 * for the predicate.  This class provides set and get methods that
 * act directly upon the backing datastore, with (optional but
 * recommended) transaction support.  See CacheingPropertyObject for a
 * cacheing alternative.
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
    PropertyObject(Store *s, QString propertyPrefix, QUrl myUri);
    PropertyObject(Store *s, QString propertyPrefix, QString myUri);

    bool hasProperty(Transaction *tx, QString name) const;
    QVariant getProperty(Transaction *tx, QString name) const;
    void setProperty(Transaction *tx, QString name, QVariant value);
    void removeProperty(Transaction *tx, QString name);

    Store *getStore(Transaction *tx) const;
    QUrl getPropertyUri(QString name) const;
    
private:
    Store *m_store;
    QString m_pfx;
    QUrl m_uri;
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
    CacheingPropertyObject(Store *s, QString propertyPrefix, QUrl myUri);
    CacheingPropertyObject(Store *s, QString propertyPrefix, QString myUri);

    bool hasProperty(Transaction *tx, QString name) const;
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
