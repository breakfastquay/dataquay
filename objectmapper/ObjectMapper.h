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

#include "../Uri.h"
#include "../Node.h"

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

/**
 * \class ObjectMapper ObjectMapper.h <dataquay/ObjectMapper.h>
 *
 * A class which maps QObject trees to RDF data in a Store.  RDF
 * properties are mapped using QObject properties, and QObject
 * parent/child and ordering relationships are recorded using
 * dedicated RDF properties in the Dataquay namespace, or with a
 * specialised RDF property mapping.
 *
 * ObjectMapper can by used to hibernate and reanimate active QObject
 * trees to a datastore, or to construct QObjects to represent
 * arbitrary RDF data using appropriate object-type and property-type
 * mappings.
 */
class ObjectMapper
{
public:
    /// Map from RDF node to object
    typedef QMap<Node, QObject *> NodeObjectMap;

    /// Map from object to RDF node
    typedef QMap<QObject *, Node> ObjectNodeMap;

    /**
     * Create an ObjectMapper ready to load and store objects from and
     * to the given RDF store.
     */
    ObjectMapper(Store *s);
    ~ObjectMapper();

    Store *getStore();

    //... document
    Uri getObjectTypePrefix() const;
    void setObjectTypePrefix(Uri prefix);

    Uri getPropertyPrefix() const;
    void setPropertyPrefix(Uri prefix);

    Uri getRelationshipPrefix() const;
    void setRelationshipPrefix(Uri prefix);

    void addTypeMapping(QString className, QString uri);
    void addTypeMapping(QString className, Uri uri);

    /**
     * Add a mapping between class name and the common parts of any
     * URIs that are automatically generated when storing instances of
     * that class that have no URI property defined.
     * 
     * For example, a mapping from "MyNamespace::MyClass" to
     * "http://mydomain.com/resource/" would ensure that automatically
     * generated "unique" URIs for instances that class all started
     * with that URI prefix.  Note that the prefix itself is also
     * subject to namespace prefix expansion when stored.
     *
     * (If no prefix mapping was given for this example, its generated
     * URIs would start with ":mynamespace_myclass_".)
     *
     * Generated URIs are only checked for uniqueness within the store
     * being exported to and cannot be guaranteed to be globally
     * unique.  For this reason, caution should be exercised in the
     * use of this function.
     *
     * Note that in principle the object mapper could use this when
     * loading, to infer class types for URIs in the store that have
     * no rdf:type.  In practice that does not happen -- the object
     * mapper will not generate a class for URIs without rdf:type.
     */
    void addTypeUriPrefixMapping(QString className, QString prefix);
    void addTypeUriPrefixMapping(QString className, Uri prefix);

    //!!! n.b. document that uris must be distinct (can't map to properties to same RDF predicate as we'd be unable to distinguish between them on reload -- we don't use the object type to distinguish which predicate is which)
    void addPropertyMapping(QString className, QString propertyName, QString uri);
    void addPropertyMapping(QString className, QString propertyName, Uri uri);
    
    enum PropertyStorePolicy {
        /** Store only properties that differ from default object */
        StoreIfChanged,
        /** Store all properties (if storable, readable & writable) (default) */
        StoreAlways
    };

    void setPropertyStorePolicy(PropertyStorePolicy policy);
    PropertyStorePolicy getPropertyStorePolicy() const;

    enum ObjectStorePolicy {
        /** Store only objects with non-empty "uri" properties */
        StoreObjectsWithURIs,
        /** Store all objects, giving them URIs as needed (default) */
        StoreAllObjects
    };

    void setObjectStorePolicy(ObjectStorePolicy policy);
    ObjectStorePolicy getObjectStorePolicy() const;

    enum BlankNodePolicy {
        /** Ensure every stored object has a URI */
        NoBlankNodes,
        /** Use blank nodes for objects with no existing URIs that are
            not known to be referred to elsewhere (default) */
        BlankNodesAsNeeded
    };

    void setBlankNodePolicy(BlankNodePolicy policy);
    BlankNodePolicy getBlankNodePolicy() const;

    /**
     * Load to the given object all QObject properties defined in this
     * object mapper's store for the given object URI.
     */
    void loadProperties(QObject *o, Uri uri);

    //!!! transaction/connection management?

    //!!! what to do about updating objects that are already stored? want to minimise changes, esp. if using a transaction

    /**
     * Save to the object mapper's RDF store all properties for the
     * given object, as QObject property types associated with the
     * given object URI.
     */
    void storeProperties(QObject *o, Uri uri);

    //!!! want loadObject, loadTree, loadGraph, loadAllGraphs (loadGraph equivalent to loadFrom) -- and then loader callback gets passed an enum value identifying load type
    
    /**
     * Construct a QObject based on the properties of the given object
     * URI in the object mapper's store.  The type of class created
     * will be determined by the rdf:type for the URI.
     *!!!??? type prefix? how these map to class names?
     */
    QObject *loadObject(Uri uri, QObject *parent); // may throw ConstructionFailedException, UnknownTypeException
    QObject *loadObjectTree(Uri rootUri, QObject *parent); // may throw ConstructionFailedException
    QObject *loadAllObjects(QObject *parent); // may throw ConstructionFailedException

    Uri storeObject(QObject *o);
    Uri storeObjectTree(QObject *root);
    void storeAllObjects(QObjectList);
    //!!! want a method to store all objects (and pass back and forth object map?)

    QObject *loadFrom(NodeObjectMap &map, Node sourceNode);
    Node store(ObjectNodeMap &map, QObject *o);
    
    struct LoadCallback {
        virtual void loaded(ObjectMapper *, NodeObjectMap &, Node, QObject *) = 0;
    };
    struct StoreCallback {
        virtual void stored(ObjectMapper *, ObjectNodeMap &, QObject *, Node) = 0;
    };

    void addLoadCallback(LoadCallback *callback);
    void addStoreCallback(StoreCallback *callback);


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

private:
    class D;
    D *m_d;
};

}

#endif
