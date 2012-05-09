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

#ifndef _TEST_BASIC_STORE_H_
#define _TEST_BASIC_STORE_H_

#include <QObject>
#include <QtTest>

#include <dataquay/Node.h>
#include <dataquay/BasicStore.h>
#include <dataquay/RDFException.h>

namespace Dataquay {

class TestBasicStore : public QObject
{
    Q_OBJECT
    
private slots:

    void initTestCase() {
	store.setBaseUri(Uri("http://breakfastquay.com/rdf/dataquay/tests"));
	base = store.getBaseUri().toString();
	count = 0;
	fromFred = 0;
	toAlice = 0;
    }

    void simpleAdd() {
	// check triple can be added
	QVERIFY(store.add
		(Triple(Node(Node::URI, ":fred"),
			Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
			Node(Node::Literal, "Fred Jenkins"))));
	++count;
	++fromFred;
        // alternative Triple constructor
	QVERIFY(store.add
		(Triple(":fred",
			"http://xmlns.com/foaf/0.1/knows",
			Node(Node::URI, ":alice"))));
	++count;
	++fromFred;
	++toAlice;
    }
    void simpleLookup() {
	// check triple just added can be found again
	QVERIFY(store.contains
		(Triple(Node(Node::URI, ":fred"),
			Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
			Node(Node::Literal, "Fred Jenkins"))));
    }

    void simpleAbsentLookup() {
	// check absent triple lookups are correctly handled
	QVERIFY(!store.contains
		(Triple(Node(Node::URI, ":fred"),
			Node(Node::URI, "http://xmlns.com/foaf/0.1/name"),
			Node(Node::Literal, "Fred Johnson"))));
    }

    void addFromVariantInt() {
        // variant conversion
        QVERIFY(store.add
		(Triple(":fred",
			":age",
			Node::fromVariant(QVariant(42)))));
        ++count;
        ++fromFred;
	Triples triples = store.match
	    (Triple(Node(Node::URI, ":fred"),
		    Node(Node::URI, ":age"),
		    Node()));
	QCOMPARE(triples.size(), 1);
	QCOMPARE(triples[0].c.toVariant().toInt(), 42);
    }	

    void addFromVariantURI() {
        // variant conversion
	Uri fredUri("http://breakfastquay.com/rdf/person/fred");
	QVERIFY(store.add
		(Triple(":fred",
			":has_some_uri",
			Node::fromVariant
			(QVariant::fromValue(fredUri)))));
        ++count;
        ++fromFred;

	QVERIFY(store.add
		(Triple(":fred",
			":has_some_local_uri",
			Node::fromVariant
			(QVariant::fromValue(store.expand(":pootle"))))));
        ++count;
        ++fromFred;

	Triples triples = store.match
	    (Triple(Node(Node::URI, ":fred"),
		    Node(Node::URI, ":has_some_uri"),
		    Node()));
	QCOMPARE(triples.size(), 1);
	QCOMPARE(Uri(triples[0].c.value), fredUri);

	triples = store.match
	    (Triple(Node(Node::URI, ":fred"),
		    Node(Node::URI, ":has_some_local_uri"),
		    Node()));
	QCOMPARE(triples.size(), 1);
	QCOMPARE(triples[0].c.toVariant().value<Uri>(), store.expand(":pootle"));
    }

    void addFromVariantFloat() {
        // variant conversion
        QVERIFY(store.add
		(Triple(":fred",
			":likes_to_think_his_age_is",
			Node::fromVariant(QVariant(21.9)))));
        ++count;
        ++fromFred;
    }

    void addFromVariantBool() {
        // variant conversion
        QVERIFY(store.add
		(Triple(":fred",
			":is_sadly_deluded",
			Node::fromVariant(true))));
        ++count;
        ++fromFred;
        Triples triples = store.match
	    (Triple(Node(Node::URI, ":fred"),
		    Node(Node::URI, ":is_sadly_deluded"),
		    Node()));
	QCOMPARE(triples.size(), 1);
	QCOMPARE(triples[0].c.toVariant().toBool(), true);
    }

    void addFromVariantList() {
        // variant conversion
        QStringList colours;
        colours << "turquoise";
        colours << "red";
        colours << "black";
	QCOMPARE(colours.size(), 3);
	QVERIFY(store.add
		(Triple(":fred",
			":favourite_colours_are",
			Node::fromVariant(QVariant(colours)))));
        ++count;
        ++fromFred;
	Triples triples = store.match
	    (Triple(Node(Node::URI, ":fred"),
		    Node(Node::URI, ":favourite_colours_are"),
		    Node()));
	QCOMPARE(triples.size(), 1);
	QStringList retrievedColours = triples[0].c.toVariant().toStringList();
	QCOMPARE(colours, retrievedColours);
    }

    void addWithRdfTypeBuiltin() {
        // rdf:type builtin
	QVERIFY(store.add
		(Triple(":fred",
			"a",
			Node(Node::URI, ":person"))));
	QVERIFY(store.contains
		(Triple(Node(Node::URI, ":fred"),
			Node(Node::URI, "rdf:type"),
			Node(Node::URI, ":person"))));
        ++count;
        ++fromFred;
    }

    void addUsingPrefix() {
	// prefix expansion
        store.addPrefix("foaf", Uri("http://xmlns.com/foaf/0.1/"));
	QVERIFY(store.add
		(Triple(Node(Node::URI, ":alice"),
			Node(Node::URI, "foaf:knows"),
			Node(Node::URI, ":fred"))));
	QVERIFY(store.contains
		(Triple(Node(Node::URI, ":alice"),
			Node(Node::URI, "http://xmlns.com/foaf/0.1/knows"),
			Node(Node::URI, ":fred"))));
        QVERIFY(store.add
		(Triple(Node(Node::URI, ":alice"),
			Node(Node::URI, "foaf:name"),
			Node(Node::Literal, QString("Alice Banquet")))));
              
        ++count;
        ++count;
    }

    void addDuplicate() {
        // we try to add a triple that is a differently-expressed
        // duplicate of an already-added one -- this should be ignored
        // (returning false) and we do not increment our count
	QVERIFY(!store.add
		(Triple(base + "alice",
			"http://xmlns.com/foaf/0.1/name",
			Node(Node::Literal, QString("Alice Banquet")))));

        // now we try to add a triple a bit like an existing one but
        // differing in prefix -- this should succeed, and we do increment
        // our count, although this is not a very useful statement
	QVERIFY(store.add
		(Triple(":alice",
			"http://xmlns.com/foaf/0.1/knows",
			Node(Node::URI, "foaf:fred"))));
        ++count;
    }

    void addBlanks() {
        // things involving blank nodes
        Node blankNode = store.addBlankNode();
        QVERIFY(store.add
		(Triple(":fred",
			"http://xmlns.com/foaf/0.1/maker",
			blankNode)));
        ++count;
        ++fromFred;

	QVERIFY(store.add
		(Triple(blankNode,
			"foaf:name",
			Node(Node::Literal, "Omnipotent Being"))));
        ++count;
    }

    void addBlankPredicateFail() {
	// can't have a blank node as predicate
	Node anotherBlank = store.addBlankNode();
	try {
	    QVERIFY(!store.add
		    (Triple(Node(Node::URI, ":fred"),
			    anotherBlank,
			    Node(Node::Literal, "this_statement_is_incomplete"))));
	} catch (RDFException &) {
	    QVERIFY(1);
	}
    
    }

    void matchCounts() {
	// must run after adds
	QCOMPARE(store.match(Triple()).size(), count);
	QCOMPARE(store.match
		 (Triple(Node(Node::URI, ":fred"), Node(), Node())).size(),
		 fromFred);
	QCOMPARE(store.match
		 (Triple(Node(), Node(), Node(Node::URI, ":alice"))).size(),
		 toAlice);
    }

    void compareTriples() {

	// check empty Triples match
	QVERIFY(Triples().matches(Triples()));

	// check two identical searches return matching (non-empty) results
        Triples t1 = store.match(Triple(Node(Node::URI, ":fred"), Node(), Node()));
        Triples t2 = store.match(Triple(Node(Node::URI, ":fred"), Node(), Node()));
	QVERIFY(t1.size() > 0);
	QVERIFY(!t1.matches(Triples()));
	QVERIFY(t1.matches(t2));
	QVERIFY(t2.matches(t1));

	// check Triples matches itself in a different order
	t2 = Triples();
        foreach (Triple t, t1) t2.push_front(t);
	QVERIFY(t1.matches(t2));
	QVERIFY(t2.matches(t1));

	// check two different searches return non-matching results
	t2 = store.match(Triple(Node(Node::URI, ":alice"), Node(), Node()));
        QVERIFY(!t1.matches(t2));
        QVERIFY(!t2.matches(t1));
    }

    void remove() {
	// check we can remove a triple
	QVERIFY(store.remove
		(Triple(":fred",
			"http://xmlns.com/foaf/0.1/knows",
			Node(Node::URI, ":alice"))));
	--count;
	--fromFred;
	--toAlice;

	// check we can't remove a triple that does not exist in store
        QVERIFY(!store.remove
		(Triple(":fred",
			"http://xmlns.com/foaf/0.1/knows",
			Node(Node::URI, ":tammy"))));

        Triples triples = store.match(Triple());
	QCOMPARE(triples.size(), count);
    }

    void query() {
	
        QString q = QString(" SELECT ?a "
                            " WHERE { :fred foaf:knows ?a } ");
        ResultSet results;

	// We cannot perform queries without an absolute base URI,
	// it seems, because the query engine takes relative URIs
	// as relative to the query URI and we can't override that
	// without an absolute prefix for the base URI

	try {
	    results = store.query(q);
	    QCOMPARE(results.size(), 1);

	    Node v = store.queryFirst(q, "a");
	    QString expected = base + "alice";
	    QCOMPARE(v.type, Node::URI);
	    QCOMPARE(v.value, expected);

	} catch (RDFUnsupportedError &e) {
	    QSKIP("SPARQL queries not supported by current store backend",
		  SkipSingle);
	}
    }

    void saveAndLoad() {
	
        store.save("test.ttl");

        BasicStore *store2 = BasicStore::load(QUrl("file:test.ttl"));
	QVERIFY(store2);

	store2->save("test2.ttl");
	
	QCOMPARE(store2->match(Triple()).size(), count);
	QCOMPARE(store2->match
		 (Triple(Node(Node::URI, ":fred"), Node(), Node())).size(),
		 fromFred);
	QCOMPARE(store2->match
		 (Triple(Node(), Node(), Node(Node::URI, ":alice"))).size(),
		 toAlice);
	
	delete store2;
	
	store.clear();
	store.import(QUrl("file:test2.ttl"),
		     BasicStore::ImportFailOnDuplicates);
	
	QCOMPARE(store.match(Triple()).size(), count);
	QCOMPARE(store.match
		 (Triple(Node(Node::URI, ":fred"), Node(), Node())).size(),
		 fromFred);
	QCOMPARE(store.match
		 (Triple(Node(), Node(), Node(Node::URI, ":alice"))).size(),
		 toAlice);
    }	

private:
    BasicStore store;
    QString base;
    int count;
    int fromFred;
    int toAlice;
};

}

#endif