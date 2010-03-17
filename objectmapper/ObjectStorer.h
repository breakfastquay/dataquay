/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2010 Chris Cannam.
  
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

#include <QHash>

class QObject;

namespace Dataquay
{

class Store;
class TypeMapping;

class ObjectStorer
{
public:
    /// Map from object to RDF node

    //!!! NB. Want to stress the fact that these maps can't be
    //!!! "persistent" unless you manage object deletion/creation
    //!!! elsewhere -- since they just use a pointer to the QObject
    //!!! (not any actual sort of identity), the QObject could change,
    //!!! be deleted etc while leaving the pointer unchanged

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

    //!!! this superficially appears to have an overlap with the StoreOption argument passed to store()
    enum PropertyStorePolicy {
        /** Store only properties that differ from default object */
        StoreIfChanged,
        /** Store all properties (if storable, readable & writable) (default) */
        StoreAlways
    };

    void setPropertyStorePolicy(PropertyStorePolicy policy);
    PropertyStorePolicy getPropertyStorePolicy() const;

    enum BlankNodePolicy {
        /** Ensure every stored object has a URI */
        NoBlankNodes,
        /** Use blank nodes for objects with no existing URIs that are
            not known to be referred to elsewhere (default) */
        BlankNodesAsNeeded
    };

    void setBlankNodePolicy(BlankNodePolicy policy);
    BlankNodePolicy getBlankNodePolicy() const;

    //!!! do we want this here? maybe not
    void removeObject(Node node); // but not any objects it refers to?

    enum FollowOption {
        FollowNone             = 0, // the default
        FollowObjectProperties = 1,
        FollowParent           = 2,
        FollowSiblings         = 4,
        FollowChildren         = 8
        // there is no FollowAll; it generally isn't a good idea
    };
    typedef int FollowPolicy;

    void setFollowPolicy(FollowPolicy policy);
    FollowPolicy getFollowPolicy() const;
    
    Uri store(QObject *o);
    Uri store(QObject *o, ObjectNodeMap &map);

    void store(QObjectList o);
    void store(QObjectList o, ObjectNodeMap &map);

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
