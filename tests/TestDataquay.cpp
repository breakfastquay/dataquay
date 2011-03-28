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

#include "../Node.h"
#include "../BasicStore.h"
#include "../PropertyObject.h"
#include "../TransactionalStore.h"
#include "../Connection.h"
#include "../objectmapper/ObjectLoader.h"
#include "../objectmapper/ObjectStorer.h"
#include "../objectmapper/ObjectMapper.h"
#include "../objectmapper/TypeMapping.h"
#include "../objectmapper/ObjectBuilder.h"
#include "../objectmapper/ContainerBuilder.h"
#include "../objectmapper/ObjectMapperExceptions.h"
#include "../RDFException.h"
#include "../Debug.h"

#include "TestObjects.h"

#include <QStringList>
#include <QTimer>
#include <iostream>

#include <cassert>

using std::cerr;
using std::endl;

QDataStream &operator<<(QDataStream &out, StreamableValueType v) {
    return out << int(v);
}

QDataStream &operator>>(QDataStream &in, StreamableValueType &v) {
    int i;
    in >> i;
    v = StreamableValueType(i);
    return in;
}

namespace Dataquay
{
namespace Test
{

extern int
testQtWidgets(int argc, char **argv);

std::ostream &
operator<<(std::ostream &target, const QString &str)
{
    return target << str.toLocal8Bit().data();
}

void
dump(Triples t)
{
    for (Triples::const_iterator i = t.begin(); i != t.end(); ++i) {
        cerr << "(" << i->a.type() << ":" << i->a.value() << ") ";
        cerr << "(" << i->b.type() << ":" << i->b.value() << ") ";
        cerr << "(" << i->c.type() << ":" << i->c.value() << ") ";
        cerr << endl;
    }
}
    
bool
testBasicStore()
{
    BasicStore store;

    cerr << "testBasicStore starting..." << endl;

    for (int i = 0; i < 2; ++i) {

        bool haveBaseUri = (i == 1);

        if (!haveBaseUri) {
            cerr << "Starting tests without absolute base URI" << endl;
        } else {
            cerr << "Starting tests with absolute base URI" << endl;
            store.clear();
            store.setBaseUri(Uri("http://blather-de-hoop/"));
        }

        QString base = store.getBaseUri().toString();

        int count = 0;
        int fromFred = 0;
        int toAlice = 0;

        cerr << "Testing adding statements..." << endl;

        if (!store.add(Triple(Node(Node::URI, ":fred"),
                              Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                              Node(Node::Literal, "Fred Jenkins")))) {
            // Most actual failures are expressed in exceptions, but we
            // don't mind here if they just cause us to terminate
            cerr << "Failed to add triple to store (count = " << count << ")" << endl;
            return false;
        }

        ++count;
        ++fromFred;

        if (!store.contains(Triple(Node(Node::URI, ":fred"),
                                   Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                                   Node(Node::Literal, "Fred Jenkins")))) {
            cerr << "Failed to find triple in store immediately after adding it" << endl;
            return false;
        }

        if (store.contains(Triple(Node(Node::URI, ":fred"),
                                  Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                                  Node(Node::Literal, "Fred Johnson")))) {
            cerr << "Found nonexistent triple in store" << endl;
            return false;
        }

        // alternative Triple constructor
        if (!store.add(Triple(":fred",
                              "http://xmlns.com/foaf/0.1/knows",
                              Node(Node::URI, ":alice")))) {
            cerr << "Failed to add triple to store (count = " << count << ")" << endl;
            return false;
        }
    
        ++count;
        ++fromFred;
        ++toAlice;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":age",
                              Node::fromVariant(QVariant(42))))) {
            cerr << "Failed to add variant integer node to store" << endl;
        }

        ++count;
        ++fromFred;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":has_some_uri",
                              Node::fromVariant(QVariant::fromValue(Uri("http://breakfastquay.com/rdf/person/fred")))))) {
            cerr << "Failed to add variant URI to store" << endl;
            return false;
        }
    
        ++count;
        ++fromFred;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":has_some_local_uri",
                              Node::fromVariant(QVariant::fromValue(store.expand(":pootle")))))) {
            cerr << "Failed to add variant local URI to store" << endl;
            return false;
        }
    
        ++count;
        ++fromFred;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":likes_to_think_his_age_is",
                              Node::fromVariant(QVariant(21.9))))) {
            cerr << "Failed to add variant double node to store" << endl;
        }

        ++count;
        ++fromFred;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":is_sadly_deluded",
                              Node::fromVariant(true)))) {
            cerr << "Failed to add variant boolean node to store" << endl;
        }

        ++count;
        ++fromFred;

        // variant conversion
        QStringList colours;
        colours << "turquoise";
        colours << "red";
        colours << "black";
        assert(colours.size() == 3);
        if (!store.add(Triple(":fred",
                              ":favourite_colours_are",
                              Node::fromVariant(QVariant(colours))))) {
            cerr << "Failed to add variant string list node to store" << endl;
        }

        ++count;
        ++fromFred;

        // rdf:type builtin
        if (!store.add(Triple(":fred",
                              "a",
                              Node(Node::URI, ":person")))) {
            cerr << "Failed to add rdf:type builtin to store" << endl;
        }

        ++count;
        ++fromFred;

        store.addPrefix("foaf", Uri("http://xmlns.com/foaf/0.1/"));
    
        if (!store.add(Triple(Node(Node::URI, ":alice"),
                              Node(Node::URI, "foaf:knows"),
                              Node(Node::URI, ":fred")))) {
            cerr << "Failed to add triple to store (count = " << count << ")" << endl;
            return false;
        }
    
        ++count;
    
        if (!store.add(Triple(Node(Node::URI, ":alice"),
                              Node(Node::URI, "foaf:name"),
                              Node(Node::Literal, QString("Alice Banquet"))))) {
            cerr << "Failed to add triple to store (count = " << count << ")" << endl;
            return false;
        }
              
        ++count;
    
        // we try to add a triple that is a differently-expressed
        // duplicate of an already-added one -- this should be ignored
        // (returning false) and we do not increment our count
    
        if (store.add(Triple(base + "alice",
                             "http://xmlns.com/foaf/0.1/knows",
                             Node(Node::URI, ":fred")))) {
            cerr << "Error in adding duplicate triple -- store reported success, should have reported failure" << endl;
            return false;
        }

        // now we try to add a triple a bit like an existing one but
        // differing in prefix -- this should succeed, and we do increment
        // our count, although this is not a very useful statement
    
        if (!store.add(Triple(":alice",
                              "http://xmlns.com/foaf/0.1/knows",
                              Node(Node::URI, "foaf:fred")))) {
            cerr << "Failed to add triple to store (count = " << count << ")" << endl;
            return false;
        }

        ++count;

        // something involving blank nodes
        
        Node blankNode = store.addBlankNode();

        if (!store.add(Triple(":fred",
                              "http://xmlns.com/foaf/0.1/maker",
                              blankNode))) {
            cerr << "Failed to add triple containing final blank node to store" << endl;
            return false;
        }

        ++count;
        ++fromFred;

        if (!store.add(Triple(blankNode,
                              "foaf:name",
                              Node(Node::Literal, "Omnipotent Being")))) {
            cerr << "Failed to add triple containing initial blank node to store" << endl;
            return false;
        }

        ++count;

        Triples triples;

        cerr << "Testing matching..." << endl;

        triples = store.match(Triple());

        if (triples.size() != count) {
            cerr << "Failed to load and match triples: loaded " << count << " but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        triples = store.match(Triple(Node(Node::URI, ":fred"), Node(), Node()));
    
        if (triples.size() != fromFred) {
            cerr << "Failed to load and match triples: loaded " << fromFred << " with Fred as subject, but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        triples = store.match(Triple(Node(), Node(), Node(Node::URI, ":alice")));
    
        if (triples.size() != toAlice) {
            cerr << "Failed to load and match triples: loaded " << toAlice << " with Alice as object, but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        cerr << "Testing basic matching to variant type..." << endl;
        
        triples = store.match(Triple(Node(Node::URI, ":fred"),
                                     Node(Node::URI, ":age"),
                                     Node()));
        if (triples.size() != 1) {
            cerr << "Failed to query expected number of properties about Fred's age" << endl;
            return false;
        }
        int age = triples[0].c.toVariant().toInt();
        if (age != 42) {
            cerr << "Fred is lying about his age" << endl;
            return false;
        }
        
        triples = store.match(Triple(Node(Node::URI, ":fred"),
                                     Node(Node::URI, ":is_sadly_deluded"),
                                     Node()));
        if (triples.size() != 1) {
            cerr << "Failed to query expected number of properties about Fred's delusional qualities" << endl;
            return false;
        }
        bool deluded = triples[0].c.toVariant().toBool();
        if (!deluded) {
            cerr << "No really, Fred is deluded" << endl;
            return false;
        }

        triples = store.match(Triple(Node(Node::URI, ":fred"),
                                     Node(Node::URI, ":favourite_colours_are"),
                                     Node()));
        if (triples.size() != 1) {
            cerr << "Failed to query expected number of properties about Fred's favourite colours" << endl;
            return false;
        }
        colours = triples[0].c.toVariant().toStringList();
        if (colours.size() != 3) {
            cerr << "Fred seems to have changed his mind about how many colours he likes (was 3, is " << colours.size() << ")" << endl;
            if (colours.size() > 0) {
                cerr << "(first is " << colours[0] << ")" << endl;
            }
            return false;
        }
        if (colours[2] != "black") {
            cerr << "Fred has changed his mind about his favourite colours" << endl;
            return false;
        }
        
        triples = store.match(Triple(Node(Node::URI, ":fred"),
                                     Node(Node::URI, ":has_some_uri"),
                                     Node()));
        if (triples.size() != 1) {
            cerr << "Failed to query expected number of properties about Fred's some-uri-or-other" << endl;
            return false;
        }
        Uri someUri(triples[0].c.value());
        if (someUri.toString() != "http://breakfastquay.com/rdf/person/fred") {
            cerr << "Fred's some-uri-or-other is not what we expected (it is "
                 << someUri.toString() << ")" << endl;
            return false;
        }
        
        triples = store.match(Triple(Node(Node::URI, ":fred"),
                                     Node(Node::URI, ":has_some_local_uri"),
                                     Node()));
        if (triples.size() != 1) {
            cerr << "Failed to query expected number of properties about Fred's some-local-uri-or-other" << endl;
            return false;
        }
        someUri = Uri(triples[0].c.toVariant().value<Uri>());
        if (someUri != store.expand(":pootle")) {
            cerr << "Fred's some-local-uri-or-other is not what we expected (it is "
                 << someUri.toString() << ")" << endl;
            return false;
        }

        cerr << "Testing triples comparison..." << endl;

        triples = store.match(Triple(Node(Node::URI, ":fred"), Node(), Node()));
        Triples triples2 = store.match(Triple(Node(Node::URI, ":fred"), Node(), Node()));

        if (!triples.matches(triples2)) {
            cerr << "Triples returned from two identical matches fail to match" << endl;
            return false;
        }

        triples2 = Triples();
        foreach (Triple t, triples) triples2.push_front(t);
        
        if (!triples.matches(triples2)) {
            cerr << "Triples fail to match same triples in reverse order" << endl;
            return false;
        }

        triples2 = store.match(Triple(Node(Node::URI, ":alice"), Node(), Node()));

        if (triples.matches(triples2)) {
            cerr << "Triples returned from two different matches compare equal" << endl;
            return false;
        }

        cerr << "Testing file save..." << endl;

        store.save("test.ttl");

        QString q = QString(" SELECT ?a "
                            " WHERE { :fred foaf:knows ?a } ");
        ResultSet results;

        if (haveBaseUri) {

            // We cannot perform queries without an absolute base URI,
            // it seems, because the query engine takes relative URIs
            // as relative to the query URI and we can't override that
            // without an absolute prefix for the base URI

            cerr << "Testing query..." << endl;

            results = store.query(q);
            if (results.size() != 1) {
                cerr << "Query produced wrong number of results (expected 1, got " << results.size() << ")" << endl;
                cerr << "Query was: " << q << endl;
                return false;
            }

            Node v = store.queryFirst(q, "a");
            QString expected = base + "alice";
            if (v.type() != Node::URI || v.value() != expected) {
                cerr << "getFirstResult returned wrong result (expected URI of <" << expected << ">, got type " << v.type() << " and value " << v.value() << ")" << endl;
                return false;
            }
        }

        cerr << "Testing removing statements..." << endl;

        if (!store.remove(Triple(":fred",
                                 "http://xmlns.com/foaf/0.1/knows",
                                 Node(Node::URI, ":alice")))) {
            cerr << "Failed to remove triple from store" << endl;
            return false;
        }

        if (store.remove(Triple(":fred",
                                "http://xmlns.com/foaf/0.1/knows",
                                Node(Node::URI, ":tammy")))) {
            cerr << "Error: succeeded in removing triple from store that was not there" << endl;
            return false;
        }

        triples = store.match(Triple());

        if (triples.size() != count-1) {
            cerr << "Failed to match triples: loaded " << count << " then removed 1, but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        if (haveBaseUri) {
            results = store.query(q);
            if (results.size() != 0) {
                cerr << "Query (after removing statement) produced wrong number of results (expected 0, got " << results.size() << ")" << endl;
                cerr << "Query was: " << q << endl;
                return false;
            }
        }
    
        cerr << "Testing file load..." << endl;

        BasicStore *store2 = BasicStore::load(QUrl("file:test.ttl"));

        cerr << "Testing file re-save..." << endl;

        store2->save("test2.ttl");

        cerr << "Testing match on loaded store..." << endl;

        triples = store2->match(Triple());

        if (triples.size() != count) {
            cerr << "Failed to load and match triples after export/import: loaded " << count << " but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        triples = store2->match(Triple(Node(Node::URI, ":fred"), Node(), Node()));
    
        if (triples.size() != fromFred) {
            cerr << "Failed to load and match triples after export/import: loaded " << fromFred << " with Fred as subject, but matched " << triples.size() << endl;
            cerr << "They were:" << endl;
            dump(triples);
            return false;
        }

        if (haveBaseUri) {
            cerr << "Testing query on loaded store..." << endl;

            results = store2->query(q);
            if (results.size() != 1) {
                cerr << "Query (after export/import) produced wrong number of results (expected 1, got " << results.size() << ")" << endl;
                cerr << "Query was: " << q << endl;
                return false;
            }

            results = store.query(q);
            if (results.size() != 0) {
                cerr << "Query (on original store after prior import/export) produced wrong number of results (expected 0, got " << results.size() << ")" << endl;
                cerr << "Query was: " << q << endl;
                return false;
            }
        }
    
        delete store2;

        if (haveBaseUri) {
            cerr << "Testing import into original store..." << endl;

            store.import(QUrl("file:test2.ttl"),
                         BasicStore::ImportIgnoreDuplicates);

            results = store.query(q);
            if (results.size() != 1) {
                cerr << "Query (on original store after importing from additional store) produced wrong number of results (expected 1, got " << results.size() << ")" << endl;
                cerr << "Query was: " << q << endl;
                return false;
            }
        }
    }

    std::cerr << "testBasicStore done" << std::endl;
    return true;
}
    
bool
testDatatypes()
{
    BasicStore store;

    cerr << "testDatatypes starting..." << endl;

    // Untyped literal should convert to QString

    cerr << "Testing basic conversions..." << endl;

    Node n(Node::Literal, "Fred Jenkins", Uri());

    QVariant v = n.toVariant();
    if (v.userType() != QMetaType::QString) {
        cerr << "Plain literal node converted to variant has unexpected type "
             << v.userType() << " (expected " << QMetaType::QString << ")" << endl;
        return false;
    }

    Node n0 = Node::fromVariant(v);
    if (n0.datatype() != n.datatype()) {
        cerr << "String variant converted to node has unexpected non-nil datatype "
             << n0.datatype() << endl;
        return false;
    }

    Triple t(store.expand(":fred"),
             Uri("http://xmlns.com/foaf/0.1/name"),
             n);

    if (!store.add(t)) {
        cerr << "Failed to add string-type triple to store" << endl;
        return false;
    }

    t.c = Node();
    Triple t0(store.matchFirst(t));
    n0 = t.c;
    if (n0.datatype() != n.datatype()) {
        cerr << "Failed to retrieve expected nil datatype" << endl;
        return false;
    }

    n = Node(Node::Literal, "1", store.expand("xsd:integer"));

    v = n.toVariant();
    if (v.userType() != QMetaType::Long) {
        cerr << "Integer literal node converted to variant has unexpected type "
             << v.userType() << " (expected " << QMetaType::Long << ")" << endl;
        return false;
    }

    n0 = Node::fromVariant(v);
    if (n0.datatype() != n.datatype()) {
        cerr << "Long integer variant converted to node has unexpected datatype "
             << n0.datatype().toString().toStdString() << " (expected " << n.datatype().toString().toStdString() << ")" << endl;
        return false;
    }

    t = Triple(store.expand(":fred"),
               store.expand(":number_of_jobs"),
               n);
    
    if (!store.add(t)) {
        cerr << "Failed to add int-type triple to store" << endl;
        return false;
    }

    t.c = Node();
    t0 = store.matchFirst(t);
    n0 = t0.c;
    if (n0.datatype() != n.datatype()) {
        cerr << "Failed to retrieve expected integer datatype (found instead <"
             << n0.datatype().toString().toStdString() << "> for value \""
             << n0.value().toStdString() << "\")" << endl;
        return false;
    }

    StreamableValueType sv = ValueC;
    NonStreamableValueType nsv = ValueJ;
    qRegisterMetaType<StreamableValueType>("StreamableValueType");
    qRegisterMetaType<NonStreamableValueType>("NonStreamableValueType");

    // first some tests on basic QVariant storage just to ensure we've
    // registered the type correctly and our understanding of expected
    // behaviour is correct

    cerr << "Testing streamable-value to variant conversion..." << endl;

    if (QMetaType::type("StreamableValueType") <= 0) {
        cerr << "No QMetaType registration for StreamableValueType? Type lookup returns " << QMetaType::type("StreamableValueType") << endl;
        return false;
    }

    QVariant svv = QVariant::fromValue<StreamableValueType>(sv);
    if (svv.userType() != QMetaType::type("StreamableValueType")) {
        cerr << "QVariant does not contain expected type " << QMetaType::type("StreamableValueType") << " for StreamableValueType (found instead " << svv.userType() << ")" << endl;
        return false;
    }

    if (svv.value<StreamableValueType>() != sv) {
        cerr << "QVariant does not contain expected value " << sv << " for StreamableValueType (found instead " << svv.value<StreamableValueType>() << ")" << endl;
        return false;
    }

    // Now register the stream operators necessary to permit
    // Node-QVariant conversion using an opaque "unknown type" node
    // datatype and the standard QVariant datastream streaming; then
    // test conversion that way

    cerr << "Testing streamable-value (without encoder, so unknown type) to Node conversion..." << endl;

    qRegisterMetaTypeStreamOperators<StreamableValueType>("StreamableValueType");

    n = Node::fromVariant(svv);

//!!! no -- the type is actually encodedVariantTypeURI from Node.cpp, and this is what we should expect -- but it's not public! we can't test it -- fix this
//    if (n.datatype() != Uri()) {
//        cerr << "Node converted from unknown type has unexpected datatype <" 
//             << n.datatype().toString().toStdString() << "> (expected nil)" << endl;
//        return false;
//    }

    t = Triple(store.expand(":fred"),
               store.expand(":has_some_value"),
               n);
    
    if (!store.add(t)) {
        cerr << "Failed to add unknown-type triple to store" << endl;
        return false;
    }

    t.c = Node();
    t0 = store.matchFirst(t);
    n0 = t0.c;
    if (n0.datatype() != n.datatype()) {
        cerr << "Failed to retrieve expected unknown-type datatype (found instead <"
             << n0.datatype().toString().toStdString() << "> for value \""
             << n0.value().toStdString() << "\")" << endl;
        return false;
    }

    QVariant v0 = n0.toVariant();
    if (v0.userType() != svv.userType() ||
        v0.value<StreamableValueType>() != svv.value<StreamableValueType>()) {
        cerr << "Conversion of unknown-type node back to variant yielded unexpected value " << v0.value<StreamableValueType>() << " of type " << v0.userType() << " (expected " << svv.value<StreamableValueType>() << " of type " << svv.userType() << ")" << endl;
        return false;
    }

    // Now we do the whole thing again using NonStreamableValueType
    // with a registered encoder, rather than StreamableValueType with
    // no encoder but a stream operator instead

    cerr << "Testing non-streamable-value (with encoder and datatype) to Node conversion..." << endl;

    QVariant nsvv = QVariant::fromValue<NonStreamableValueType>(nsv);
    if (nsvv.userType() != QMetaType::type("NonStreamableValueType")) {
        cerr << "QVariant does not contain expected type " << QMetaType::type("NonStreamableValueType") << " for NonStreamableValueType (found instead " << nsvv.userType() << ")" << endl;
        return false;
    }

    if (nsvv.value<NonStreamableValueType>() != nsv) {
        cerr << "QVariant does not contain expected value " << nsv << " for NonStreamableValueType (found instead " << nsvv.value<NonStreamableValueType>() << ")" << endl;
        return false;
    }

    struct NonStreamableEncoder : public Node::VariantEncoder {
        QVariant toVariant(const QString &s) {
            if (s == "F") return QVariant::fromValue(ValueF);
            else if (s == "G") return QVariant::fromValue(ValueG);
            else if (s == "H") return QVariant::fromValue(ValueH);
            else if (s == "I") return QVariant::fromValue(ValueI);
            else if (s == "J") return QVariant::fromValue(ValueJ);
            else return QVariant();
        }
        QString fromVariant(const QVariant &v) {
            NonStreamableValueType nsv = v.value<NonStreamableValueType>();
            switch (nsv) {
            case ValueF: return "F";
            case ValueG: return "G";
            case ValueH: return "H";
            case ValueI: return "I";
            case ValueJ: return "J";
            default: return "";
            }
        }
    };

    Uri nsvDtUri("http://blather-de-hoop/nonstreamable");

    Node::registerDatatype(nsvDtUri,
                           "NonStreamableValueType",
                           new NonStreamableEncoder());

    n = Node::fromVariant(nsvv);

    if (n.datatype() != nsvDtUri) {
        cerr << "Node converted from custom type has unexpected datatype <" 
             << n.datatype().toString().toStdString() << "> (expected <"
             << nsvDtUri.toString().toStdString() << ")" << endl;
        return false;
    }

    t = Triple(store.expand(":fred"),
               store.expand(":has_some_other_value"),
               n);
    
    if (!store.add(t)) {
        cerr << "Failed to add custom-type triple to store" << endl;
        return false;
    }

    t.c = Node();
    t0 = store.matchFirst(t);
    n0 = t0.c;
    if (n0.datatype() != n.datatype()) {
        cerr << "Failed to retrieve expected custom-type datatype (found instead <"
             << n0.datatype().toString().toStdString() << "> for value \""
             << n0.value().toStdString() << "\")" << endl;
        return false;
    }

    v0 = n0.toVariant();
    if (v0.userType() != nsvv.userType() ||
        v0.value<NonStreamableValueType>() != nsvv.value<NonStreamableValueType>()) {
        cerr << "Conversion of unknown-type node back to variant yielded unexpected value " << v0.value<NonStreamableValueType>() << " of type " << v0.userType() << " (expected " << nsvv.value<NonStreamableValueType>() << " of type " << nsvv.userType() << ")" << endl;
        return false;
    }

    // also means to retrieve node as particular variant type even
    // when node datatype is missing.  Just setting the datatype on
    // the node isn't ideal, since we'd have to do it in more than one
    // step (i.e. looking up what the datatype was supposed to be)

    n0.setDatatype(Uri());

    // first test without prompting
    v0 = n0.toVariant();
    if (v0.userType() != QMetaType::QString) {
        cerr << "Conversion of unknown-type node with no datatype back to variant yielded unexpected type " << v0.userType() << " (expected " << QMetaType::QString << " for plain string variant)" << endl;
        return false;
    }

    // now with
    v0 = n0.toVariant(QMetaType::type("NonStreamableValueType"));
    if (v0.userType() != nsvv.userType() ||
        v0.value<NonStreamableValueType>() != nsvv.value<NonStreamableValueType>()) {
        cerr << "Conversion of unknown-type node with no datatype but prompted type back to variant yielded unexpected value " << v0.value<NonStreamableValueType>() << " of type " << v0.userType() << " (expected " << nsvv.value<NonStreamableValueType>() << " of type " << nsvv.userType() << ")" << endl;
        return false;
    }

    return true;
}
  
bool
testImportOptions()
{
    BasicStore store;

    cerr << "testImportOptions starting..." << endl;

    store.setBaseUri(Uri("http://blather-de-hoop/"));

    QString base = store.getBaseUri().toString();

    int count = 0;

    if (!store.add(Triple(Node(Node::URI, ":fred"),
                          Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                          Node(Node::Literal, "Fred Jenkins")))) {
        cerr << "Failed to add triple to store (count = " << count << ")" << endl;
        return false;
    }
    ++count;

    if (!store.add(Triple(":fred",
                          ":age",
                          Node::fromVariant(QVariant(42))))) {
        cerr << "Failed to add variant integer node to store" << endl;
    }
    ++count;

    if (!store.add(Triple(":fred",
                          ":has_some_uri",
                          Node::fromVariant(QVariant::fromValue(Uri("http://breakfastquay.com/rdf/person/fred")))))) {
        cerr << "Failed to add variant URI to store" << endl;
    }
    ++count;

    if (!store.add(Triple(":fred",
                          ":has_some_local_uri",
                          Node::fromVariant(QVariant::fromValue(store.expand(":pootle")))))) {
        cerr << "Failed to add variant local URI to store" << endl;
    }
    ++count;

    if (!store.add(Triple(":fred",
                          ":likes_to_think_his_age_is",
                          Node::fromVariant(QVariant(21.9))))) {
        cerr << "Failed to add variant double node to store" << endl;
    }
    ++count;

    store.save("test3.ttl");
    
    bool caught = false;
    try {
        store.import(QUrl("file:test3.ttl"), BasicStore::ImportFailOnDuplicates);
    } catch (RDFDuplicateImportException) {
        cerr << "Correctly caught RDFDuplicateImportException when importing duplicate store" << endl;
        caught = true;
    }
    if (!caught) {
        cerr << "Failed to get RDFDuplicateImportException when importing duplicate store" << endl;
        return false;
    }

    Triples triples = store.match(Triple());
    if (triples.size() != count) {
        cerr << "Wrong number of triples in store after failed import: expected " << count << ", found " << triples.size() << endl;
        return false;
    }

    try {
        store.import(QUrl("file:test3.ttl"), BasicStore::ImportPermitDuplicates);
    } catch (RDFDuplicateImportException) {
        cerr << "Incorrectly caught RDFDuplicateImportException when importing duplicate store with duplicates permitted" << endl;
        return false;
    }

    triples = store.match(Triple());
    cerr << "Note: Imported " << count << " triples twice with duplicates permitted, query then returned " << triples.size() << endl;

    store.clear();

    try {
        store.import(QUrl("file:test3.ttl"), BasicStore::ImportIgnoreDuplicates);
        store.import(QUrl("file:test3.ttl"), BasicStore::ImportIgnoreDuplicates);
    } catch (RDFDuplicateImportException) {
        cerr << "Wrongly caught RDFDuplicateImportException when importing duplicate store with ImportIgnoreDuplicates" << endl;
        return false;
    }

    triples = store.match(Triple());
    if (triples.size() != count) {
        cerr << "Wrong number of triples in store after failed import: expected " << count << ", found " << triples.size() << endl;
        return false;
    }

    return true;
}
    
bool
testTransactionalStore()
{
    BasicStore store;

    cerr << "testTransactionalStore starting..." << endl;

    store.clear();
    store.setBaseUri(Uri("http://blather-de-hoop/"));

    TransactionalStore ts(&store);

    Triples triples;
    int n;

    Transaction *t = ts.startTransaction();
    int added = 0;

    // These add calls are all things we've tested in testBasicStore already
    t->add(Triple(Node(Node::URI, ":fred"),
                  Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                  Node(Node::Literal, "Fred Jenkins")));
    ++added;
    t->add(Triple(":fred",
                  "http://xmlns.com/foaf/0.1/knows",
                  Node(Node::URI, ":alice")));
    ++added;
    t->add(Triple(":fred",
                  ":age",
                  Node::fromVariant(QVariant(43))));
    ++added;
    t->remove(Triple(":fred",
                     ":age",
                     Node()));
    --added;
    t->add(Triple(":fred",
                  ":age",
                  Node::fromVariant(QVariant(42))));
    ++added;

    // pause to test transactional isolation
    triples = ts.match(Triple());
    n = triples.size();
    if (n != 0) {
        cerr << "Transactional isolation failure -- match() during initial add returned " << n << " results (should have been 0)" << endl;
        return false;
    }

    // do it again, just to check internal state isn't being bungled
    triples = ts.match(Triple());
    n = triples.size();
    if (n != 0) {
        cerr << "Transactional isolation failure -- second match() during initial add returned " << n << " results (should have been 0)" << endl;
        return false;
    }

    // and check that reads *through* the transaction return the
    // partial state as expected
    triples = t->match(Triple());
    n = triples.size();
    if (n != added) {
        cerr << "Transactional failure -- match() within initial transaction returned " << n << " results (should have been " << added << ")" << endl;
        return false;
    }

    t->add(Triple(":fred",
                  ":likes_to_think_his_age_is",
                  Node::fromVariant(QVariant(21.9))));
    ++added;
    t->add(Triple(":fred",
                  ":is_sadly_deluded",
                  Node::fromVariant(true)));
    ++added;

    // Get the changes that are about to be committed.  We'll use this
    // for a revert test later
    ChangeSet changes = t->getChanges();

    // but also check whether this is empty (it should be)
    ChangeSet changes2 = t->getCommittedChanges();
    if (!changes2.empty()) {
        cerr << "Transactional failure -- getCommittedChanges() within initial transaction returned " << changes2.size() << " changes (should have been empty)" << endl;
        return false;
    }
    
    t->commit();

    // now this should be the same as our getChanges call
    changes2 = t->getCommittedChanges();
    if (changes2.empty()) {
        cerr << "Transactional failure -- getCommittedChanges() after commit returned empty change set" << endl;
        return false;
    }
    if (changes2 != changes) {
        cerr << "Transactional failure -- getCommittedChanges() after commit returned different change set from getChanges() immediately before commit (" << changes2.size() << " vs " << changes.size() << " changes)" << endl;
        return false;
    }

    // Test that we can't use the transaction after a commit

    bool caught = false;
    try {
        t->add(Triple(":fred2",
                      "http://xmlns.com/foaf/0.1/knows",
                      Node(Node::URI, ":samuel")));
    } catch (RDFException) {
        cerr << "Correctly caught RDFException when trying to use a transaction after committing it" << endl;
        caught = true;
    }
    if (!caught) {
        cerr << "Failed to get RDFException when trying to use a transaction after committing it" << endl;
        return false;
    }

    delete t;

    triples = ts.match(Triple());
    n = triples.size();
    if (n != added) {
        cerr << "Store add didn't take at all (triples matched on query != expected " << added << ")" << endl;
        return false;
    }

    t = ts.startTransaction();
    t->revert(changes);
    t->commit();
    delete t;

    triples = ts.match(Triple());
    if (triples.size() > 0) {
        cerr << "Store revert failed (store is not empty)" << endl;
        return false;
    }

    t = ts.startTransaction();
    t->change(changes);
    t->commit();
    delete t;

    triples = ts.match(Triple());
    if (triples.size() != n) {
        cerr << "Store re-change failed (triple count " << triples.size() << " != original " << n << ")" << endl;
        return false;
    }

    
    if (ts.contains(Triple(":fred",
                           ":age",
                           Node::fromVariant(QVariant(43))))) {
        cerr << "Store re-change resulted in incorrect age for Fred?" << endl;
        return false;
    }
    
    if (!ts.contains(Triple(":fred",
                            ":age",
                            Node::fromVariant(QVariant(42))))) {
        cerr << "Store re-change resulted in lack of proper age for Fred?" << endl;
        return false;
    }

    
    // Test that we can't delete a transaction that has been used
    // without committing it or rolling back
        
    t = ts.startTransaction();

    t->add(Triple(":fred2",
                  "http://xmlns.com/foaf/0.1/knows",
                  Node(Node::URI, ":samuel")));
    
    caught = false;
    try {
        delete t;
    } catch (RDFException) {
        cerr << "Correctly caught RDFException when trying to delete a used but uncommitted/unrolled-back transaction" << endl;
        caught = true;
    }
    if (!caught) {
        cerr << "Failed to get RDFException when trying to delete a used but uncommitted/unrolled-back transaction" << endl;
        return false;
    }


    // Test explicit rollback

    t = ts.startTransaction();

    t->add(Triple(Node(Node::URI, ":fred2"),
                  Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                  Node(Node::Literal, "Fred Jenkins")));
    t->add(Triple(":fred2",
                  "http://xmlns.com/foaf/0.1/knows",
                  Node(Node::URI, ":alice")));
    t->add(Triple(":fred2",
                  ":age",
                  Node::fromVariant(QVariant(42))));
    t->rollback();
    int exceptionCount = 0;
    int expectedExceptions = 0;
    try {
        t->add(Triple(":fred2",
                      ":likes_to_think_his_age_is",
                      Node::fromVariant(QVariant(21.9))));
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    try {
        t->add(Triple(":fred2",
                      ":is_sadly_deluded",
                      Node::fromVariant(true)));
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    delete t;

    triples = ts.match(Triple());
    n = triples.size();
    if (n != added) {
        cerr << "Transactional rollback failed (" << n << " triples matched on query != expected " << added << " left from previous transaction)" << endl;
        return false;
    }

    if (exceptionCount != expectedExceptions) {
        cerr << "Transaction failed to throw expected exceptions when used after rollback" << endl;
        return false;
    }
    

    // Test automatic rollback triggered by exception

    t = ts.startTransaction();

    t->add(Triple(Node(Node::URI, ":fred2"),
                  Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                  Node(Node::Literal, "Fred Jenkins")));
    t->add(Triple(":fred2",
                  "http://xmlns.com/foaf/0.1/knows",
                  Node(Node::URI, ":alice")));
    t->add(Triple(":fred2",
                  ":age",
                  Node::fromVariant(QVariant(42))));
    exceptionCount = 0;
    expectedExceptions = 0;
    // Add incomplete statement to provoke an exception
    try {
        t->add(Triple(Node(),
                      Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                      Node(Node::Literal, "The Man Who Wasn't There")));
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    try {
        t->add(Triple(":fred2",
                      ":likes_to_think_his_age_is",
                      Node::fromVariant(QVariant(21.9))));
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    try {
        t->add(Triple(":fred2",
                      ":is_sadly_deluded",
                      Node::fromVariant(true)));
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    try {
        t->commit();
    } catch (RDFException) {
        ++exceptionCount;
    }
    ++expectedExceptions;
    delete t;

    triples = ts.match(Triple());
    n = triples.size();
    if (n != added) {
        cerr << "Auto-rollback on failed transaction failed (" << n << " triples matched on query != expected " << added << " left from previous transaction)" << endl;
        return false;
    }

    if (exceptionCount != expectedExceptions) {
        cerr << "Transaction failed to throw expected exceptions when used after auto-rollback" << endl;
        return false;
    }
    
    

    std::cerr << "testTransactionalStore done" << std::endl;
    return true;
}

bool
testConnection()
{
    BasicStore store;

    cerr << "testConnection starting..." << endl;

    store.clear();
    store.setBaseUri(Uri("http://blather-de-hoop/"));

    TransactionalStore ts(&store);
    
    // We really need to test many simultaneous connections from
    // multiple threads in order to make this worthwhile... oh well

    {
        Connection c(&ts);
    
        Triples triples;
        int n;
        int added = 0;

        c.add(Triple(Node(Node::URI, ":fred"),
                     Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
                     Node(Node::Literal, "Fred Jenkins")));
        ++added;
        c.add(Triple(":fred",
                     "http://xmlns.com/foaf/0.1/knows",
                     Node(Node::URI, ":alice")));
        ++added;
        c.add(Triple(":fred",
                     ":age",
                     Node::fromVariant(QVariant(43))));
        ++added;
        c.remove(Triple(":fred",
                        ":age",
                        Node()));
        --added;
        c.add(Triple(":fred",
                     ":age",
                     Node::fromVariant(QVariant(42))));
        ++added;

        // query on connection
        triples = c.match(Triple());
        n = triples.size();
        if (n != added) {
            cerr << "Transactional failure -- match() within connection transaction returned " << n << " results (should have been " << added << ")" << endl;
            return false;
        }

        // query on store
        triples = ts.match(Triple());
        n = triples.size();
        if (n != 0) {
            cerr << "Transactional isolation failure -- match() during initial add returned " << n << " results (should have been 0)" << endl;
            return false;
        }

        c.add(Triple(":fred",
                     ":likes_to_think_his_age_is",
                     Node::fromVariant(QVariant(21.9))));
        ++added;
        c.add(Triple(":fred",
                     ":is_sadly_deluded",
                     Node::fromVariant(true)));
        ++added;

        c.commit();

        triples = ts.match(Triple());
        n = triples.size();
        if (n != added) {
            cerr << "Transactional failure -- match() after connection commit complete returned " << n << " results (should have been " << added << ")" << endl;
            return false;
        }

        // test implicit commit on dtor
        for (int i = 0; i < triples.size(); ++i) {
            c.remove(triples[i]);
        }
    }
    Triples triples = ts.match(Triple());
    int n = triples.size();
    if (n != 0) {
        cerr << "Connection failure -- match() after remove and implicit commit on close returned " << n << " results (should have been none)" << endl;
        return false;
    }

    std::cerr << "testConnection done" << std::endl;
    return true;
}

bool
compare(Triples t1, Triples t2)
{
    if (t1 == t2) return true;

    QMap<Triple, int> s1;
    QMap<Triple, int> s2;
    foreach (Triple t, t1) {
        if (t.a.type() == Node::Blank) t.a.setValue("");
        if (t.c.type() == Node::Blank) t.c.setValue("");
        s1[t] = 1;
    }
    foreach (Triple t, t2) {
        if (t.a.type() == Node::Blank) t.a.setValue("");
        if (t.c.type() == Node::Blank) t.c.setValue("");
        s2[t] = 1;
    }
    if (s1 == s2) return true;

    cerr << "Two stored versions of the same object tree differ:" << endl;
/*
            cerr << "\nFirst version:" << endl;
            foreach (Triple t, s1.keys()) { cerr << t << endl; }
            cerr << "\nSecond version:" << endl;
            foreach (Triple t, s2.keys()) { cerr << t << endl; }
*/
    cerr << "\nIn first but not in second:" << endl;
    foreach (Triple t, s1.keys()) { if (!s2.contains(t)) cerr << t << endl; }
    cerr << "\nIn second but not in first:" << endl;
    foreach (Triple t, s2.keys()) { if (!s1.contains(t)) cerr << t << endl; }
    return false;
}
    
bool
testReloadability(BasicStore &s0)
{
    cerr << "Testing reload consistency..." << endl;

    s0.save("test-reloadability-prior.ttl");

    ObjectLoader m0(&s0);
//    QObject *parent = m0.loadAllObjects(0);
    QObjectList objects = m0.loadAll();

    BasicStore s1;
    ObjectStorer m1(&s1);
    //   m1.setPropertyStorePolicy(ObjectStorer::StoreIfChanged);
    m1.setFollowPolicy(ObjectStorer::FollowChildren |
                       ObjectStorer::FollowSiblings |
                       ObjectStorer::FollowParent |
                       ObjectStorer::FollowObjectProperties);
    m1.store(objects);
    s1.addPrefix("type", m1.getTypeMapping().getObjectTypePrefix());
    s1.addPrefix("property", m1.getTypeMapping().getPropertyPrefix());
    s1.addPrefix("rel", m1.getTypeMapping().getRelationshipPrefix());
    s1.save("test-reloadability-a.ttl");

    ObjectLoader o1(&s1);
    o1.setFollowPolicy(ObjectLoader::FollowSiblings); //!!! doesn't mean what we think it should -- c.f. ObjectLoader::D::load()
//    QObject *newParent = o1.loadAllObjects(0);
    QObjectList newObjects = o1.loadAll();

    {
        Triples t;
        t = s0.match(Triple());
        cerr << "(Note: original store has " << t.size() << " triples of which ";
        t = s0.match(Triple(Node(), "a", Node()));
        cerr << t.size() << " are type nodes;" << endl;
        t = s1.match(Triple());
        cerr << "saved store has " << t.size() << " triples of which ";
        t = s1.match(Triple(Node(), "a", Node()));
        cerr << t.size() << " are type nodes)" << endl;
    }

    if (newObjects.size() != objects.size()) {
        cerr << "Reloaded object list has " << newObjects.size()
             << " children, expected " << objects.size()
             << " to match prior copy" << endl;
        return false;
    }

    BasicStore s2;
    ObjectStorer m2(&s2);
    m2.setFollowPolicy(m1.getFollowPolicy());
    m2.store(newObjects);

    s2.addPrefix("type", m2.getTypeMapping().getObjectTypePrefix());
    s2.addPrefix("property", m2.getTypeMapping().getPropertyPrefix());
    s2.addPrefix("rel", m2.getTypeMapping().getRelationshipPrefix());
    s2.save("test-reloadability-b.ttl");

    Triples t1 = s1.match(Triple());
    Triples t2 = s2.match(Triple());
    if (!compare(t1, t2)) {
        return false;
    }

    //!!! test whether s1 and s2 are equal and whether newParent has all the same children as parent
    return true;
}

bool 
testObjectMapper()
{
    cerr << "testObjectMapper starting..." << endl;

    cerr << "Testing simple QObject store and recall..." << endl;

    BasicStore store;
    
    ObjectStorer storer(&store);
    storer.setPropertyStorePolicy(ObjectStorer::StoreIfChanged);
    store.addPrefix("type", storer.getTypeMapping().getObjectTypePrefix());
    store.addPrefix("property", storer.getTypeMapping().getPropertyPrefix());
    store.addPrefix("rel", storer.getTypeMapping().getRelationshipPrefix());
    
    QObject *o = new QObject;
    o->setObjectName("Test Object");

    Uri uri = storer.store(o);
    cerr << "Stored QObject as " << uri << endl;

    ObjectLoader loader(&store);
    QObject *recalled = loader.load(uri);
    if (!recalled) {
        cerr << "Failed to recall object" << endl;
        return false;
    }
    if (recalled->objectName() != o->objectName()) {
        cerr << "Wrong object name recalled" << endl;
        return false;
    }

    if (!testReloadability(store)) {
        cerr << "Reloadability test for simple QObject store and recall failed" 
             << endl;
        return false;
    }

    cerr << "Testing QTimer store..." << endl;

    QTimer *t = new QTimer(o);
    t->setSingleShot(true);
    t->setInterval(4);

    Uri turi = storer.store(t);
    cerr << "Stored QTimer as " << turi << endl;

    cerr << "Testing QTimer recall without registration..." << endl;

    bool caught = false;
    try {
        recalled = loader.load(turi);
    } catch (UnknownTypeException) {
        cerr << "Correctly caught UnknownTypeException when trying to recall unregistered object" << endl;
        caught = true;
    }
    if (!caught) {
        cerr << "Expected UnknownTypeException when trying to recall unregistered object did not materialise" << endl;
        return false;
    }

    cerr << "Testing QTimer recall after registration..." << endl;

    ObjectBuilder::getInstance()->registerClass<QTimer, QObject>();

    recalled = loader.load(turi);
    if (!recalled) {
        cerr << "Failed to recall object" << endl;
        return false;
    }
    if (recalled->objectName() != t->objectName()) {
        cerr << "Wrong object name recalled" << endl;
        return false;
    }
    if (!qobject_cast<QTimer *>(recalled)) {
        cerr << "QTimer not a QTimer after recall" << endl;
        return false;
    }

    if (!testReloadability(store)) {
        cerr << "Reloadability test for QObject and QTimer store and recall failed" 
             << endl;
        return false;
    }

    cerr << "Testing custom object store and recall..." << endl;

    qRegisterMetaType<A*>("A*");
    qRegisterMetaType<B*>("B*");
    qRegisterMetaType<C*>("C*");
    qRegisterMetaType<QList<float> >("QList<float>");
    qRegisterMetaType<QList<B*> >("QList<B*>");
    qRegisterMetaType<QSet<C*> >("QSet<C*>");
    qRegisterMetaType<QObjectList>("QObjectList");
    ObjectBuilder::getInstance()->registerClass<A, QObject>("A*");
    ObjectBuilder::getInstance()->registerClass<B, QObject>("B*");
    ObjectBuilder::getInstance()->registerClass<C, QObject>("C*");

    ContainerBuilder::getInstance()->registerContainer<A*, QList<A*> >("A*", "QList<A*>", ContainerBuilder::SequenceKind);
    ContainerBuilder::getInstance()->registerContainer<B*, QList<B*> >("B*", "QList<B*>", ContainerBuilder::SequenceKind);
    ContainerBuilder::getInstance()->registerContainer<C*, QList<C*> >("C*", "QList<C*>", ContainerBuilder::SequenceKind);
    ContainerBuilder::getInstance()->registerContainer<QObject*, QObjectList>("QObject*", "QObjectList", ContainerBuilder::SequenceKind);
    ContainerBuilder::getInstance()->registerContainer<float, QList<float> >("float", "QList<float>", ContainerBuilder::SequenceKind);
    ContainerBuilder::getInstance()->registerContainer<C *, QSet<C *> >("C*", "QSet<C*>", ContainerBuilder::SetKind);

    cerr << "Testing single custom object store..." << endl;

    A *a = new A(o);
    a->setProperty("uri", QVariant::fromValue<Uri>(Uri(":A_first_with_timer")));
    a->setRef(t);
    Uri auri = storer.store(a);
    cerr << "Stored A-object as " << auri << endl;

    cerr << "Testing single custom object recall..." << endl;

    recalled = loader.load(auri);
    if (!recalled) {
        cerr << "Failed to recall A-object" << endl;
        return false;
    }
    if (recalled->objectName() != a->objectName()) {
        cerr << "Wrong A-object name recalled" << endl;
        return false;
    }
    if (!qobject_cast<A *>(recalled)) {
        cerr << "A-object not an A-object after recall" << endl;
        return false;
    }

    store.save("test-object-mapper.ttl");
    cerr << "Saved single custom object example to test-object-mapper.ttl" << endl;

    cerr << "Testing object network store and recall..." << endl;
    
    B *b = new B(o);
    // This B must have neither object name nor URI, it's checked for later
    b->setA(a);

    bool circular = true;

    C *c = new C;
    c->setObjectName("C with strings and floats and other Cs");
    QStringList strings;
    strings << "First string";
    strings << "Second string";
    c->setStrings(strings);
    QList<float> floats;
    floats << 1.f;
    floats << 2.f;
    floats << 3.f;
    floats << 4.f;
    c->setFloats(floats);
    QList<B *> blist;
    B *b0 = new B;
    cerr << "(Including circular graph test)" << endl;
    b0->setA(a);
    b0->setObjectName("b0");
    B *b1 = new B;
    A *a1 = new A;
    a1->setObjectName("a1");
    b1->setA(a1);
    b1->setObjectName("b1");
    B *b2 = new B;
    b2->setObjectName("b2");
    blist << b0 << b1;
    c->setBees(blist);
    QSet<C *> cset;
    C *c1 = new C;
    c1->setObjectName("c1");
    C *c2 = new C;
    c2->setObjectName("c2");
    cset.insert(c1);
    cset.insert(c2);
    if (circular) {
        cset.insert(c);
    }
    c->setCees(cset);
    QObjectList ol;
    ol << b2;
    c->setObjects(ol);
    a->setRef(c);

    storer.setFollowPolicy(ObjectStorer::FollowObjectProperties |
                           ObjectStorer::FollowChildren);
    ObjectStorer::ObjectNodeMap map;
    storer.store(o, map);

    store.save("test-object-mapper-2.ttl");

    // We should have:
    //
    // - one object with URI, one property, of type:QObject
    // - one object with URI, two? three? properties, and parent, of type:QTimer
    // - one object with URI, one property, follows, and parent, of type:A
    // - one blank node of type:A
    // - three blank nodes of type:C
    // - four blank nodes of type:B, one of which has parent
    //
    // We will check existence (though not qualities) of all the
    // above.  First though we do a quick count of type triples, then
    // store again using the same object-node map, then check the
    // results match (in case the storer is generating new URIs or
    // blank nodes for things that already have them in the map).

    Triples test = store.match(Triple(Node(), "a", Node()));
    if (test.size() != 11) {
        cerr << "Wrong number of type triples in store (found " << test.size() << ", expected 11)" << endl;
        return false;
    }

    cerr << "Note: object C (" << c << ") has " << c->getBees().size() << " Bs" << endl;

    storer.store(o, map);

    store.save("test-object-mapper-3.ttl");
    
    test = store.match(Triple(Node(), "a", Node()));
    if (test.size() != 11) {
        cerr << "Wrong number of type triples in store after re-store (found " << test.size() << ", expected 11)" << endl;
        cerr << "Found:" << endl;
        for (int i = 0; i < test.size(); ++i) {
            cerr << test[i] << endl;
        }
        return false;
    }

    // Now the rest of the content tests

    test = store.match(Triple(Node(), "a", store.expand("type:QObject")));
    if (test.size() != 1) {
        cerr << "Wrong number of QObject type nodes in store (found " << test.size() << ", expected 1)" << endl;
        return false;
    }
    if (test[0].a.type() != Node::URI) {
        cerr << "QObject type node in store lacks URI (node is " << test[0].a << ")" << endl;
        return false;
    }

    test = store.match(Triple(Node(), "a", store.expand("type:QTimer")));
    if (test.size() != 1) {
        cerr << "Wrong number of QTimer type nodes in store (found " << test.size() << ", expected 1)" << endl;
        return false;
    }
    if (test[0].a.type() != Node::URI) {
        cerr << "QTimer type node in store lacks URI (node is " << test[0].a << ")" << endl;
        return false;
    }

    test = store.match(Triple(test[0].a, "rel:parent", Node()));
    if (test.size() != 1) {
        cerr << "Wrong number of parents for QTimer in store (found " << test.size() << ", expected 1)" << endl;
        return false;
    }
    if (test[0].c.type() != Node::URI) {
        cerr << "Parent of QTimer in store lacks URI (node is " << test[0].c << ")" << endl;
        return false;
    }
    
    test = store.match(Triple(Node(), "rel:parent", test[0].c));
    if (test.size() != 3) {
        cerr << "Wrong number of children for parent object in store (found "
             << test.size() << ", expected 3)" << endl;
        return false;
    }

    test = store.match(Triple(Node(), "a", store.expand("type:A")));
    if (test.size() != 2) {
        cerr << "Wrong number of A-type nodes in store (found " << test.size() << ", expected 2)" << endl;
        return false;
    }
    int blankCount = 0, uriCount = 0;
    foreach (Triple t, test) {
        if (t.a.type() == Node::URI) uriCount++;
        else if (t.a.type() == Node::Blank) blankCount++;
    }
    if (blankCount != 1 || uriCount != 1) {
        cerr << "Unexpected distribution of URI and blank A-type nodes in store (expected 1 and 1, got " << blankCount << " and " << uriCount << ")" << endl;
        return false;
    }

    test = store.match(Triple(Node(), "a", store.expand("type:B")));
    if (test.size() != 4) {
        cerr << "Wrong number of B-type nodes in store (found " << test.size() << ", expected 4)" << endl;
        return false;
    }
    foreach (Triple t, test) {
        if (t.a.type() != Node::Blank) {
            cerr << "B-type node in store is not expected blank node" << endl;
            return false;
        }
    }

    test = store.match(Triple(Node(), "a", store.expand("type:C")));
    if (test.size() != 3) {
        cerr << "Wrong number of C-type nodes in store (found " << test.size() << ", expected 3)" << endl;
        return false;
    }
    foreach (Triple t, test) {
        if (t.a.type() != Node::Blank) {
            cerr << "C-type node in store is not expected blank node" << endl;
            return false;
        }
    }

    // We should be able to do a more sophisticated query to ensure
    // the relationships are right:
    ResultSet rs = store.query(
        " SELECT ?bn ?pn WHERE { "
        "   ?b a type:B ; "
        "      property:objectName ?bn ; "
        "      property:aref ?a . "
        "   ?a a type:A ; "
        "      rel:parent ?p . "
        "   ?p property:objectName ?pn . "
        " } "
        );
    if (rs.size() != 1) {
        cerr << "Query on stored objects returns unexpected number of replies "
             << rs.size() << " (expected 1)" << endl;
        return false;
    }
    if (rs[0]["bn"].type() != Node::Literal ||
        rs[0]["bn"].value() != "b0") {
        cerr << "Name of B-type object from query on stored objects is incorrect (expected b0, got " << rs[0]["bn"].value() << ")" << endl;
        return false;
    }
    if (rs[0]["pn"].type() != Node::Literal ||
        rs[0]["pn"].value() != "Test Object") {
        cerr << "Name of parent object from query on stored objects is incorrect (expected Test Object, got " << rs[0]["pn"].value() << ")" << endl;
        return false;
    }


    if (!testReloadability(store)) {
        cerr << "Reloadability test for custom object network store and recall failed" 
             << endl;
        return false;
    }


    {

    TransactionalStore ts(&store);

    ObjectMapper mapper(&ts);
    mapper.manage(o);
    mapper.manage(t);
    mapper.manage(a);
    
    caught = false;
    try {
        mapper.manage(a1);
    } catch (NoUriException) {
        // This is expected; a1 has no URI (it should have been
        // written as a blank node, because only referred to as an
        // object property) so cannot be managed without being added
        cerr << "Correctly caught NoUriException when trying to manage object written as blank node" << endl;
        caught = true;
    }
    if (!caught) {
        cerr << "Expected NoUriException when trying to manage object written as blank node did not materialise" << endl;
        return false;
    }
    mapper.add(a1);
    mapper.add(b);
    mapper.add(b0);
    mapper.add(b1);
    mapper.add(b2);
    mapper.add(c);
    mapper.add(c1);
    mapper.add(c2);

    Node n = mapper.getNodeForObject(c);
    if (n != Node()) {
        cerr << "ObjectMapper returns non-nil Node for managed object prior to commit" << endl;
        return false;
    }

    mapper.commit();

    n = mapper.getNodeForObject(c);
    if (n == Node()) {
        cerr << "ObjectMapper returns nil Node for managed object after commit" << endl;
        return false;
    }

    Triple t = ts.matchFirst(Triple(n, "property:string", Node()));
    if (t.c != Node() && t.c != Node(Node::Literal, "")) {
        cerr << "Unexpected node " << t.c << " in store for property that we have not even set yet (expected nil or empty string literal)" << endl;
        return false;
    }

    c->setString("Lone string");

    t = ts.matchFirst(Triple(n, "property:string", Node()));
    if (t.c != Node() && t.c != Node(Node::Literal, "")) {
        cerr << "Unexpected node " << t.c << " in store for property that ObjectMapper should not have committed yet (expected nil or empty string literal)" << endl;
        return false;
    }

    mapper.commit();
    
    t = ts.matchFirst(Triple(n, "property:string", Node()));
    if (t.c.value() != "Lone string") {
        cerr << "Incorrect node " << t.c << " in store for property that ObjectMapper should have committed" << endl;
        return false;
    }

    c->setString("New lone string");
    
    t = ts.matchFirst(Triple(n, "property:string", Node()));
    if (t.c.value() != "Lone string") {
        cerr << "Incorrect node " << t.c << " in store for property that ObjectMapper should not have re-committed" << endl;
        return false;
    }

    mapper.commit();
    
    t = ts.matchFirst(Triple(n, "property:string", Node()));
    if (t.c.value() != "New lone string") {
        cerr << "Incorrect node " << t.c << " in store for property that ObjectMapper should have re-committed" << endl;
        return false;
    }

    Transaction *tx = ts.startTransaction();
    if (!tx->remove(Triple(n, "property:string", Node()))) {
        cerr << "Failed to remove property triple in transactional store!" << endl;
        return false;
    }
    tx->commit();
    delete tx;

    if (c->getString() != "") {
        cerr << "Incorrect value " << c->getString() << " for property deleted in store that ObjectMapper should have reloaded" << endl;
        return false;
    }
    
    tx = ts.startTransaction();
    if (!tx->add(Triple(n, "property:string",
                        Node(Node::Literal, "Another lone string")))) {
        cerr << "Failed to add property triple in transactional store!" << endl;
        return false;
    }
    tx->commit();
    delete tx;

    if (c->getString() != "Another lone string") {
        cerr << "Incorrect value " << c->getString() << " for property added in store that ObjectMapper should have reloaded" << endl;
        return false;
    }

    strings << "Third string";
    c->setStrings(strings);

    store.save("test-mapper-auto-1.ttl");

    mapper.commit();
    
    store.save("test-mapper-auto-2.ttl");

    delete c;

    store.save("test-mapper-auto-3.ttl");

    mapper.commit();
    
    store.save("test-mapper-auto-4.ttl");

    delete a;
    delete a1;
    delete b;
    delete b0;
    delete b1;
    delete b2;
    delete c1;
    delete c2;

    mapper.commit();
  
    store.save("test-mapper-auto-5.ttl");
    
    // This is a test for a very specific situation -- we cause to be
    // deleted the parent of a managed object, by removing all
    // references to the parent from the store so that the mapper
    // deletes the parent when the transaction is committed.  This
    // causes the child to be deleted by Qt; is the mapper clever
    // enough to realise that this is a different deletion signal from
    // that of the parent, and re-sync the child accordingly?

    tx = ts.startTransaction();
    // recall that uri is the uri of the original test object o, which
    // is the parent of the timer object t
    tx->remove(Triple(uri, Node(), Node()));
    tx->commit();
    delete tx;

    mapper.commit();

    store.save("test-mapper-auto-6.ttl");
    
    // and turi is the uri of the timer object
    
    test = ts.match(Triple(turi, Node(), Node()));
    if (test.size() > 0) {
        cerr << "Incorrectly found " << test.size() << " triples with " << turi
             << " as subject in store after erasing all triples for the parent "
             << "object " << uri << " and having mapper resync" << endl;
        return false;
    }

    }
    

    // Another very specific one involving cycles -- an object that is
    // referred to as a property of its own parent.  On loading
    // through the child with follow-parent set, we should find that
    // the object has the correct parent and that the parent refers to
    // the correct object in its property (and not that the loader has
    // instantiated the child without a parent in order to fill the
    // parent's property in a recursive call while instantiating the
    // parent for the child)

    Node child(store.getUniqueUri(":child_"));
    Node parent(store.getUniqueUri(":parent_"));
    store.add(Triple(child, "a", store.expand("type:A")));
    store.add(Triple(child, "rel:parent", parent));
    store.add(Triple(parent, "a", store.expand("type:B")));
    store.add(Triple(parent, "property:aref", child));
    store.save("test-object-mapper-4.ttl");
    loader.setFollowPolicy(ObjectLoader::FollowObjectProperties |
                           ObjectLoader::FollowParent);
    ObjectLoader::NodeObjectMap testMap;
    loader.reload(Nodes() << child, testMap);
    
    if (testMap.size() != 2) {
        cerr << "Incorrect number of nodes in map (" << testMap.size() 
             << ", expected 2) after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (!testMap.contains(parent) || !testMap.contains(child)) {
        cerr << "Parent and/or child node missing from map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (!testMap.value(parent) || !testMap.value(child)) {
        cerr << "Parent and/or child object missing from map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (!qobject_cast<A*>(testMap.value(child))) {
        cerr << "Child has wrong type in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (!qobject_cast<B*>(testMap.value(parent))) {
        cerr << "Parent has wrong type in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (testMap.value(child)->parent() != testMap.value(parent)) {
        cerr << "Child has wrong parent (" << testMap.value(child)->parent()
             << ", expected " << testMap.value(parent) << ") in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (testMap.value(parent)->children().size() != 1) {
        cerr << "Parent has wrong number of children (" << testMap.value(parent)->children().size()
             << ", expected 1) in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (testMap.value(parent)->children()[0] != testMap.value(child)) {
        cerr << "Parent has wrong child (" << testMap.value(parent)->children()[0]
             << ", expected " << testMap.value(child) << ") in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }
    if (qobject_cast<B*>(testMap.value(parent))->getA() != testMap.value(child)) {
        cerr << "Parent has wrong value for Aref property ("
             << qobject_cast<B*>(testMap.value(parent))->getA()
             << ", expected " << testMap.value(child) << ") in map after loading parent-refers-to-child circular graph" << endl;
        return false;
    }

    std::cerr << "testObjectMapper done" << std::endl;
    return true;
}

}

}

int
main(int argc, char **argv)
{
    Dataquay::Uri uri("http://blather-de-hoop/parp/");
    QVariant v = QVariant::fromValue(uri);
    Dataquay::Uri uri2(v.value<Dataquay::Uri>());
    if (uri != uri2) {
        std::cerr << "ERROR: URI " << uri << " converted to variant and back again yields different URI " << uri2 << std::endl;
        return 1;
    }

    if (!Dataquay::Test::testBasicStore()) return 1;
    if (!Dataquay::Test::testDatatypes()) return 1;
    if (!Dataquay::Test::testImportOptions()) return 1;
    if (!Dataquay::Test::testTransactionalStore()) return 1;
    if (!Dataquay::Test::testConnection()) return 1;
    if (!Dataquay::Test::testObjectMapper()) return 1;

//    if (!Dataquay::Test::testQtWidgets(argc, argv)) return 1;

    std::cerr << "testDataquay successfully completed" << std::endl;
    return 0;
}

