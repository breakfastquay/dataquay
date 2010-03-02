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

#ifndef _DATAQUAY_OBJECT_STORER_H_
#define _DATAQUAY_OBJECT_STORER_H_

#include "../Node.h"

#include <QMap>

class QObject;

namespace Dataquay
{

class Store;
class TypeMapping;

class ObjectStorer
{
public:
    /// Map from object to RDF node
    typedef QHash<QObject *, Node> ObjectNodeMap;

    /**
     * Create an ObjectStorer ready to store objects to the given RDF
     * store.
     */
    ObjectStorer(Store *s);
    ~ObjectStorer();

    Store *getStore();

    void setTypeMapping(const TypeMapping &);
    const TypeMapping &getTypeMapping() const;
        
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
     * Save to the object mapper's RDF store all properties for the
     * given object, as QObject property types associated with the
     * given object URI.
     */
    void storeProperties(QObject *o, Uri uri);

    Uri storeObject(QObject *o);
    Uri storeObjectTree(QObject *root);
    void storeAllObjects(QObjectList);
    //!!! want a method to store all objects (and pass back and forth object map?)
    Node store(ObjectNodeMap &map, QObject *o);

    struct StoreCallback {
        virtual void stored(ObjectStorer *, ObjectNodeMap &, QObject *, Node) = 0;
    };

    void addStoreCallback(StoreCallback *callback);

private:
    ObjectStorer(const ObjectStorer &);
    ObjectStorer &operator=(const ObjectStorer &);

    class D;
    D *m_d;
};

}

#endif