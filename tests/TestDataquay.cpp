/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "../BasicStore.h"
#include "../PropertyObject.h"
#include "../TransactionalStore.h"
#include "../RDFException.h"
#include "../Debug.h"

#include <QStringList>
#include <QUrl>
#include <iostream>

#include <cassert>

using std::cerr;
using std::endl;

namespace Dataquay
{
namespace Test
{

std::ostream &
operator<<(std::ostream &target, const QString &str)
{
    return target << str.toLocal8Bit().data();
}

std::ostream &
operator<<(std::ostream &target, const QUrl &u)
{
    return target << "<" << u.toString() << ">";
}

void
dump(Triples t)
{
    for (Triples::const_iterator i = t.begin(); i != t.end(); ++i) {
        cerr << "(" << i->a.type << ":" << i->a.value << ") ";
        cerr << "(" << i->b.type << ":" << i->b.value << ") ";
        cerr << "(" << i->c.type << ":" << i->c.value << ") ";
        cerr << endl;
    }
}
    
bool
testBasicStore()
{
    BasicStore store;

    cerr << "testRDFStore starting..." << endl;

    for (int i = 0; i < 2; ++i) {

        bool haveBaseUri = (i == 1);

        if (!haveBaseUri) {
            cerr << "Starting tests without absolute base URI" << endl;
        } else {
            cerr << "Starting tests with absolute base URI" << endl;
            store.clear();
            store.setBaseUri("http://blather-de-hoop/");
        }

        QString base = store.getBaseUri();

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
                              Node::fromVariant(QUrl("http://breakfastquay.com/rdf/person/fred"))))) {
            cerr << "Failed to add variant URI to store" << endl;
            return false;
        }
    
        ++count;
        ++fromFred;

        // variant conversion
        if (!store.add(Triple(":fred",
                              ":has_some_local_uri",
                              Node::fromVariant(store.expand(":pootle"))))) {
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

        store.addPrefix("foaf", "http://xmlns.com/foaf/0.1/");
    
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
        QUrl someUri = triples[0].c.toVariant().toUrl();
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
        someUri = triples[0].c.toVariant().toUrl();
        if (someUri != store.expand(":pootle")) {
            cerr << "Fred's some-local-uri-or-other is not what we expected (it is "
                 << someUri.toString() << ")" << endl;
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
            if (v.type != Node::URI || v.value != expected) {
                cerr << "getFirstResult returned wrong result (expected URI of <" << expected << ">, got type " << v.type << " and value " << v.value << ")" << endl;
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

        BasicStore *store2 = BasicStore::load("file:test.ttl");

        //!!! todo: have the store read these from parser!
//        if (haveBaseUri) {
//            store2->setBaseUri(base);
//        }
//        store2->addPrefix("foaf", "http://xmlns.com/foaf/0.1/");


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

            store.import("file:test2.ttl");

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
testTransactionalStore()
{
    BasicStore store;

    cerr << "testTransactionalStore starting..." << endl;

    store.clear();
    store.setBaseUri("http://blather-de-hoop/");

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

    ChangeSet changes = t->getChanges();

    delete t;

    triples = ts.match(Triple());
    n = triples.size();
    if (n != added) {
        cerr << "Store add didn't take at all (triples matched on query != expected " << added << ")" << endl;
        return false;
    }

    t = ts.startTransaction();
    t->revert(changes);
    delete t;

    triples = ts.match(Triple());
    if (triples.size() > 0) {
        cerr << "Store revert failed (store is not empty)" << endl;
        return false;
    }

    t = ts.startTransaction();
    t->change(changes);
    delete t;

    triples = ts.match(Triple());
    if (triples.size() != n) {
        cerr << "Store re-change failed (triple count " << triples.size() << " != original " << n << ")" << endl;
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

}

}

int
main()
{
    Dataquay::Test::testBasicStore();
    Dataquay::Test::testTransactionalStore();

    std::cerr << "testDataquay successfully completed" << std::endl;
    return true;
}

