/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2011 Chris Cannam.
  
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
     * will be calculated from the rdf:type for the URI, using the
     * current TypeMapping (see TypeMapping::getClassForTypeUri).
     *
     * Use caution in calling this method when the FollowPolicy is set
     * to anything other than FollowNone.  Other objects may be loaded
     * when following connections from the given node according to the
     * current FollowPolicy, but only the object initially requested
     * is actually returned from the function -- other objects loaded
     * may be only accessible as parent/child or properties of this
     * node, or in some cases (e.g. FollowSiblings) may even be
     * inaccessible to the caller and be leaked.
     */
    QObject *load(Node node);

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

    /**
     * Load and return an object for each node in the store that can
     * be loaded.
     */
    QObjectList loadAll();

    /**
     * Load and return an object for each node in the store that can
     * be loaded, updating the map with all resulting node-object
     * correspondences.  Note that this loads all suitably-typed nodes
     * found in the store, not the objects found in the map.  If there
     * are nodes in the map which are not found in the store, they
     * will be ignored (and not deleted from the map).
     */
    QObjectList loadAll(NodeObjectMap &map);
    
    struct LoadCallback {
        /**
         * An object has been loaded by the given ObjectLoader from
         * the given RDF node.  The node and object will also be found
         * in the NodeObjectMap, which additionally references any
         * other objects which have been loaded during this load
         * sequence.
         */
        virtual void loaded(ObjectLoader *, NodeObjectMap &, Node, QObject *) = 0;
    };

    /**
     * Register the given callback (a subclass of the abstract
     * LoadCallback class) as providing a "loaded" callback method
     * which will be called after each object is loaded.
     */
    void addLoadCallback(LoadCallback *callback);

private:
    ObjectLoader(const ObjectLoader &);
    ObjectLoader &operator=(const ObjectLoader &);
    class D;
    D *m_d;
};

}

#endif
