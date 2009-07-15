/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_PROPERTY_OBJECT_H_
#define _RDF_PROPERTY_OBJECT_H_

#include "Transaction.h"
#include "RDFException.h"

#include <map>

namespace Dataquay
{

//!!! pull out implementations into cpp file for clarity

class PropertyObject
{
public:
    PropertyObject(Store *s, QUrl uri);
    PropertyObject(Store *s, QString uri);

    bool hasProperty(Transaction *tx, QString name) const;
    QVariant getProperty(Transaction *tx, QString name) const;
    void setProperty(Transaction *tx, QString name, QVariant value);
    void removeProperty(Transaction *tx, QString name);

    Store *getStore(Transaction *tx) const;
    QUrl getPropertyUri(QString name) const;
    
private:
    Store *m_store;
    QUrl m_uri;
};

class CacheingPropertyObject
{
public:
    CacheingPropertyObject(Store *s, QUrl uri);
    CacheingPropertyObject(Store *s, QString uri);

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
