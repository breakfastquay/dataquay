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

#ifndef _DATAQUAY_OBJECT_MAPPER_H_
#define _DATAQUAY_OBJECT_MAPPER_H_

#include <QObject>

#include "../Node.h"
#include "../Store.h"

namespace Dataquay
{

class TransactionalStore;
class TypeMapping;

/**
 * \class ObjectMapper ObjectMapper.h <dataquay/objectmapper/ObjectMapper.h>
 *
 * ObjectMapper manages a set of objects to maintain a consistent
 * record of their state in an RDF store.  It uses ObjectStorer to map
 * objects (derived from QObject) to the store, and then watches both
 * the objects and the store for changes, applying to the store any
 * changes in the objects and using ObjectLoader to bring the objects
 * up to date with any changes in the store.
 *
 * See ObjectStorer for details of how objects are mapped to the RDF
 * store, and ObjectLoader for details of how changes in the RDF store
 * are mapped back to the objects.
 *
 * ObjectMapper watches QObject properties' notify signals to
 * determine when a property has changed, and uses QObject::destroyed
 * to determine when an object has been deleted.  You can also advise
 * it of changes using the objectModified slot directly (for example
 * where a property has no notify signal).
 *
 * ObjectMapper requires a TransactionalStore as its backing RDF
 * store, and uses the TransactionalStore's transactionCommitted
 * signal to tell it when a change has been made to the store which
 * should be mapped back to the object structure.
 *  
 * You must call the commit() method to cause any changes to be
 * written to the store.  This also commits the underlying
 * transaction.
 *
 * Call add() to add a new object to the store (managing it, and also
 * marking it to be stored on the next commit).  ObjectMapper does not
 * have any other way to find out about new objects, even if they are
 * properties or children of existing managed objects.
 *
 * Call manage() to manage an object without marking it as needing to
 * be written -- implying that the object is known to be up-to-date
 * with the store already.  ObjectMapper will refuse to manage any
 * object that lacks a uri property, as any objects that have not
 * previously been mapped will normally need to be add()ed rather than
 * manage()d.  Call unmanage() to tell ObjectMapper to stop watching
 * an object.
 *
 * ObjectMapper is thread-safe.
 */
class ObjectMapper : public QObject
{
    Q_OBJECT

public:
    /**
     * Construct an object mapper backed by the given store.  The
     * mapper is initially managing no objects.
     *
     * The store must be a TransactionalStore (rather than for example
     * a BasicStore) because the object mapper commits each update as
     * a single transaction and relies on the
     * TransactionalStore::transactionCommitted signal to learn about
     * changes in the store.
     */
    ObjectMapper(TransactionalStore *ts);
    ~ObjectMapper();

    /**
     * Obtain the TransactionalStore that was passed to the
     * constructor.
     */
    TransactionalStore *getStore();

    /**
     * Supply a TypeMapping object describing the RDF URIs that should
     * be used to encode each object's property and class names.
     * Without this, ObjectMapper (or rather its ObjectStorer and
     * ObjectLoader classes) will generate suitable-looking URIs for
     * each class and property names.
     */
    void setTypeMapping(const TypeMapping &);

    /**
     * Obtain the TypeMapping previously set using setTypeMapping, or
     * the default (empty) TypeMapping if none has been set.
     */
    const TypeMapping &getTypeMapping() const;

    /**
     * Obtain the RDF node to which the given object has been mapped,
     * or a null Node if the object has not yet been stored by this
     * ObjectMapper.
     */
    Node getNodeForObject(QObject *o) const;

    /**
     * Obtain the QObject which has been mapped to the given node, or
     * NULL if the node is not one that has been stored by this
     * ObjectMapper.
     */
    QObject *getObjectByNode(Node n) const;

signals:
    void committed();

public slots:
    void add(QObject *);
    void add(QObjectList);

    void manage(QObject *);
    void manage(QObjectList);

    void unmanage(QObject *);
    void unmanage(QObjectList);

    void commit();

    void objectModified(QObject *);
    void objectDestroyed(QObject *);

private slots:
    void transactionCommitted(const ChangeSet &cs);

private:
    ObjectMapper(const ObjectMapper &);
    ObjectMapper &operator=(const ObjectMapper &);

    class D;
    D *m_d;
};

}

#endif
