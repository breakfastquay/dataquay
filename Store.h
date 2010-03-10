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

#ifndef _DATAQUAY_STORE_H_
#define _DATAQUAY_STORE_H_

#include "Triple.h"

#include <QList>
#include <QHash>
#include <QPair>

namespace Dataquay
{

/// A list of RDF triples.
typedef QList<Triple> Triples;

/// A mapping from key to node, used to list results for a set of result keys.
typedef QHash<QString, Node> Dictionary;

/// A list of Dictionary types, used to contain a sequence of query results.
typedef QList<Dictionary> ResultSet;

enum ChangeType { AddTriple, RemoveTriple };

/// An add or remove operation specified by add/remove token and triple.
typedef QPair<ChangeType, Triple> Change;

/// A sequence of add/remove operations such as may be enacted by a transaction.
typedef QList<Change> ChangeSet;


/**
 * \class Store Store.h <dataquay/Store.h>
 *
 * Abstract interface for Dataquay RDF data stores.
 */
class Store
{
public:
    /**
     * Add a triple to the store.  Prefix expansion is performed on
     * URI nodes in the triple.  Return false if the triple was
     * already in the store.  (Dataquay does not permit duplicate
     * triples in a store.)  Throw RDFException if the triple can not
     * be added for some other reason.
     */
    virtual bool add(Triple t) = 0;
    
    /**
     * Remove a triple from the store.  Prefix expansion is performed
     * on URI nodes in the triple.  If some nodes in the triple are
     * Nothing nodes, remove all matching triples.  Return false if no
     * matching triple was found in the store.  Throw RDFException if
     * removal failed for some other reason.
     */
    virtual bool remove(Triple t) = 0;

    /**
     * Atomically apply the sequence of add/remove changes described
     * in the given ChangeSet.  Throw RDFException if any operation
     * fails for any reason (including duplication etc).
     */
    virtual void change(ChangeSet changes) = 0;
    
    /**
     * Atomically apply the sequence of add/remove changes described
     * in the given ChangeSet, in reverse (ie removing adds and
     * adding removes, in reverse order).  Throw RDFException if any
     * operation fails for any reason (including duplication etc).
     */
    virtual void revert(ChangeSet changes) = 0;
    
    /**
     * Return true if the store contains the given triple, false
     * otherwise.  Prefix expansion is performed on URI nodes in the
     * triple.  Throw RDFException if the triple is not complete or if
     * the test failed for any other reason.
     */
    virtual bool contains(Triple t) const = 0;
    
    /**
     * Return all triples matching the given wildcard triple.  A node
     * of type Nothing in any part of the triple matches any node in
     * the data store.  Prefix expansion is performed on URI nodes in
     * the triple.  Return an empty list if there are no matches; may
     * throw RDFException if matching fails in some other way.
     */
    virtual Triples match(Triple t) const = 0;

    /**
     * Run a SPARQL query against the store and return its results.
     * Any prefixes added previously using addQueryPrefix will be
     * available in this query without needing to be declared in the
     * SPARQL given here (equivalent to writing "PREFIX prefix: <uri>"
     * for each prefix,uri pair set with addPrefix).
     *
     * May throw RDFException.
     *
     * Note that the RDF store must have an absolute base URI (rather
     * than the default "#") in order to perform queries, as relative
     * URIs in the query will be interpreted relative to the query
     * base rather than the store and without a proper base URI there
     * is no way to override that internally.
     */
    virtual ResultSet query(QString sparql) const = 0;

    /**
     * Return the first triple to match the given wildcard triple.  A
     * node of type Nothing in any part of the triple matches any node
     * in the data store.  Prefix expansion is performed on URI nodes
     * in the triple.  Return an empty triple (three Nothing nodes) if
     * there are no matches.  May throw RDFException.
     */
    virtual Triple matchFirst(Triple t) const = 0;

    /**
     * Run a SPARQL query against the store and return the node of
     * the first result for the given query binding.  This is a
     * shorthand for use with queries that are only expected to have
     * one result.  May throw RDFException.
     */
    virtual Node queryFirst(QString sparql, QString bindingName) const = 0;
    
    /**
     * Get a new URI, starting with the given prefix (e.g. ":event_"),
     * that does not currently exist within this store.  The URI will
     * be prefix expanded.
     */
    virtual Uri getUniqueUri(QString prefix) const = 0;

    /**
     * Create and return a new blank node.  This node can only be
     * referred to using the given Node object, and only during its
     * lifetime within this instance of the store.
     */
    virtual Node addBlankNode() = 0;

    /**
     * Expand the given URI (which may use local namespaces) and
     * prefix-expand it, returning the result as a Uri.  (The Uri
     * class is not suitable for storing URIs that use namespaces,
     * particularly local ones.)
     */
    virtual Uri expand(QString uri) const = 0;

protected:
    virtual ~Store() { }
};

}

#endif

