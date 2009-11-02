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

#ifndef _DATAQUAY_OBJECT_MAPPER_H_
#define _DATAQUAY_OBJECT_MAPPER_H_

#include <QUrl>

#include <exception>

class QObject;

namespace Dataquay
{

class Store;

/*
 * strategies for mapping qobject trees to datastore:
 *
 * 1. our custom qobjects are themselves written to use the rdf store
 *    through e.g. propertyobject -- use ObjectMapper to reanimate the
 *    tree when loading document, but don't need to use it thereafter?
 *
 * 2. custom qobjects emit notify signals when properties change (or
 *    e.g. children added), ObjectMapper receives those signals and
 *    re-writes the properties to database
 *
 * 3. qobjects store the data themselves, we just load the objects and
 *    then write them all back en masse on request.
 *
 * How to manage transactions in each case? And commands?
 */

class ObjectMapper
{
public:
    typedef QHash<QString, QObject *> UriObjectMap;
    typedef QMap<QObject *, QUrl> ObjectUriMap;

    /**
     * Create a ObjectMapper ready to load and store objects from and
     * to the given RDF store.
     */
    ObjectMapper(Store *s);
    ~ObjectMapper();

    class UnknownTypeException : virtual public std::exception {
    public:
        UnknownTypeException(QString type) throw() : m_type(type) { }
        virtual ~UnknownTypeException() throw() { }
        virtual const char *what() const throw() {
            return QString("Unknown type: %1").arg(m_type).toLocal8Bit().data();
        }
    protected:
        QString m_type;
    };

    class ConstructionFailedException : virtual public std::exception {
    public:
        ConstructionFailedException(QString type) throw() : m_type(type) { }
        virtual ~ConstructionFailedException() throw() { }
        virtual const char *what() const throw() {
            return QString("Failed to construct type: %1")
                .arg(m_type).toLocal8Bit().data();
        }
    protected:
        QString m_type;
    };

    enum PropertyStorePolicy {
        StoreIfChanged, /// Store only properties that differ from default object
        StoreAlways     /// Store all properties (if storable, readable & writable)
    };

    void setPropertyStorePolicy(PropertyStorePolicy policy);
    PropertyStorePolicy getPropertyStorePolicy() const;

    enum ObjectStorePolicy {
        StoreObjectsWithURIs, /// Store only objects with non-empty "uri" properties
        StoreAllObjects       /// Store all objects, giving them URIs as needed
    };

    void setObjectStorePolicy(ObjectStorePolicy policy);
    ObjectStorePolicy getObjectStorePolicy() const;

    /**
     * Load to the given object all QObject properties defined in this
     * object mapper's store for the given object URI.
     */
    void loadProperties(QObject *o, QUrl uri);

    //!!! transaction/connection management?

    /**
     * Save to the object mapper's RDF store all properties for the
     * given object, as QObject property types associated with the
     * given object URI.
     */
    void storeProperties(QObject *o, QUrl uri);

    /**
     * Construct a QObject based on the properties of the given object
     * URI in the object mapper's store.  The type of class created
     * will be determined by the rdf:type for the URI.
     *!!!??? type prefix? how these map to class names?
     */
    QObject *loadObject(QUrl uri, QObject *parent);
    QObject *loadObjects(QUrl rootUri, QObject *parent);
    QObject *loadAllObjects(QObject *parent);

    QUrl storeObject(QObject *o);
    QUrl storeObjects(QObject *root);
    
    struct LoadCallback {
        virtual void loaded(Store *, UriObjectMap &, QUrl, QObject *) = 0;
    };
    void addLoadCallback(LoadCallback *callback);

    struct StoreCallback {
        virtual void stored(Store *, ObjectUriMap &, QObject *, QUrl) = 0;
    };
    void addStoreCallback(StoreCallback *callback);

private:
    class D;
    D *m_d;
};

}

#endif
