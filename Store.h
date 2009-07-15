/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_STORE_H_
#define _RDF_STORE_H_

#include "Triple.h"

#include <QList>
#include <QHash>

namespace Dataquay
{
	
typedef QList<Triple> Triples;
typedef QHash<QString, Node> Dictionary;
typedef QList<Dictionary> ResultSet;

enum ChangeType { AddTriple, RemoveTriple };
typedef std::pair<ChangeType, Triple> Change;
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
     * already in the store.  Throw RDFException if the triple can not
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
     * Get a new URI, starting with the given prefix, that does not
     * currently exist within this store.  The URI will be prefix
     * expanded.
     */
    virtual QUrl getUniqueUri(QString prefix) const = 0;

    /**
     * Expand the given URI (which may use local namespaces) and
     * prefix-expand it, returning the result as a QUrl.  (The QUrl
     * class is not suitable for storing URIs that use namespaces,
     * particularly local ones.)
     */
    virtual QUrl expand(QString uri) const = 0;

protected:
    virtual ~Store() { }
};

}

#endif

