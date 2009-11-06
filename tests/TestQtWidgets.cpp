
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
	cerr << "LayoutLoader::loaded: uri " << uri.toString().toStdString() << ", object type " << o->metaObject()->className() << endl;

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
	    pod.setProperty(0, "layout_of", m->store(o->parent(), map));
	}
	QWidget *widget = dynamic_cast<QWidget *>(o);
	if (widget) {
	    if (widget->layout()) {
		pod.setProperty(0, "layout", m->store(widget->layout(), map));
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
    b->registerClass<QMainWindow>();
    b->registerClass<QFrame, QWidget>();
    b->registerClass<QLabel, QWidget>();
    b->registerClass<QGridLayout, QWidget>();
    b->registerClass<QVBoxLayout, QWidget>();
    b->registerClass<QMenu, QWidget>();
    b->registerClass<QMenuBar, QWidget>();
    b->registerClass<QAction, QObject>();

    ObjectMapper mapper(&store);
    mapper.setObjectTypePrefix(qtypePrefix);
    mapper.setPropertyPrefix(qtypePrefix);

    LayoutLoader loader;
    mapper.addLoadCallback(&loader);

    QObject *parent = mapper.loadAllObjects(0);

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
    ObjectMapper mapper2(&store2);
    mapper2.setPropertyStorePolicy(ObjectMapper::StoreIfChanged);
    mapper2.setObjectStorePolicy(ObjectMapper::StoreObjectsWithURIs);
    LayoutStorer storer;
    mapper2.addStoreCallback(&storer);
    mapper2.storeObjects(parent);
    store2.save("test-qt-widgets-out.ttl");

    return app.exec();
}

}
}
