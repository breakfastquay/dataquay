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

#ifndef _DATAQUAY_BASIC_STORE_H_
#define _DATAQUAY_BASIC_STORE_H_

#include "Store.h"

namespace Dataquay
{
	
/**
 * \class BasicStore BasicStore.h <dataquay/BasicStore.h>
 *
 * In-memory RDF data store implementing the Store interface,
 * providing add, remove, matching and query operations for RDF
 * triples and SPARQL, as well as export and import.  BasicStore uses
 * the Redland librdf datastore internally.
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
     * If the prefix has already been added, this overrides any uri
     * associated with it.
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

    Uri getUniqueUri(QString prefix) const;
    Node addBlankNode();
    Uri expand(QString uri) const;

    /**
     * Export the store to an RDF/TTL file with the given filename.
     * If the file already exists, it will if possible be overwritten.
     * May throw RDFException, FileOperationFailed, FailedToOpenFile,
     * etc.
     *
     * Note that unlike import and load (which take URL arguments),
     * save takes a simple filename with no file:// prefix.
     */
    void save(QString filename) const;

    /**
     * ImportDuplicatesMode determines the outcome when an import
     * operation encounters a triple in the imported data set that
     * already exists in the store.
     *
     * ImportIgnoreDuplicates: Any duplicate of a triple that is
     * already in the store is discarded without comment.
     *
     * ImportFailOnDuplicates: Import will fail with an
     * RDFDuplicateImportException if any duplicate of a triple
     * already in the store is found, and nothing will be imported.
     *
     * ImportPermitDuplicates: No tests for duplicate triples will be
     * carried out, and the behaviour when duplicates are imported
     * will depend on the underlying store implementation (which may
     * merge them or store them as separate duplicate triples).  This
     * is usually inadvisable: besides its unpredictability, this
     * class does not generally handle duplicate triples well in other
     * contexts.
     */
    enum ImportDuplicatesMode {
        ImportIgnoreDuplicates,
        ImportFailOnDuplicates,
        ImportPermitDuplicates
    };

    /**
     * Import the RDF document found at the given URL into the current
     * store (in addition to its existing contents).  Its behaviour
     * when a triple is encountered that already exists in the store
     * is controlled by the ImportDuplicatesMode.
     * 
     * May throw RDFException.
     *
     * Note that the URL must be a URL, not just a filename
     * (i.e. local files need the file: prefix).
     *
     * If format is specified, it will be taken as the RDF parse
     * format (e.g. ntriples).  The set of supported format strings
     * depends on the underlying RDF library configuration.  The
     * default is to guess the format if possible.
     */
    void import(QString url, ImportDuplicatesMode idm, QString format = "");

    /**
     * Construct a new BasicStore from the RDF document at the given
     * URL.  May throw RDFException.  The returned BasicStore is owned
     * by the caller and must be deleted using delete when finished
     * with.
     *
     * Note that the URL must be a URL, not just a filename
     * (i.e. local files need the file: prefix).
     *
     * If format is specified, it will be taken as the RDF parse
     * format (e.g. ntriples).  The set of supported format strings
     * depends on the underlying RDF library configuration.  The
     * default is to guess the format if possible.
     */
    static BasicStore *load(QString url, QString format = "");

private:
    class D;
    D *m_d;
};

}

#endif
    
