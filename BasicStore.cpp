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

#include "BasicStore.h"
#include "RDFException.h"

#include <redland.h>

#include <QThread>
#include <QMutex>
#include <QMutexLocker>
#include <QMap>
#include <QUrl>
#include <QCryptographicHash>
#include <QReadWriteLock>

#include "Debug.h"

#include <cstdlib>
#include <iostream>

namespace Dataquay
{

class BasicStore::D
{
public:
    D() : m_storage(0), m_model(0), m_counter(0) {
        m_baseUri = "#";
        m_prefixes[""] = m_baseUri;
        m_prefixes["rdf"] = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
        m_prefixes["xsd"] = "http://www.w3.org/2001/XMLSchema#";
        clear();
    }

    ~D() {
        QMutexLocker locker(&m_librdfLock);
        if (m_model) librdf_free_model(m_model);
        if (m_storage) librdf_free_storage(m_storage);
    }
    
    void setBaseUri(QString baseUri) {
        QWriteLocker elocker(&m_expansionLock);
        QMutexLocker plocker(&m_prefixLock);
        m_baseUri = baseUri;
        m_prefixes[""] = m_baseUri;
        m_expansions.clear();
    }
    
    QString getBaseUri() const {
        return m_baseUri;
    }

    void clear() {
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::clear" << endl;
        if (m_model) librdf_free_model(m_model);
        if (m_storage) librdf_free_storage(m_storage);
        m_storage = librdf_new_storage(m_w.getWorld(), "trees", 0, 0);
        if (!m_storage) {
            DEBUG << "Failed to create RDF trees storage, falling back to default storage type" << endl;
            m_storage = librdf_new_storage(m_w.getWorld(), 0, 0, 0);
            if (!m_storage) throw RDFException("Failed to create RDF data storage");
        }
        m_model = librdf_new_model(m_w.getWorld(), m_storage, 0);
        if (!m_model) throw RDFException("Failed to create RDF data model");
    }

    void addPrefix(QString prefix, QString uri) {
        QWriteLocker elocker(&m_expansionLock);
        QMutexLocker plocker(&m_prefixLock);
        m_prefixes[prefix] = uri;
        m_expansions.clear();
    }

    bool add(Triple t) {
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::add: " << t << endl;
        return doAdd(t);
    }

    bool remove(Triple t) {
        QMutexLocker locker(&m_librdfLock);
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
                    throw RDFException("Failed to remove matched statement in remove() with wildcards");
                }
            }
            return true;
        } else {
            return doRemove(t);
        }
    }

    void change(ChangeSet cs) {
        QMutexLocker locker(&m_librdfLock);
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
        QMutexLocker locker(&m_librdfLock);
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
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::contains: " << t << endl;
        librdf_statement *statement = tripleToStatement(t);
        if (!checkComplete(statement)) {
            librdf_free_statement(statement);
            throw RDFException("Failed to test for triple (statement is incomplete)");
        }
        if (!librdf_model_contains_statement(m_model, statement)) {
            librdf_free_statement(statement);
            return false;
        } else {
            librdf_free_statement(statement);
            return true;
        }
    }
    
    Triples match(Triple t) const {
        QMutexLocker locker(&m_librdfLock);
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
        QMutexLocker locker(&m_librdfLock);
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
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::query: " << sparql << endl;
        ResultSet rs = runQuery(sparql);
        return rs;
    }

    Node queryFirst(QString sparql, QString bindingName) const {
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::queryFirst: " << bindingName << " from " << sparql << endl;
        ResultSet rs = runQuery(sparql);
        if (rs.empty()) return Node();
        for (ResultSet::const_iterator i = rs.begin(); i != rs.end(); ++i) {
            Dictionary::const_iterator j = i->find(bindingName);
            if (j == i->end()) continue;
            if (j->type == Node::Nothing) continue;
            return *j;
        }
        return Node();
    }

    QUrl getUniqueUri(QString prefix) const {
        QMutexLocker locker(&m_librdfLock);
        DEBUG << "BasicStore::getUniqueUri: prefix " << prefix << endl;
        int base = (int)(long)this; // we don't care at all about overflow
        bool good = false;
        QString uri;
        while (!good) {
            int n = base + m_counter;
            m_counter++;
            QString hashed =
                QString::fromLocal8Bit
                (QCryptographicHash::hash(QString("%1").arg(n).toLocal8Bit(),
                                          QCryptographicHash::Sha1).toHex())
                .left(12);
            uri = prefix + hashed;
            Triples t =
                doMatch(Triple(Node(Node::URI, uri), Node(), Node()), true);
            if (t.empty()) good = true;
        }
        return expand(uri);
    }

    QUrl expand(QString uri) const {

        // We cache even URIs that are already expanded or cannot be
        // expanded, since QString to QUrl conversion is slow

        if (uri == "a") uri = "rdf:type";

        m_expansionLock.lockForRead();
        if (m_expansions.contains(uri)) {
            QUrl expandedUri = m_expansions[uri];
            m_expansionLock.unlock();
            return expandedUri;
        }
        m_expansionLock.unlock();

        QUrl qu(uri);
        QString expanded(uri);
        QString maybePrefix = qu.scheme();
        m_prefixLock.lock();
        if (maybePrefix != "") {
            if (m_prefixes.find(maybePrefix) != m_prefixes.end()) {
                expanded = m_prefixes[maybePrefix] +
                    uri.right(uri.length() - (maybePrefix.length() + 1));
            }
        } else {
            if (uri.startsWith(':')) {
                expanded = m_baseUri + uri.right(uri.length() - 1);
            }
        }
        m_prefixLock.unlock();

        QUrl expandedUri(expanded);
        m_expansionLock.lockForWrite();
        if (m_expansions.contains(uri)) {
            expandedUri = m_expansions[uri];
        } else {
            m_expansions[uri] = QUrl(expanded);
            DEBUG << "new expansion: " << uri << " to "
                  << expanded << " (now have "
                  << m_expansions.size() << ")" << endl;
        }
        m_expansionLock.unlock();
        return expandedUri;
    }

    Node addBlankNode() {
        QMutexLocker locker(&m_librdfLock);
        librdf_node *node = librdf_new_node_from_blank_identifier(m_w.getWorld(), 0);
        if (!node) throw RDFException("Failed to create new blank node");
        return lrdfNodeToNode(node);
    }

    void save(QString filename) const {

        QMutexLocker wlocker(&m_librdfLock);
        QMutexLocker plocker(&m_prefixLock);

        librdf_uri *base_uri = stringToUri(m_baseUri);
        QByteArray b = filename.toLocal8Bit();
        const char *lname = b.data();

        librdf_serializer *s = librdf_new_serializer(m_w.getWorld(), "turtle", 0, 0);
        if (!s) throw RDFException("Failed to construct RDF serializer");

        for (PrefixMap::const_iterator i = m_prefixes.begin();
             i != m_prefixes.end(); ++i) {
            QByteArray b = i.key().toUtf8();
            librdf_serializer_set_namespace(s, stringToUri(i.value()), b.data());
        }
        librdf_serializer_set_namespace(s, stringToUri(m_baseUri), "");
        
        if (librdf_serializer_serialize_model_to_file(s, lname, base_uri, m_model)) {
            librdf_free_serializer(s);
            throw RDFException("Failed to export RDF model to file", filename);
        } else {
            librdf_free_serializer(s);
        }
    }

    void import(QString url, ImportDuplicatesMode idm, QString format) {

        QMutexLocker wlocker(&m_librdfLock);
        QWriteLocker elocker(&m_expansionLock);
        QMutexLocker plocker(&m_prefixLock);

        m_expansions.clear();

        librdf_uri *luri = stringToUri(url);
        librdf_uri *base_uri = stringToUri(m_baseUri);

        if (format == "") format = "guess";

        librdf_parser *parser = librdf_new_parser
            (m_w.getWorld(), format.toLocal8Bit().data(), NULL, NULL);
        if (!parser) {
            throw RDFException("Failed to construct RDF parser");
        }

        if (idm == ImportPermitDuplicates) {
            // The normal Redland behaviour, so the easy case.
            if (librdf_parser_parse_into_model
                (parser, luri, base_uri, m_model)) {
                librdf_free_parser(parser);
                throw RDFException("Failed to import model from URL", url);
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
                throw RDFException("Failed to create import RDF data storage");
            }
            librdf_model *im = librdf_new_model(m_w.getWorld(), is, 0);
            if (!im) {
                librdf_free_storage(is);
                librdf_free_parser(parser);
                throw RDFException("Failed to create import RDF data model");
            }

            librdf_stream *stream = 0;
            librdf_statement *all = 0;

            try { // so as to free parser and im on exception

                if (librdf_parser_parse_into_model(parser, luri, base_uri, im)) {
                    throw RDFException("Failed to import model from URL", url);
                }
                all = tripleToStatement(Triple());

                if (idm == ImportFailOnDuplicates) {
                    // Need to query twice, first time to check for dupes
                    stream = librdf_model_find_statements(im, all);
                    if (!stream) {
                        throw RDFException("Failed to list imported RDF model in duplicates check");
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
                    throw RDFException("Failed to list imported RDF model");
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
            QString quri = uriToString(uri);
            DEBUG << "namespace " << i << ": " << qpfx << " -> " << quri << endl;
            if (qpfx == "" && quri != "#") {
                // base uri
                if (m_baseUri == "#") {
                    std::cerr << "BasicStore::import: NOTE: Loading file into store with no base URI; setting base URI to <" << quri.toStdString() << "> from file" << std::endl;
                    m_baseUri = quri;
                    m_prefixes[""] = m_baseUri;
                } else {
                    if (quri != m_baseUri) {
                        std::cerr << "BasicStore::import: NOTE: Base URI of loaded file differs from base URI of store (<" << quri.toStdString() << "> != <" << m_baseUri.toStdString() << ">)" << std::endl;
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
    }

private:
    class World
    {
    public:
        World() {
            QMutexLocker locker(&m_mutex);
            if (!m_world) {
                m_world = librdf_new_world();
                librdf_world_open(m_world);
            }
            ++m_refcount;
        }
        ~World() {
            QMutexLocker locker(&m_mutex);
            if (--m_refcount == 0) {
                DEBUG << "Freeing world" << endl;
                librdf_free_world(m_world);
                m_world = 0;
            }
        }
        
        librdf_world *getWorld() const { return m_world; }
        
    private:
        static QMutex m_mutex;
        static librdf_world *m_world;
        static int m_refcount;
    };

    World m_w;
    librdf_storage *m_storage;
    librdf_model *m_model;
    static QMutex m_librdfLock; // assume the worst

    typedef QMap<QString, QString> PrefixMap;
    QString m_baseUri;
    PrefixMap m_prefixes;
    mutable QMutex m_prefixLock; // also protects m_baseUri

    typedef QHash<QString, QUrl> ExpansionMap;
    mutable ExpansionMap m_expansions;
    mutable QReadWriteLock m_expansionLock;

    mutable int m_counter;

    bool doAdd(Triple t) {
        librdf_statement *statement = tripleToStatement(t);
        if (!checkComplete(statement)) {
            librdf_free_statement(statement);
            throw RDFException("Failed to add triple (statement is incomplete)");
        }
        if (librdf_model_contains_statement(m_model, statement)) {
            librdf_free_statement(statement);
            return false;
        }
        if (librdf_model_add_statement(m_model, statement)) {
            librdf_free_statement(statement);
            throw RDFException("Failed to add statement to model");
        }
        librdf_free_statement(statement);
        return true;
    }

    bool doRemove(Triple t) {
        librdf_statement *statement = tripleToStatement(t);
        if (!checkComplete(statement)) {
            librdf_free_statement(statement);
            throw RDFException("Failed to remove triple (statement is incomplete)");
        }
        // Looks like librdf_model_remove_statement returns the wrong
        // value in trees storage as of 1.0.9, so let's do this check
        // separately and ignore its return value
        if (!librdf_model_contains_statement(m_model, statement)) {
            librdf_free_statement(statement);
            return false;
        }
        librdf_model_remove_statement(m_model, statement);
        librdf_free_statement(statement);
        return true;
    }

    librdf_uri *stringToUri(QString uri) const {
        librdf_uri *luri = librdf_new_uri
            (m_w.getWorld(), (const unsigned char *)uri.toUtf8().data());
        if (!luri) throw RDFException("Failed to construct URI from string", uri);
        return luri;
    }

    QString uriToString(librdf_uri *u) const {
        const char *s = (const char *)librdf_uri_as_string(u);
        if (s) return QString::fromUtf8(s);
        else return "";
    }

    QString prefixExpand(const QString uri) const {
        return expand(uri).toString();
    }

    librdf_node *nodeToLrdfNode(Node v) const { // called with m_librdfLock held
        librdf_node *node = 0;
        switch (v.type) {
        case Node::Nothing:
            return 0;
        case Node::Blank: {
            QByteArray b = v.value.toUtf8();
            const unsigned char *bident = (const unsigned char *)b.data();
            node = librdf_new_node_from_blank_identifier(m_w.getWorld(), bident);
            if (!node) throw RDFException
                           ("Failed to construct node from blank identifier",
                            v.value);
        }
            break;
        case Node::URI: {
            QString value = prefixExpand(v.value);
            librdf_uri *uri = stringToUri(value);
            if (!uri) throw RDFException("Failed to construct URI from value string", v.value);
            node = librdf_new_node_from_uri(m_w.getWorld(), uri);
            if (!node) throw RDFException("Failed to construct node from URI");
        }
            break;
        case Node::Literal: {
            QByteArray b = v.value.toUtf8();
            const unsigned char *literal = (const unsigned char *)b.data();
            if (v.datatype != "") {
                QString dtu = prefixExpand(v.datatype);
                librdf_uri *type_uri = stringToUri(dtu);
                node = librdf_new_node_from_typed_literal
                    (m_w.getWorld(), literal, 0, type_uri);
                if (!node) throw RDFException
                               ("Failed to construct node from typed literal");
            } else {
                node = librdf_new_node_from_literal
                    (m_w.getWorld(), literal, 0, 0);
                if (!node) throw RDFException
                               ("Failed to construct node from literal");
            }
        }
            break;
        }
        return node;
    }

    Node lrdfNodeToNode(librdf_node *node) const {
        
        Node v;
        if (!node) return v;

        v.type = Node::Literal;
        QString text;

        if (librdf_node_is_resource(node)) {

            v.type = Node::URI;
            librdf_uri *uri = librdf_node_get_uri(node);
            v.value = uriToString(uri);

        } else if (librdf_node_is_literal(node)) {

            v.type = Node::Literal;
            const char *s = (const char *)librdf_node_get_literal_value(node);
            if (s) v.value = QString::fromUtf8(s);
            librdf_uri *type_uri = librdf_node_get_literal_value_datatype_uri(node);
            if (type_uri) v.datatype = uriToString(type_uri);
            
        } else if (librdf_node_is_blank(node)) {

            v.type = Node::Blank;
            const char *s = (const char *)librdf_node_get_blank_identifier(node);
            if (s) v.value = s;
        }

        return v;
    }

    librdf_statement *tripleToStatement(Triple t) const {
        librdf_node *na = nodeToLrdfNode(t.a);
        librdf_node *nb = nodeToLrdfNode(t.b);
        librdf_node *nc = nodeToLrdfNode(t.c);
        librdf_statement *statement = 
            librdf_new_statement_from_nodes(m_w.getWorld(), na, nb, nc);
        if (!statement) throw RDFException("Failed to construct statement");
        return statement;
    }

    Triple statementToTriple(librdf_statement *statement) const {
        librdf_node *subject = librdf_statement_get_subject(statement);
        librdf_node *predicate = librdf_statement_get_predicate(statement);
        librdf_node *object = librdf_statement_get_object(statement);
        Triple triple(lrdfNodeToNode(subject),
                      lrdfNodeToNode(predicate),
                      lrdfNodeToNode(object));
        return triple;
    }

    bool checkComplete(librdf_statement *statement) const {
        if (librdf_statement_is_complete(statement)) return true;
        else {
            unsigned char *text = librdf_statement_to_string(statement);
            QString str = QString::fromUtf8((char *)text);
            std::cerr << "BasicStore::checkComplete: WARNING: RDF statement is incomplete: " << str.toStdString() << std::endl;
            free(text);
            return false;
        }
    }

    Triples doMatch(Triple t, bool single = false) const {
        // Any of a, b, and c in t that have Nothing as their node type
        // will contribute all matching nodes to the returned triples
        Triples results;
        librdf_statement *templ = tripleToStatement(t);
        librdf_stream *stream = librdf_model_find_statements(m_model, templ);
        librdf_free_statement(templ);
        if (!stream) throw RDFException("Failed to match RDF triples");
        while (!librdf_stream_end(stream)) {
            librdf_statement *current = librdf_stream_get_object(stream);
            if (current) results.push_back(statementToTriple(current));
            if (single) break;
            librdf_stream_next(stream);
        }
        librdf_free_stream(stream);
        return results;
    }

    ResultSet runQuery(QString rawQuery) const {
    
        if (m_baseUri == "#") {
            std::cerr << "BasicStore::runQuery: WARNING: Query requested on RDF store with default '#' base URI: results may be not as expected" << std::endl;
        }

        QString sparql;
        m_prefixLock.lock();
        for (PrefixMap::const_iterator i = m_prefixes.begin();
             i != m_prefixes.end(); ++i) {
            sparql += QString(" PREFIX %1: <%2> ").arg(i.key()).arg(i.value());
        }
        m_prefixLock.unlock();
        sparql += rawQuery;

        ResultSet returned;
        librdf_query *query =
            librdf_new_query(m_w.getWorld(), "sparql", 0,
                             (const unsigned char *)sparql.toUtf8().data(), 0);
        if (!query) return returned;

        librdf_query_results *results = librdf_query_execute(query, m_model);
        if (!results) {
            librdf_free_query(query);
            return returned;
        }
        if (!librdf_query_results_is_bindings(results)) {
            librdf_free_query_results(results);
            librdf_free_query(query);
            return returned;
        }
    
        while (!librdf_query_results_finished(results)) {
            int count = librdf_query_results_get_bindings_count(results);
            Dictionary dict;
            for (int i = 0; i < count; ++i) {

                const char *name =
                    librdf_query_results_get_binding_name(results, i);
                if (!name) continue;
                QString key = (const char *)name;

                librdf_node *node =
                    librdf_query_results_get_binding_value(results, i);

                dict[key] = lrdfNodeToNode(node);
            }

            returned.push_back(dict);
            librdf_query_results_next(results);
        }

        librdf_free_query_results(results);
        librdf_free_query(query);

//        DEBUG << "runQuery: returning " << returned.size() << " result(s)" << endl;
        
        return returned;
    }
};

QMutex
BasicStore::D::m_librdfLock;

QMutex
BasicStore::D::World::m_mutex;

librdf_world *
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
BasicStore::setBaseUri(QString uri)
{
    m_d->setBaseUri(uri);
}

QString
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
BasicStore::addPrefix(QString prefix, QString uri)
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

QUrl
BasicStore::getUniqueUri(QString prefix) const
{
    return m_d->getUniqueUri(prefix);
}

QUrl
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
BasicStore::import(QString url, ImportDuplicatesMode idm, QString format)
{
    m_d->import(url, idm, format);
}

BasicStore *
BasicStore::load(QString url, QString format)
{
    BasicStore *s = new BasicStore();
    // store is empty, ImportIgnoreDuplicates is faster
    s->import(url, ImportIgnoreDuplicates, format);
    return s;
}

}



		
