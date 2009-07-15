/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_BASIC_STORE_H_
#define _RDF_BASIC_STORE_H_

#include "Store.h"

namespace Dataquay
{
	
/**
 * \class BasicStore BasicStore.h <dataquay/BasicStore.h>
 *
 * In-memory RDF data store implementing the Store interface,
 * providing add, remove, matching and query operations for RDF
 * triples and SPARQL, as well as export and import.  It uses librdf
 * internally.
 *
 * All operations are thread safe.
 */
class BasicStore : public Store
{
public:
    BasicStore();
    ~BasicStore();

    /**
     * Set the base URI for the store.  This is used to expand the
     * empty URI prefix when adding and querying triples, and is also
     * used as the document base URI when exporting.
     *
     * The default base URI is "#" (resolve local URIs relative to
     * this document).
     */
    void setBaseUri(QString uri);

    /**
     * Retrieve the base URI for the store.
     */
    QString getBaseUri() const;
    
    /**
     * Empty the store (including prefixes as well as triples).
     */
    void clear();

    /**
     * Add a prefix/uri pair (an XML namespace, except that this class
     * doesn't directly deal in XML) for use in subsequent operations.
     *
     * Example: addPrefix("dc", "http://purl.org/dc/elements/1.1/") to
     * add a prefix for the Dublin Core namespace.
     *
     * The store always knows about the XSD and RDF namespaces.
     *
     * Note that the base URI is always available as the empty prefix.
     * For example, the URI ":blather" will be expanded to the base
     * URI plus "blather".
     */
    void addPrefix(QString prefix, QString uri);

    // Store interface

    bool add(Triple t);
    bool remove(Triple t);

    void change(ChangeSet changes);
    void revert(ChangeSet changes);

    bool contains(Triple t) const;
    Triples match(Triple t) const;
    ResultSet query(QString sparql) const;

    Triple matchFirst(Triple t) const;
    Node queryFirst(QString sparql, QString bindingName) const;

    QUrl getUniqueUri(QString prefix) const;
    QUrl expand(QString uri) const;

    /**
     * Export the store to an RDF/TTL file with the given filename.
     * If the file already exists, it will if possible be overwritten.
     * May throw RDFException, FileOperationFailed, FailedToOpenFile
     * etc.
     */
    void save(QString filename) const;

    /**
     * Import the RDF document found at the given URL into the current
     * store (in addition to its existing contents).  May throw
     * RDFException, FileNotFound etc.
     *
     * Note that prefixes are not restored from the imported file.
    */
    void import(QString url);

    /**
     * Construct a new Store from the RDF document at the given URL.
     * May throw RDFException, FileNotFound etc.
     *
     * Note that query prefixes added using addQueryPrefix are not
     * restored from in the imported file.
     */
    static BasicStore *load(QString url);

private:
    class D;
    D *m_d;
};

}

#endif
    
