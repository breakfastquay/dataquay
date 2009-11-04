
#include "../BasicStore.h"
#include "../PropertyObject.h"
#include "../TransactionalStore.h"
#include "../Connection.h"
#include "../RDFException.h"
#include "../ObjectMapper.h"
#include "../ObjectBuilder.h"
#include "../Debug.h"

#include <QStringList>
#include <QUrl>
#include <QMutex>
#include <QMutexLocker>
#include <QWidget>
#include <iostream>

#include <QMainWindow>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QMenuBar>
#include <QMenu>
#include <QAction>

#include <cassert>

using std::cerr;
using std::endl;

#include <QMetaObject>
#include <QMetaProperty>

#include <QApplication>

namespace Dataquay
{
namespace Test
{

static QString qtypePrefix = "http://breakfastquay.com/rdf/dataquay/qtype/";
static QString dqPrefix = "http://breakfastquay.com/rdf/dataquay/common/"; //???

struct LayoutLoader : public ObjectMapper::LoadCallback {

    void loaded(ObjectMapper *m, ObjectMapper::UriObjectMap &map, QUrl uri, QObject *o)
    {
	Store *s = m->getStore();
	CacheingPropertyObject pod(s, dqPrefix, uri);

	QObject *parent = o->parent();
	if (dynamic_cast<QMainWindow *>(parent) &&
	    dynamic_cast<QMenu *>(o)) {
	    dynamic_cast<QMainWindow *>(parent)->menuBar()->addMenu
		(dynamic_cast<QMenu *>(o));
	}
	if (dynamic_cast<QMenu *>(parent) &&
	    dynamic_cast<QAction *>(o)) {
	    dynamic_cast<QMenu *>(parent)->addAction
		(dynamic_cast<QAction *>(o));
	}

	QObject *layout = 0;  
	QObject *layoutOf = 0;
	QObject *centralOf = 0;

	if (pod.hasProperty("layout")) {
	    layout = m->loadFrom(pod.getProperty("layout").toUrl(), map);
	}
	if (pod.hasProperty("layout_of")) {
	    layoutOf = m->loadFrom(pod.getProperty("layout_of").toUrl(), map);
	}
	if (pod.hasProperty("central_widget_of")) {
	    centralOf = m->loadFrom(pod.getProperty("central_widget_of").toUrl(), map);
	}

	if (centralOf) {
	    QMainWindow *m = dynamic_cast<QMainWindow *>(centralOf);
	    QWidget *w = dynamic_cast<QWidget *>(o);
	    if (m && w) {
		m->setCentralWidget(w);
		std::cerr << "added central widget" << std::endl;
	    }
	}

	if (layoutOf) {
	    QWidget *w = dynamic_cast<QWidget *>(layoutOf);
	    QLayout *l = dynamic_cast<QLayout *>(o);
	    if (w && l) {
		w->setLayout(l);
		std::cerr << "added layout to widget" << std::endl;
	    }
	}
	
	if (layout) {
	    QLayout *cl = dynamic_cast<QLayout *>(layout);
	    if (cl) {
		QWidget *w = dynamic_cast<QWidget *>(o);
		if (w) cl->addWidget(w);
		std::cerr << "added widget to layout" << std::endl;
	    }
	}
    }
};

struct LayoutStorer : public ObjectMapper::StoreCallback {

    void stored(ObjectMapper *m, ObjectMapper::ObjectUriMap &map, QObject *o, QUrl uri)
    {
	PropertyObject pod(m->getStore(), dqPrefix, uri);

	QLayout *layout = dynamic_cast<QLayout *>(o);
	if (layout) {
	    //!!! use map for storeObject
	    pod.setProperty(0, "layout_of", m->storeObject(o->parent()));
	}
	QWidget *widget = dynamic_cast<QWidget *>(o);
	if (widget) {
	    if (widget->layout()) {
		pod.setProperty(0, "layout", m->storeObject(widget->layout()));
	    }
	}
    }
};

extern int
testQtWidgets(int argc, char **argv)
{
    QApplication app(argc, argv);

    QString infile = "file:test-qt-widgets.ttl";
    if (argc > 1) {
	infile = QString("file:%1").arg(argv[1]);
    }

    BasicStore store;
    store.import(infile, BasicStore::ImportIgnoreDuplicates);

    ObjectBuilder *b = ObjectBuilder::getInstance();
    b->registerWithDefaultConstructor<QMainWindow>();
    b->registerWithParentConstructor<QFrame, QWidget>();
    b->registerWithParentConstructor<QLabel, QWidget>();
    b->registerWithParentConstructor<QGridLayout, QWidget>();
    b->registerWithParentConstructor<QVBoxLayout, QWidget>();
    b->registerWithParentConstructor<QMenu, QWidget>();
    b->registerWithParentConstructor<QMenuBar, QWidget>();
    b->registerWithParentConstructor<QAction, QObject>();

    ObjectMapper mapper(&store);
    mapper.setObjectTypePrefix(qtypePrefix);
    mapper.setPropertyPrefix(qtypePrefix);

    QObject *parent = mapper.loadAllObjects(0);
/*
    Triples candidates = store.match(Triple(Node(), "a", Node()));

    const char *n = 0;
    int i = 0;
    while ((n = QMetaType::typeName(i))) {
	std::cerr << "meta type " << i << " = " << n << std::endl;
	++i;
    }
    
    MakerMap mo = makers();

    UriObjectMap uriObjectMap;

    foreach (Triple t, candidates) {

	if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

	QString u = t.c.value;
	if (!u.startsWith(qtypePrefix)) continue;

	if (!mo.contains(u)) {
	    std::cerr << "WARNING: Unknown object URI <"
		      << u.toStdString() << ">" << std::endl;
	    continue;
	}

	QObject *o = load(store, uriObjectMap, t.a.value);
	if (o) uriObjectMap[t.a.value] = o;

	std::cerr << "top-level load of <" << t.a.value.toStdString() << "> succeeded" << std::endl;
    }

    candidates = store.match(Triple(Node(), "a", store.expand("dq:Connection")));

    QString slotTemplate = SLOT(xxx());
    QString signalTemplate = SIGNAL(xxx());

    foreach (Triple t, candidates) {

	PropertyObject po(&store, "dq:", t.a.value);

	QObject *source = 0;
	QObject *target = 0;

	if (po.hasProperty("source")) {
	    source = load(store, uriObjectMap, po.getProperty("source").toUrl());
	}
	if (po.hasProperty("target")) {
	    target = load(store, uriObjectMap, po.getProperty("target").toUrl());
	}

	if (!source || !target) continue;

	if (po.hasProperty("source_signal") && po.hasProperty("target_slot")) {
	    QString signal = signalTemplate.replace
		("xxx", po.getProperty("source_signal").toString());
	    QString slot = slotTemplate.replace
		("xxx", po.getProperty("target_slot").toString());
	    QByteArray sigba = signal.toLocal8Bit();
	    QByteArray slotba = slot.toLocal8Bit();
	    QObject::connect(source, sigba.data(), target, slotba.data());
	}
    }
*/
    QMainWindow *mw = dynamic_cast<QMainWindow *>(parent);
    if (!mw) {
	foreach (QObject *o, parent->children()) {
	    std::cerr << "child: " << o->metaObject()->className() << std::endl;
	    QMainWindow *hmw = dynamic_cast<QMainWindow *>(o);
	    if (hmw) {
		mw = hmw;
		std::cerr << "showing main window" << std::endl;
	    }
        }
    }
    if (mw) mw->show();

    BasicStore store2;
    store2.setBaseUri(store.getBaseUri());
    store2.addPrefix("dq", dqPrefix);
    store2.addPrefix("qtype", qtypePrefix);
//    ObjectUriMap rmap;
//    foreach (QString s, uriObjectMap.keys()) rmap[uriObjectMap[s]] = s;
//    save(store2, rmap, mw);
    ObjectMapper mapper2(&store2);
    mapper2.setPropertyStorePolicy(ObjectMapper::StoreIfChanged);
    mapper2.setObjectStorePolicy(ObjectMapper::StoreObjectsWithURIs);
    mapper2.storeObjects(parent);
    store2.save("test-qt-widgets-out.ttl");

    return app.exec();
}

}
}
