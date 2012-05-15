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

#include <dataquay/Node.h>
#include <dataquay/BasicStore.h>
#include <dataquay/RDFException.h>

#include <dataquay/objectmapper/TypeMapping.h>
#include <dataquay/objectmapper/ObjectStorer.h>
#include <dataquay/objectmapper/ObjectLoader.h>
#include <dataquay/objectmapper/ObjectMapper.h>
#include <dataquay/objectmapper/ObjectBuilder.h>
#include <dataquay/objectmapper/ObjectMapperExceptions.h>

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
    }


private:
    BasicStore store;
    ObjectStorer storer;
};    

}


#endif
