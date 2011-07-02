/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management.
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

#ifdef USE_SORD

#include "BasicStore.h"
#include "RDFException.h"

#include <sord/sord.h>

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <QFile>
#include <QCryptographicHash>
#include <QReadWriteLock>

#include "../Debug.h"

#include <cstdlib>
#include <iostream>

namespace Dataquay
{

class BasicStore::D
{
public:
    D() : m_model(0) {
        m_baseUri = Uri("#");
        m_prefixes[""] = m_baseUri;
        // note this is also hardcoded in expand():
        m_prefixes["rdf"] = Uri("http://www.w3.org/1999/02/22-rdf-syntax-ns#");
        m_prefixes["xsd"] = Uri("http://www.w3.org/2001/XMLSchema#");
        clear();
    }

    ~D() {
        QMutexLocker locker(&m_backendLock);
        if (m_model) sord_free(m_model);
    }

    QString getNewString() const {
        QString s =
            QString::fromLocal8Bit
            (QCryptographicHash::hash(QString("%1").arg(random()).toLocal8Bit(),
                                      QCryptographicHash::Sha1).toHex())
            .left(12);
        // This may be used as the whole of a name in some contexts,
        // so it must not start with a digit
        if (s[0].isDigit()) {
            s = "x" + s.right(s.length()-1);
        }
        return s;
    }
    
    void collision() const {
        // If we get a collision when generating a "random" string,
        // seed the random number generator (it probably means the
        // generator hasn't been seeded at all).  But only once.
        static QMutex m;
        static bool seeded = false;
        static QMutexLocker l(&m);
        if (!seeded) return;
        srandom(time(0));
        seeded = true;
    }

    void setBaseUri(Uri baseUri) {
        QMutexLocker plocker(&m_prefixLock);
        m_baseUri = baseUri;
        m_prefixes[""] = m_baseUri;
    }
    
    Uri getBaseUri() const {
        return m_baseUri;
    }

    void clear() {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::clear" << endl;
        if (m_model) sord_free(m_model);
        //!!! are these the right indices?
        m_model = sord_new(m_w.getWorld(), SORD_SPO|SORD_OPS, false);
        if (!m_model) throw RDFInternalError("Failed to create RDF data model");
    }

    void addPrefix(QString prefix, Uri uri) {
        QMutexLocker plocker(&m_prefixLock);
        m_prefixes[prefix] = uri;
    }

    bool add(Triple t) {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::add: " << t << endl;
        return doAdd(t);
    }

    bool remove(Triple t) {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::remove: " << t << endl;
        if (t.a.type == Node::Nothing || 
            t.b.type == Node::Nothing ||
            t.c.type == Node::Nothing) {
            Triples tt = doMatch(t);
            if (tt.empty()) return false;
            DEBUG << "BasicStore::remove: Removing " << tt.size() << " triple(s)" << endl;
            for (int i = 0; i < tt.size(); ++i) {
                if (!doRemove(tt[i])) {
                    DEBUG << "Failed to remove matched triple in remove() with wildcards; triple was: " << tt[i] << endl;
                    throw RDFInternalError("Failed to remove matched statement in remove() with wildcards");
                }
            }
            return true;
        } else {
            return doRemove(t);
        }
    }

    void change(ChangeSet cs) {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::change: " << cs.size() << " changes" << endl;
        for (int i = 0; i < cs.size(); ++i) {
            ChangeType type = cs[i].first;
            switch (type) {
            case AddTriple:
                if (!doAdd(cs[i].second)) {
                    throw RDFException("Change add failed due to duplication");
                }
                break;
            case RemoveTriple:
                if (!doRemove(cs[i].second)) {
                    throw RDFException("Change remove failed due to absence");
                }
                break;
            }
        }
    }

    void revert(ChangeSet cs) {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::revert: " << cs.size() << " changes" << endl;
        for (int i = cs.size()-1; i >= 0; --i) {
            ChangeType type = cs[i].first;
            switch (type) {
            case AddTriple:
                if (!doRemove(cs[i].second)) {
                    throw RDFException("Change revert add failed due to absence");
                }
                break;
            case RemoveTriple:
                if (!doAdd(cs[i].second)) {
                    throw RDFException("Change revert remove failed due to duplication");
                }
                break;
            }
        }
    }

    bool contains(Triple t) const {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::contains: " << t << endl;
        SordQuad statement;
        tripleToStatement(t, statement);
        if (!checkComplete(statement)) {
            throw RDFException("Failed to test for triple (statement is incomplete)");
        }
        if (!sord_contains(m_model, statement)) {
            return false;
        }
        freeStatement(statement);
        return true;
    }
    
    Triples match(Triple t) const {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::match: " << t << endl;
        Triples result = doMatch(t);
#ifndef NDEBUG
        DEBUG << "BasicStore::match result (size " << result.size() << "):" << endl;
        for (int i = 0; i < result.size(); ++i) {
            DEBUG << i << ". " << result[i] << endl;
        }
#endif
        return result;
    }

    Triple matchFirst(Triple t) const {
        if (t.c != Node() && t.b != Node() && t.a != Node()) {
            // triple is complete: short-circuit to a single lookup
            if (contains(t)) return t;
            else return Triple();
        }
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::matchFirst: " << t << endl;
        Triples result = doMatch(t, true);
#ifndef NDEBUG
        DEBUG << "BasicStore::matchFirst result:" << endl;
        for (int i = 0; i < result.size(); ++i) {
            DEBUG << i << ". " << result[i] << endl;
        }
#endif
        if (result.empty()) return Triple();
        else return result[0];
    }

    ResultSet query(QString sparql) const {
        throw RDFUnsupportedError
            ("SPARQL queries are not supported with Sord backend",
             sparql);
    }

    Node queryFirst(QString sparql, QString bindingName) const {
        throw RDFUnsupportedError
            ("SPARQL queries are not supported with Sord backend",
             sparql);
    }

    Uri getUniqueUri(QString prefix) const {
        QMutexLocker locker(&m_backendLock);
        DEBUG << "BasicStore::getUniqueUri: prefix " << prefix << endl;
        bool good = false;
        QString uri;
        while (!good) {
            QString s = getNewString();
            uri = prefix + s;
            Triples t =
                doMatch(Triple(Node(Node::URI, uri), Node(), Node()), true);
            if (t.empty()) good = true;
            else collision();
        }
        return expand(uri);
    }

    Uri expand(QString shrt) const {

        if (shrt == "a") {
            static Uri rdfTypeUri
                ("http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
            return rdfTypeUri;
        }

        int index = shrt.indexOf(':');
        if (index == 0) {
            // starts with colon
            return Uri(m_baseUri.toString() + shrt.right(shrt.length() - 1));
        } else if (index > 0) {
            // colon appears in middle somewhere
            if (index + 2 < shrt.length() &&
                shrt[index+1] == '/' &&
                shrt[index+2] == '/') {
                // we have found "://", this is a scheme, therefore
                // the uri is already expanded
                return Uri(shrt);
            }
        } else {
            // no colon present, no possibility of expansion
            return Uri(shrt);
        }

        // fall through only for colon in middle and no "://" found,
        // i.e. a plausible prefix appears

        QString prefix = shrt.left(index);
        QString expanded;

        m_prefixLock.lock();
        PrefixMap::const_iterator pi = m_prefixes.find(prefix);
        if (pi != m_prefixes.end()) {
            expanded = pi.value().toString() +
                shrt.right(shrt.length() - (index + 1));
        } else {
            expanded = shrt;
        }
        m_prefixLock.unlock();

        return Uri(expanded);
    }

    Node addBlankNode() {
        QMutexLocker locker(&m_backendLock);
        QString blankId = getNewString();
        //!!! todo: how to check whether the blank node is already in use
        SordNode *node = sord_new_blank(m_w.getWorld(), (uint8_t *)blankId.toUtf8().data());
        if (!node) throw RDFInternalError("Failed to create new blank node");
        return sordNodeToNode(node);
    }

    static size_t saveSink(const void *buf, size_t len, void *stream) {
        QIODevice *dev = (QIODevice *)stream;
        qint64 r = dev->write((const char *)buf, len);
        if (r < 0) throw RDFException("Write failed");
        else return r;
    }

    void save(QString filename) const {

        QMutexLocker wlocker(&m_backendLock);
        QMutexLocker plocker(&m_prefixLock);

        DEBUG << "BasicStore::save(" << filename << ")" << endl;

        QByteArray bb = m_baseUri.toString().toUtf8();
        SerdURI bu;

        if (serd_uri_parse((uint8_t *)bb.data(), &bu) != SERD_SUCCESS) {
            throw RDFInternalError("Failed to parse base URI", m_baseUri);
        }

        SerdNode bn = serd_node_from_string(SERD_URI, (uint8_t *)bb.data());
        SerdEnv *env = serd_env_new(&bn);

        for (PrefixMap::const_iterator i = m_prefixes.begin();
             i != m_prefixes.end(); ++i) {
            addToSerdNamespace(env, i.key(), i.value().toString());
        }

//        addToSerdNamespace(env, QString(), m_baseUri.toString());

        QFile f(filename);
        if (!f.exists()) {
            if (!f.open(QFile::WriteOnly)) {
                throw RDFException("Failed to open file for writing", filename);
            }
            f.close();
        }

        QString tmpFilename = QString("%1.part").arg(filename);

        QFile tf(tmpFilename);
        if (!tf.open(QFile::WriteOnly)) {
            throw RDFException("Failed to open partial file for writing", tmpFilename);
        }
        
        SerdEnv *wenv = serd_env_new(&bn);

        SerdWriter *writer = serd_writer_new
            (SERD_TURTLE,
             SerdStyle(SERD_STYLE_ABBREVIATED | SERD_STYLE_RESOLVED | SERD_STYLE_CURIED),
             wenv, &bu, saveSink, &tf);

	serd_env_foreach(env,
	                 (SerdPrefixSink)serd_writer_set_prefix,
	                 writer);

        sord_write(m_model, writer, NULL);

	serd_writer_finish(writer);
	serd_writer_free(writer);

	serd_env_free(env);
	serd_env_free(wenv);
        
        tf.close();

        // New file is now completed; the following is scruffy, but
        // that shouldn't really matter now

        if (!QFile::remove(filename)) {
            // Not necessarily fatal
            DEBUG << "BasicStore::save: Failed to remove former save file "
                  << filename << endl;
        }
        if (!QFile::rename(tmpFilename, filename)) {
            throw RDFException("Failed to rename temporary file to save file",
                               filename);
        }
    }

    void addPrefixOnImport(QString pfx, Uri uri) {

        DEBUG << "namespace: " << pfx << " -> " << uri << endl;

        if (pfx == "" && uri != Uri("#")) {
            // base uri
            if (m_baseUri == Uri("#")) {
                std::cerr << "BasicStore::import: NOTE: Loading file into store with no base URI; setting base URI to <" << uri.toString().toStdString() << "> from file" << std::endl;
                m_baseUri = uri;
                m_prefixes[""] = m_baseUri;
            } else {
                if (uri != m_baseUri) {
                    std::cerr << "BasicStore::import: NOTE: Base URI of loaded file differs from base URI of store (<" << uri.toString().toStdString() << "> != <" << m_baseUri.toString().toStdString() << ">)" << std::endl;
                }
            }
        }

        // don't call addPrefix; it tries to lock the mutex,
        // and anyway we want to add the prefix only if it
        // isn't already there (to avoid surprisingly changing
        // a prefix in unusual cases, or changing the base URI)
        if (m_prefixes.find(pfx) == m_prefixes.end()) {
            m_prefixes[pfx] = uri;
        }
    }

    static SerdStatus addPrefixSink(void *handle,
                                    const SerdNode *name,
                                    const SerdNode *uri) {

        D *d = (D *)handle;

        QString qpfx(QString::fromUtf8((const char *)name->buf, name->n_bytes));
        Uri quri(QString::fromUtf8((const char *)uri->buf, uri->n_bytes));

        d->addPrefixOnImport(qpfx, quri);

        return SERD_SUCCESS;
    }

    void import(QUrl url, ImportDuplicatesMode idm, QString format) {

        QMutexLocker wlocker(&m_backendLock);
        QMutexLocker plocker(&m_prefixLock);

        //!!! todo: format?

        //!!! todo: retrieve prefixes from model after loading

        QByteArray bb = m_baseUri.toString().toUtf8();
        SerdURI bu;

        if (serd_uri_parse((uint8_t *)bb.data(), &bu) != SERD_SUCCESS) {
            throw RDFInternalError("Failed to parse base URI", m_baseUri);
        }

        SerdNode bn = serd_node_from_string(SERD_URI, (uint8_t *)bb.data());
        SerdEnv *env = serd_env_new(&bn);

        QString fileUri = url.toString();

        // serd_uri_to_path doesn't like the brief file:blah
        // convention, it insists that file: is followed by //
        // (technically correct, but a bit pedantic)

        if (fileUri.startsWith("file:") && !fileUri.startsWith("file://")) {
            // however, it's happy with scheme-less paths
            fileUri = fileUri.right(fileUri.length()-5);
        }

        if (idm == ImportPermitDuplicates) {

            // No special handling for duplicates, do whatever the
            // underlying engine does

            SerdReader *reader = sord_new_reader(m_model, env, SERD_TURTLE, NULL);

            if (serd_reader_read_file
                (reader, (const uint8_t *)fileUri.toLocal8Bit().data())) {
                serd_reader_free(reader);
                serd_env_free(env);
                throw RDFException("Failed to import model from URL",
                                   url.toString());
            }

            serd_reader_free(reader);

        } else {

            // ImportFailOnDuplicates and ImportIgnoreDuplicates:
            // import into a separate model and transfer across

            SordModel *im = sord_new(m_w.getWorld(), 0, false); // no index
            
            SerdReader *reader = sord_new_reader(im, env, SERD_TURTLE, NULL);

            if (serd_reader_read_file
                (reader, (const uint8_t *)fileUri.toLocal8Bit().data())) {
                serd_reader_free(reader);
                sord_free(im);
                serd_env_free(env);
                throw RDFException("Failed to import model from URL",
                                   url.toString());
            }

            serd_reader_free(reader);

            SordQuad templ;
            tripleToStatement(Triple(), templ);

            if (idm == ImportFailOnDuplicates) {

                SordIter *itr = sord_find(im, templ);
                while (!sord_iter_end(itr)) {
                    SordQuad q;
                    sord_iter_get(itr, q);
                    if (sord_contains(m_model, q)) {
                        sord_iter_free(itr);
                        freeStatement(templ);
                        sord_free(im);
                        serd_env_free(env);
                        throw RDFDuplicateImportException("Duplicate statement encountered on import in ImportFailOnDuplicates mode");
                    }
                    sord_iter_next(itr);
                }
                sord_iter_free(itr);
            }
            
            SordIter *itr = sord_find(im, templ);
            while (!sord_iter_end(itr)) {
                SordQuad q;
                sord_iter_get(itr, q);
                if (idm == ImportFailOnDuplicates || // (already tested if so)
                    !sord_contains(m_model, q)) {
                    sord_add(m_model, q);
                }
                sord_iter_next(itr);
            }
            sord_iter_free(itr);
            freeStatement(templ);
            sord_free(im);
        }

	serd_env_foreach(env, addPrefixSink, this);
        serd_env_free(env);

/*!!!
        librdf_uri *luri = uriToSordUri(Uri(url));
        librdf_uri *base_uri = uriToSordUri(m_baseUri);

        if (format == "") format = "guess";

        librdf_parser *parser = librdf_new_parser
            (m_w.getWorld(), format.toLocal8Bit().data(), NULL, NULL);
        if (!parser) {
            throw RDFInternalError("Failed to construct RDF parser");
        }

        if (idm == ImportPermitDuplicates) {
            // The normal Redland behaviour, so the easy case.
            if (librdf_parser_parse_into_model
                (parser, luri, base_uri, m_model)) {
                librdf_free_parser(parser);
                throw RDFException("Failed to import model from URL",
                                   url.toString());
            }
        } else { // ImportFailOnDuplicates and ImportIgnoreDuplicates modes

            // This is complicated by our desire to avoid storing any
            // duplicate triples on import, and optionally to be able
            // to fail if any are found.  So we import into a separate
            // model and then transfer over.  Not very efficient, but
            // scalability is not generally the primary concern for us

            librdf_storage *is = librdf_new_storage(m_w.getWorld(), "trees", 0, 0);
            if (!is) is = librdf_new_storage(m_w.getWorld(), 0, 0, 0);
            if (!is) {
                librdf_free_parser(parser);
                throw RDFInternalError("Failed to create import RDF data storage");
            }
            librdf_model *im = librdf_new_model(m_w.getWorld(), is, 0);
            if (!im) {
                librdf_free_storage(is);
                librdf_free_parser(parser);
                throw RDFInternalError("Failed to create import RDF data model");
            }

            librdf_stream *stream = 0;
            librdf_statement *all = 0;

            try { // so as to free parser and im on exception

                if (librdf_parser_parse_into_model(parser, luri, base_uri, im)) {
                    throw RDFException("Failed to import model from URL",
                                       url.toString());
                }
                tripleToStatement(Triple(), all);

                if (idm == ImportFailOnDuplicates) {
                    // Need to query twice, first time to check for dupes
                    stream = librdf_model_find_statements(im, all);
                    if (!stream) {
                        throw RDFInternalError("Failed to list imported RDF model in duplicates check");
                    }
                    while (!librdf_stream_end(stream)) {
                        librdf_statement *current = librdf_stream_get_object(stream);
                        if (!current) continue;
                        if (librdf_model_contains_statement(m_model, current)) {
                            throw RDFDuplicateImportException("Duplicate statement encountered on import in ImportFailOnDuplicates mode");
                        }
                        librdf_stream_next(stream);
                    }
                    librdf_free_stream(stream);
                    stream = 0;
                }

                // Now import.  Have to do this "manually" because librdf
                // may allow duplicates and we want to avoid them
                stream = librdf_model_find_statements(im, all);
                if (!stream) {
                    throw RDFInternalError("Failed to list imported RDF model");
                }
                while (!librdf_stream_end(stream)) {
                    librdf_statement *current = librdf_stream_get_object(stream);
                    if (!current) continue;
                    if (!librdf_model_contains_statement(m_model, current)) {
                        librdf_model_add_statement(m_model, current);
                    }
                    librdf_stream_next(stream);
                }
                librdf_free_stream(stream);
                stream = 0;

            } catch (...) {
                if (stream) librdf_free_stream(stream);
                if (all) librdf_free_statement(all);
                librdf_free_parser(parser);
                librdf_free_model(im);
                librdf_free_storage(is);
                throw;
            }

            librdf_free_model(im);
            librdf_free_storage(is);
        }

        int namespaces = librdf_parser_get_namespaces_seen_count(parser);
        DEBUG << "Parser found " << namespaces << " namespaces" << endl;
        for (int i = 0; i < namespaces; ++i) {
            const char *pfx = librdf_parser_get_namespaces_seen_prefix(parser, i);
            librdf_uri *uri = librdf_parser_get_namespaces_seen_uri(parser, i);
            QString qpfx = QString::fromUtf8(pfx);
            Uri quri = sordUriToUri(uri);
            DEBUG << "namespace " << i << ": " << qpfx << " -> " << quri << endl;
            if (qpfx == "" && quri != Uri("#")) {
                // base uri
                if (m_baseUri == Uri("#")) {
                    std::cerr << "BasicStore::import: NOTE: Loading file into store with no base URI; setting base URI to <" << quri.toString().toStdString() << "> from file" << std::endl;
                    m_baseUri = quri;
                    m_prefixes[""] = m_baseUri;
                } else {
                    if (quri != m_baseUri) {
                        std::cerr << "BasicStore::import: NOTE: Base URI of loaded file differs from base URI of store (<" << quri.toString().toStdString() << "> != <" << m_baseUri.toString().toStdString() << ">)" << std::endl;
                    }
                }
            }
            // don't call addPrefix; it tries to lock the mutex,
            // and anyway we want to add the prefix only if it
            // isn't already there (to avoid surprisingly changing
            // a prefix in unusual cases, or changing the base URI)
            if (m_prefixes.find(qpfx) == m_prefixes.end()) {
                m_prefixes[qpfx] = quri;
            }
        }

        librdf_free_parser(parser);
*/
    }

private:
    class World
    {
    public:
        World() {
            QMutexLocker locker(&m_mutex);
            if (!m_world) {
                m_world = sord_world_new();
            }
            ++m_refcount;
        }
        ~World() {
            QMutexLocker locker(&m_mutex);
            DEBUG << "~World: About to lower refcount from " << m_refcount << endl;
            if (--m_refcount == 0) {
                DEBUG << "Freeing world" << endl;
                sord_world_free(m_world);
                m_world = 0;
            }
        }
        
        SordWorld *getWorld() const {
            return m_world;
        }
        
    private:
        static QMutex m_mutex;
        static SordWorld *m_world;
        static int m_refcount;
    };

    World m_w;
    SordModel *m_model;
    static QMutex m_backendLock; // assume the worst

    typedef QHash<QString, Uri> PrefixMap;
    Uri m_baseUri;
    PrefixMap m_prefixes;
    mutable QMutex m_prefixLock; // also protects m_baseUri

    bool doAdd(Triple t) {
        SordQuad statement;
        tripleToStatement(t, statement);
        if (!checkComplete(statement)) {
            throw RDFException("Failed to add triple (statement is incomplete)");
        }
        if (sord_contains(m_model, statement)) {
            return false;
        }
        sord_add(m_model, statement);
        freeStatement(statement);
        return true;
    }

    bool doRemove(Triple t) {
        SordQuad statement;
        tripleToStatement(t, statement);
        if (!checkComplete(statement)) {
            throw RDFException("Failed to remove triple (statement is incomplete)");
        }
        if (!sord_contains(m_model, statement)) {
            return false;
        }
        sord_remove(m_model, statement);
        freeStatement(statement);
        return true;
    }

    SordNode *uriToSordNode(Uri uri) const {
        SordNode *node = sord_new_uri
            (m_w.getWorld(), 
             (const unsigned char *)uri.toString().toUtf8().data());
        if (!node) throw RDFInternalError("Failed to convert URI to internal representation", uri);
        return node;
    }

    //!!! utility function to extract string value from node would be more useful
    Uri sordNodeToUri(const SordNode *n) const {
        if (!n || sord_node_get_type(n) != SORD_URI) {
            return Uri();
        }
        const uint8_t *s = sord_node_get_string(n);
        if (s) return Uri(QString::fromUtf8((char *)s));
        else return Uri();
    }

    SordNode *nodeToSordNode(Node v) const { // called with m_backendLock held
        SordNode *node = 0;
        switch (v.type) {
        case Node::Nothing:
            return 0;
        case Node::Blank: {
            QByteArray b = v.value.toUtf8();
            const unsigned char *bident = (const unsigned char *)b.data();
            node = sord_new_blank(m_w.getWorld(), bident);
            if (!node) throw RDFException
                           ("Failed to construct node from blank identifier",
                            v.value);
        }
            break;
        case Node::URI: {
            Uri value = expand(v.value);
            node = uriToSordNode(value);
            if (!node) throw RDFException("Failed to construct node from URI");
        }
            break;
        case Node::Literal: {
            QByteArray b = v.value.toUtf8();
            const unsigned char *literal = (const unsigned char *)b.data();
            if (v.datatype != Uri()) {
                SordNode *typeNode = uriToSordNode(v.datatype);
                node = sord_new_literal(m_w.getWorld(), typeNode, literal, 0);
                if (!node) throw RDFException
                               ("Failed to construct node from literal of type ",
                                v.datatype);
            } else {
                node = sord_new_literal(m_w.getWorld(), 0, literal, 0);
                if (!node) throw RDFException
                               ("Failed to construct node from literal");
            }
        }
            break;
        }
        return node;
    }

    Node sordNodeToNode(const SordNode *node) const {
        
        Node v;
        if (!node) return v;

        SordNodeType type = sord_node_get_type(node);

        switch (type) {

        case SORD_URI:
            v.type = Node::URI;
            v.value = sordNodeToUri(node).toString();
            break;

        case SORD_BLANK:
            v.type = Node::Blank;
            //!!! utility function for this -- types and what if it's null?
            v.value = QString::fromUtf8((char *)sord_node_get_string(node));
            break;

        case SORD_LITERAL:
            v.type = Node::Literal;
            //!!! utility function for this -- types and what if it's null?
            v.value = QString::fromUtf8((char *)sord_node_get_string(node));
            v.datatype = sordNodeToUri(sord_node_get_datatype(node));
            break;
        }

        return v;
    }

    void tripleToStatement(Triple t, SordQuad q) const {
        q[0] = nodeToSordNode(t.a);
        q[1] = nodeToSordNode(t.b);
        q[2] = nodeToSordNode(t.c);
        q[3] = 0;
    }

    void freeStatement(SordQuad q) const {
        // Not for removing statements from the store
        for (int i = 0; i < 4; ++i) {
            sord_node_free(m_w.getWorld(), (SordNode *)q[i]);
        }
    }

    Triple statementToTriple(const SordQuad q) const {
        Triple triple(sordNodeToNode(q[0]),
                      sordNodeToNode(q[1]),
                      sordNodeToNode(q[2]));
        return triple;
    }

    bool checkComplete(const SordQuad q) const {
        if (!q[0] || !q[1] || !q[2]) {
            std::cerr << "BasicStore::checkComplete: WARNING: RDF statement contains one or more NULL nodes" << std::endl;
            return false;
        }
        if ((sord_node_get_type(q[0]) == SORD_URI ||
             sord_node_get_type(q[0]) == SORD_BLANK) &&
            (sord_node_get_type(q[1]) == SORD_URI)) {
            return true;
        } else {
            std::cerr << "BasicStore::checkComplete: WARNING: RDF statement is incomplete: [" << sord_node_get_string(q[0]) << "," << sord_node_get_string(q[1]) << "," << sord_node_get_string(q[2]) << "]" << std::endl;
            return false;
        }
    }

    void addToSerdNamespace(SerdEnv *env, QString key, QString value) const {

        QByteArray b = key.toUtf8();
        QByteArray v = value.toUtf8();

        SerdNode name = serd_node_from_string(SERD_URI, (uint8_t *)b.data());
        SerdNode uri = serd_node_from_string(SERD_URI, (uint8_t *)v.data());
            
        serd_env_set_prefix(env, &name, &uri); // copies name, uri
    }

    Triples doMatch(Triple t, bool single = false) const {
        // Any of a, b, and c in t that have Nothing as their node type
        // will contribute all matching nodes to the returned triples
        Triples results;
        SordQuad templ;
        tripleToStatement(t, templ);
        SordIter *itr = sord_find(m_model, templ);
        while (!sord_iter_end(itr)) {
            SordQuad q;
            sord_iter_get(itr, q);
            results.push_back(statementToTriple(q));
            if (single) break;
            sord_iter_next(itr);
        }
        sord_iter_free(itr);
        freeStatement(templ);
        return results;
    }
};

QMutex
BasicStore::D::m_backendLock;

QMutex
BasicStore::D::World::m_mutex;

SordWorld *
BasicStore::D::World::m_world = 0;

int
BasicStore::D::World::m_refcount = 0;

BasicStore::BasicStore() :
    m_d(new D())
{
}

BasicStore::~BasicStore()
{
    delete m_d;
}

void
BasicStore::setBaseUri(Uri uri)
{
    m_d->setBaseUri(uri);
}

Uri
BasicStore::getBaseUri() const
{
    return m_d->getBaseUri();
}

void
BasicStore::clear()
{
    m_d->clear();
}

bool
BasicStore::add(Triple t)
{
    return m_d->add(t);
}

bool
BasicStore::remove(Triple t)
{
    return m_d->remove(t);
}

void
BasicStore::change(ChangeSet t)
{
    m_d->change(t);
}

void
BasicStore::revert(ChangeSet t)
{
    m_d->revert(t);
}

bool
BasicStore::contains(Triple t) const
{
    return m_d->contains(t);
}

Triples
BasicStore::match(Triple t) const
{
    return m_d->match(t);
}

void
BasicStore::addPrefix(QString prefix, Uri uri)
{
    m_d->addPrefix(prefix, uri);
}

ResultSet
BasicStore::query(QString sparql) const
{
    return m_d->query(sparql);
}

Triple
BasicStore::matchFirst(Triple t) const
{
    return m_d->matchFirst(t);
}

Node
BasicStore::queryFirst(QString sparql, QString bindingName) const
{
    return m_d->queryFirst(sparql, bindingName);
}

Uri
BasicStore::getUniqueUri(QString prefix) const
{
    return m_d->getUniqueUri(prefix);
}

Uri
BasicStore::expand(QString uri) const
{
    return m_d->expand(uri);
}

Node
BasicStore::addBlankNode()
{
    return m_d->addBlankNode();
}

void
BasicStore::save(QString filename) const
{
    m_d->save(filename);
}

void
BasicStore::import(QUrl url, ImportDuplicatesMode idm, QString format)
{
    m_d->import(url, idm, format);
}

BasicStore *
BasicStore::load(QUrl url, QString format)
{
    BasicStore *s = new BasicStore();
    // store is empty, ImportIgnoreDuplicates is faster
    s->import(url, ImportIgnoreDuplicates, format);
    return s;
}

BasicStore::Features
BasicStore::getSupportedFeatures() const
{
    Features fs;
    fs << ModifyFeature;
    return fs;
}

}

#endif


		
