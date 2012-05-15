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

#ifndef _TEST_OBJECT_MAPPER_H_
#define _TEST_OBJECT_MAPPER_H_

#include <QObject>
#include <QtTest>
#include <QMetaType>
#include <QStringList>
#include <QSet>

#include <dataquay/Node.h>
#include <dataquay/BasicStore.h>
#include <dataquay/RDFException.h>

#include <dataquay/objectmapper/TypeMapping.h>
#include <dataquay/objectmapper/ObjectStorer.h>
#include <dataquay/objectmapper/ObjectLoader.h>
#include <dataquay/objectmapper/ObjectMapper.h>
#include <dataquay/objectmapper/ObjectBuilder.h>
#include <dataquay/objectmapper/ContainerBuilder.h>
#include <dataquay/objectmapper/ObjectMapperExceptions.h>

// Object types to be used in the tests

class A : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject *ref READ getRef WRITE setRef STORED true)

public:
    A(QObject *parent = 0) : QObject(parent), m_ref(0) { }

    QObject *getRef() const { return m_ref; }
    void setRef(QObject *r) { m_ref = r; }

private:
    QObject *m_ref;
};

Q_DECLARE_METATYPE(A*)

class B : public QObject
{
    Q_OBJECT

    Q_PROPERTY(A *aref READ getA WRITE setA STORED true)

public:
    B(QObject *parent = 0) : QObject(parent), m_a(0) { }

    A *getA() const { return m_a; }
    void setA(A *r) { m_a = qobject_cast<A *>(r); }

private:
    A *m_a;
};

Q_DECLARE_METATYPE(B*)

class C : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString string READ getString WRITE setString NOTIFY stringChanged STORED true)
    Q_PROPERTY(QStringList strings READ getStrings WRITE setStrings NOTIFY stringsChanged STORED true)
    Q_PROPERTY(QList<float> floats READ getFloats WRITE setFloats STORED true)
    Q_PROPERTY(QList<B *> bees READ getBees WRITE setBees STORED true)
    Q_PROPERTY(QSet<C *> cees READ getCees WRITE setCees STORED true)
    Q_PROPERTY(QObjectList objects READ getObjects WRITE setObjects STORED true)

public:
    C(QObject *parent = 0) : QObject(parent) { }

    QString getString() const { return m_string; }
    void setString(QString s) { m_string = s; emit stringChanged(s); }

    QStringList getStrings() const { return m_strings; }
    void setStrings(QStringList sl) { m_strings = sl; emit stringsChanged(sl); }

    QList<float> getFloats() const { return m_floats; }
    void setFloats(QList<float> fl) { m_floats = fl; }

    QList<B *> getBees() const { return m_bees; }
    void setBees(QList<B *> bl) { m_bees = bl; }

    QSet<C *> getCees() const { return m_cees; }
    void setCees(QSet<C *> cl) { m_cees = cl; }

    QObjectList getObjects() const { return m_objects; }
    void setObjects(QObjectList ol) { m_objects = ol; }

signals:
    void stringChanged(QString s);
    void stringsChanged(QStringList sl);

private:
    QString m_string;
    QStringList m_strings;
    QList<float> m_floats;
    QList<B *> m_bees;
    QSet<C *> m_cees;
    QObjectList m_objects;
};

Q_DECLARE_METATYPE(C*)
Q_DECLARE_METATYPE(QList<float>)
Q_DECLARE_METATYPE(QObjectList)
Q_DECLARE_METATYPE(QList<A*>)
Q_DECLARE_METATYPE(QList<B*>)
Q_DECLARE_METATYPE(QList<C*>)
Q_DECLARE_METATYPE(QSet<C*>)

namespace Dataquay {

class TestObjectMapper : public QObject
{
    Q_OBJECT

public:
    TestObjectMapper() :
	store(),
	storer(&store) { }

    ~TestObjectMapper() { }

private slots:
    void initTestCase() {
	store.addPrefix("type", storer.getTypeMapping().getObjectTypePrefix());
	store.addPrefix("property", storer.getTypeMapping().getPropertyPrefix());
	store.addPrefix("rel", storer.getTypeMapping().getRelationshipPrefix());
    }
    
    void init() {
	store.clear();
	storer.setPropertyStorePolicy(ObjectStorer::StoreIfChanged);
    }

    void untypedStoreRecall() {
	QObject *o = new QObject;
	o->setObjectName("Test Object");
	Uri uri = storer.store(o);
	QVERIFY(uri != Uri());

	ObjectLoader loader(&store);
	QObject *recalled = loader.load(uri);
	QVERIFY(recalled);
	QCOMPARE(recalled->objectName(), o->objectName());
	delete recalled;
	delete o;
    }

    void typedStoreRecall() {
	
	QTimer *t = new QTimer;
	t->setSingleShot(true);
	t->setInterval(4);
	Uri uri = storer.store(t);
	QVERIFY(uri != Uri());

	ObjectLoader loader(&store);
	QObject *recalled = 0;
	try {
	    // object type is not registered, this load should fail
	    recalled = loader.load(uri);
	    QVERIFY(0);
	} catch (UnknownTypeException) {
	    QVERIFY(1);
	}

	ObjectBuilder::getInstance()->registerClass<QTimer, QObject>();

	recalled = loader.load(uri);
	QVERIFY(recalled);
	QCOMPARE(recalled->objectName(), t->objectName());
	QVERIFY(qobject_cast<QTimer *>(recalled));

	delete recalled;
	delete t;
    }

    void customTypeStoreRecall() {
	
	A *a = new A;
	a->setProperty("uri", QVariant::fromValue<Uri>(Uri(":a")));
	Uri uri = storer.store(a);
	QVERIFY(uri != Uri());

	ObjectLoader loader(&store);
	QObject *recalled = 0;

	try {
	    // object type is not registered, this load should fail
	    recalled = loader.load(uri);
	    QVERIFY(0);
	} catch (UnknownTypeException) {
	    QVERIFY(1);
	}

	qRegisterMetaType<A*>("A*");
	ObjectBuilder::getInstance()->registerClass<A, QObject>("A*");

	recalled = loader.load(uri);
	QVERIFY(recalled);
	QCOMPARE(recalled->objectName(), a->objectName());
	QVERIFY(qobject_cast<A *>(recalled));

	delete recalled;
	delete a;
    }

    void prepareGraphTypes() {

	// We registered the metatype and object builder class for A
	// in the previous test
	
	qRegisterMetaType<B*>("B*");
	qRegisterMetaType<C*>("C*");
	qRegisterMetaType<QList<float> >("QList<float>");
	qRegisterMetaType<QList<B*> >("QList<B*>");
	qRegisterMetaType<QSet<C*> >("QSet<C*>");
	qRegisterMetaType<QObjectList>("QObjectList");
	ObjectBuilder::getInstance()->registerClass<B, QObject>("B*");
	ObjectBuilder::getInstance()->registerClass<C, QObject>("C*");

	ContainerBuilder::getInstance()->registerContainer<A*, QList<A*> >("A*", "QList<A*>", ContainerBuilder::SequenceKind);
	ContainerBuilder::getInstance()->registerContainer<B*, QList<B*> >("B*", "QList<B*>", ContainerBuilder::SequenceKind);
	ContainerBuilder::getInstance()->registerContainer<C*, QList<C*> >("C*", "QList<C*>", ContainerBuilder::SequenceKind);
	ContainerBuilder::getInstance()->registerContainer<QObject*, QObjectList>("QObject*", "QObjectList", ContainerBuilder::SequenceKind);
	ContainerBuilder::getInstance()->registerContainer<float, QList<float> >("float", "QList<float>", ContainerBuilder::SequenceKind);
	ContainerBuilder::getInstance()->registerContainer<C *, QSet<C *> >("C*", "QSet<C*>", ContainerBuilder::SetKind);
    }

    void complexGraphStoreRecall() {

	QObject *o = new QObject;
	o->setObjectName("Test Object");
	A *a = new A(o);
	B *b = new B(o);
	b->setA(a);
	C *c = new C;
	c->setObjectName("C with many properties");
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
	// nb circular reference
	cset.insert(c);
	c->setCees(cset);
	QObjectList ol;
	ol << b2;
	c->setObjects(ol);
	a->setRef(c);
	
	storer.setFollowPolicy(ObjectStorer::FollowObjectProperties |
			       ObjectStorer::FollowChildren);
	ObjectStorer::ObjectNodeMap map;
	storer.store(o, map);

	// We should have:
	//
	// - one object with URI, one property, of type:QObject
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
	QCOMPARE(test.size(), 10);

	storer.store(o, map);
	
	test = store.match(Triple(Node(), "a", Node()));
	QCOMPARE(test.size(), 10);
	
	// Now the rest of the content tests

	test = store.match(Triple(Node(), "a", store.expand("type:QObject")));
	QCOMPARE(test.size(), 1);
	QCOMPARE(test[0].a.type, Node::URI);

	test = store.match(Triple(Node(), "a", store.expand("type:A")));
	QCOMPARE(test.size(), 2);

	int blankCount = 0, uriCount = 0;
	foreach (Triple t, test) {
	    if (t.a.type == Node::URI) uriCount++;
	    else if (t.a.type == Node::Blank) blankCount++;
	}
	QCOMPARE(blankCount, 1);
	QCOMPARE(uriCount, 1);

	test = store.match(Triple(Node(), "a", store.expand("type:B")));
	QCOMPARE(test.size(), 4);

/*!!! not a good test -- one of the Bs ("b") will be given a URI because it's a top-level node rather than a property
    foreach (Triple t, test) {
        if (t.a.type != Node::Blank) {
            cerr << "B-type node in store is not expected blank node" << endl;
            return false;
        }
    }
*/

	test = store.match(Triple(Node(), "a", store.expand("type:C")));
	QCOMPARE(test.size(), 3);

	// pull out the b0 object node and check some of its properties
	test = store.match(Triple(Node(), "property:objectName",
				  Node(Node::Literal, "b0")));
	QCOMPARE(test.size(), 1);

	Node b0node = test[0].a;
	test = store.match(Triple(b0node, "property:aref",
				  Node()));
	QCOMPARE(test.size(), 1);

	Node anode = test[0].c;
	test = store.match(Triple(anode, "a", store.expand("type:A")));
	QCOMPARE(test.size(), 1);

	test = store.match(Triple(anode, "rel:parent", Node()));
	QCOMPARE(test.size(), 1);

	Node pnode = test[0].c;
	test = store.match(Triple(pnode, "property:objectName", Node()));
	QCOMPARE(test.size(), 1);
	QCOMPARE(test[0].c.value, QString("Test Object"));
    }


private:
    BasicStore store;
    ObjectStorer storer;
};    

}


#endif
