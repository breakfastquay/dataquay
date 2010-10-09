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

#ifndef _DATAQUAY_OBJECT_LOADER_H_
#define _DATAQUAY_OBJECT_LOADER_H_

#include "../Node.h"

#include <QHash>

class QObject;

namespace Dataquay
{

class Store;
class TypeMapping;

/**
 * \class ObjectLoader ObjectLoader.h <dataquay/objectmapper/ObjectLoader.h>
 *
 * ObjectLoader can create and refresh objects based on the types and
 * relationships set out in a Store.  
 *!!!
 *
 * ObjectLoader is re-entrant, but not thread-safe.
 */
 
class ObjectLoader
{
public:
    /// Map from RDF node to object
    typedef QHash<Node, QObject *> NodeObjectMap;

    /**
     * Create an ObjectLoader ready to load objects from the given RDF
     * store.
     */
    ObjectLoader(Store *s);
    ~ObjectLoader();

    Store *getStore();

    void setTypeMapping(const TypeMapping &);
    const TypeMapping &getTypeMapping() const;
    
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

    enum AbsentPropertyPolicy {
        IgnoreAbsentProperties,
        ResetAbsentProperties
    };

    void setAbsentPropertyPolicy(AbsentPropertyPolicy policy);
    AbsentPropertyPolicy getAbsentPropertyPolicy() const;

    /**
     * Construct a QObject based on the properties of the given object
     * URI in the object mapper's store.  The type of class created
     * will be determined by the rdf:type for the URI.
     *!!!??? type prefix? how these map to class names?
     */


    QObject *load(Node node); //!!! n.b. could be very expensive if follow options are set! might load lots & lots of stuff and then leak it! ... also, want to specify parent as in old loadObject, and to have uri arg... old method was better here

    /**
     * For each node of the given RDF type found in the store,
     * construct a corresponding QObject, returning the objects.
     */
    QObjectList loadType(Uri type);

    /**
     * For each node of the given RDF type found in the store,
     * construct a corresponding QObject, updating the map with all
     * resulting node-object correspondences and returning the
     * objects.
     */
    QObjectList loadType(Uri type, NodeObjectMap &map);
    
    /**
     * Examine each of the nodes passed in, and if there is no
     * corresponding node in the node-object map, load the node as a
     * new QObject and place it in the map; if there is a
     * corresponding node in the node-object map, update it with
     * current properties from the store.  If a node is passed in that
     * does not exist in the store, delete any object associated with
     * it from the map.
     */
    void reload(Nodes nodes, NodeObjectMap &map);

    //!!! currently this loads all objects in store -- that is not the same thing as reloading all objects whose nodes are in the map
    QObjectList loadAll();
    QObjectList loadAll(NodeObjectMap &map);
    
    struct LoadCallback {
        virtual void loaded(ObjectLoader *, NodeObjectMap &, Node, QObject *) = 0;
    };

    void addLoadCallback(LoadCallback *callback);

private:
    ObjectLoader(const ObjectLoader &);
    ObjectLoader &operator=(const ObjectLoader &);
    class D;
    D *m_d;
};

}

#endif
