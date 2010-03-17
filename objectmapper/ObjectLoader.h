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
        
    /**
     * Load to the given object all QObject properties defined in this
     * object mapper's store for the given object URI.
     */
    void loadProperties(QObject *o, Uri uri);

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

    QObject *loadFrom(NodeObjectMap &map, Node sourceNode);

    //!!! new method... do we want to distinguish between load-create
    //!!! and reload?
    // NB this will also delete objects if they are no longer referred
    // to in store
    void reload(Nodes nodes, NodeObjectMap &map);
    
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
