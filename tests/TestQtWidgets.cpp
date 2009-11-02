
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

static QString qtypePrefix = "http://breakfastquay.com/rdf/dataquay/qtype/";
static QString dqPrefix = "http://breakfastquay.com/rdf/dataquay/common/";

struct MakerBase {
    virtual QObject *make(QObject *) = 0;
};

template <typename T>
struct Maker0 : public MakerBase {
    virtual QObject *make(QObject *) { return new T(); }
};

template <typename T, typename P>
struct Maker1 : public MakerBase {
    virtual QObject *make(QObject *p) { return new T(dynamic_cast<P *>(p)); }
};

typedef QHash<QString, MakerBase *> MakerMap;

static MakerMap
makers()
{
    static MakerMap map;
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (map.empty()) {
#define MAP0(x) map[qtypePrefix + #x] = new Maker0<x>();
#define MAP1(x,y) map[qtypePrefix + #x] = new Maker1<x,y>();
	MAP0(QMainWindow);
	MAP1(QFrame, QWidget);
	MAP1(QLabel, QWidget);
	MAP1(QGridLayout, QWidget);
	MAP1(QVBoxLayout, QWidget);
	MAP1(QMenu, QWidget);
	MAP1(QMenuBar, QWidget);
	MAP1(QAction, QObject);
#undef MAP0
#undef MAP1
    }
    return map;
}

#include <QApplication>

using namespace Dataquay;

typedef QHash<QString, QObject *> UriObjectMap;

QObject *
load(Store &store, UriObjectMap &map, QUrl objectUri);

void
loadLayoutProperties(Store &store, UriObjectMap &map, CacheingPropertyObject &pod, QObject *o)
{
    QObject *layout = 0;  
    QObject *layoutOf = 0;
    QObject *centralOf = 0;

    if (pod.hasProperty("layout")) {
	layout = load(store, map, pod.getProperty("layout").toUrl());
    }
    if (pod.hasProperty("layout_of")) {
	layoutOf = load(store, map, pod.getProperty("layout_of").toUrl());
    }
    if (pod.hasProperty("central_widget_of")) {
	centralOf = load(store, map, pod.getProperty("central_widget_of").toUrl());
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

QObject *
load(Store &store, UriObjectMap &map, QUrl objectUri)
{
    if (map.contains(objectUri.toString())) {
	std::cerr << "returning: " << objectUri.toString().toStdString() << std::endl;
	return map[objectUri.toString()];
    }

    std::cerr << "loading: " << objectUri.toString().toStdString() << std::endl;

    // - Look up the dq properties: parent, layout_of, layout.  For
    // each of these, make sure the referred object is loaded before
    // we proceed.  Then create our object.

    // Note that (at the time of writing) librdf queries do not
    // properly support OPTIONAL.  So we don't have the option of
    // looking these up in a single SPARQL query.  Let's use a
    // PropertyObject instead

    QObject *parent = 0;
  
    CacheingPropertyObject pod(&store, "dq:", objectUri);

    if (pod.hasProperty("follows")) {
	load(store, map, pod.getProperty("follows").toUrl());
    }

    if (pod.hasProperty("parent")) {
	parent = load(store, map, pod.getProperty("parent").toUrl());
    }

    QString type = pod.getObjectType().toString();
    if (!type.startsWith(qtypePrefix)) {
	std::cerr << "not a qtypePrefix property: " << type.toStdString() << std::endl;
	return 0;
    }

    MakerMap mo = makers();
    if (!mo.contains(type)) {
	std::cerr << "not a known type: " << type.toStdString() << std::endl;
	return 0;
    }
    
    std::cerr << "Making object <" << objectUri.toString().toStdString()
	      << "> of type <" << type.toStdString() << ">" << std::endl;

    QObject *o = mo[type]->make(parent);
    if (!o) {
	std::cerr << "Failed to make object!" << std::endl;
	return o;
    }

    if (dynamic_cast<QMainWindow *>(parent) &&
	dynamic_cast<QMenu *>(o)) {
	dynamic_cast<QMainWindow *>(parent)->menuBar()->addMenu
	    (dynamic_cast<QMenu *>(o));
    }
    if (dynamic_cast<QMenu *>(parent) &&
	dynamic_cast<QAction *>(o)) {
	dynamic_cast<QMenu *>(parent)->addAction(dynamic_cast<QAction *>(o));
    }

    loadLayoutProperties(store, map, pod, o);

    PropertyObject po(&store, "qtype:", objectUri);
    QStringList pl = po.getProperties();
    foreach (QString property, pl) {
	QByteArray ba = property.toLocal8Bit();
	QVariant value = po.getProperty(property);
	if (!o->setProperty(ba.data(), value)) {
	    std::cerr << "property set failed for " << ba.data() << " to " << value.toString().toStdString() << std::endl;
	} else {
	    std::cerr << "property set succeeded" << std::endl;
	}
    }

    map[objectUri.toString()] = o;
    
    return o;
}

typedef QMap<QObject *, QString> ObjectUriMap;

QUrl
save(Store &store, ObjectUriMap &map, QObject *o);

void
saveLayoutProperties(Store &store, ObjectUriMap &map, PropertyObject &pod, QObject *o)
{
    QLayout *layout = dynamic_cast<QLayout *>(o);
    if (layout) {
	pod.setProperty(0, "layout_of", save(store, map, o->parent()));
    }
    QWidget *widget = dynamic_cast<QWidget *>(o);
    if (widget) {
	if (widget->layout()) {
	    pod.setProperty(0, "layout", save(store, map, widget->layout()));
	}
    }
}

QUrl
save(Store &store, ObjectUriMap &map, QObject *o)
{
    if (map.contains(o)) {
	std::cerr << "returning" << std::endl;
	return map[o];
    }

    QString cname = o->metaObject()->className();

    QUrl uri = store.getUniqueUri(":" + cname.toLower().right(cname.length()-1) + "_");
    map[o] = uri.toString();

    QUrl type = store.expand(QString("qtype:%1").arg(cname));
    store.add(Triple(uri, "a", type));

    if (o->parent() && map.contains(o->parent())) {
	store.add(Triple(uri, "dq:parent",
			 store.expand(map[o->parent()])));
    }

    MakerMap mm = makers();
    PropertyObject po(&store, "qtype:", uri);
    for (int i = 0; i < o->metaObject()->propertyCount(); ++i) {
	if (o->metaObject()->property(i).isStored()) {
	    QString pname = o->metaObject()->property(i).name();
	    QByteArray pnba = pname.toLocal8Bit();
	    bool write = true;
	    if (mm.contains(type.toString())) {
		//!!! or without parent?
		QObject *deft = mm[type.toString()]->make(o->parent());
		if (o->property(pnba.data()) == deft->property(pnba.data())) {
		    std::cerr << "unchanged: " << pnba.data() << std::endl;
		    write = false;
		} else {
		    QDebug(QtDebugMsg) << "different: " << o->property(pnba.data()) << " from " << deft->property(pnba.data()) << endl;
		}
		delete deft;
	    }
	    if (write) {
		po.setProperty(0, pname, o->property(pnba.data()));
	    }
	}
    }

    QObject *previous = 0;
    foreach (QObject *child, o->children()) {
	QUrl childUri = save(store, map, child);
	if (previous) {
	    store.add(Triple(childUri, "dq:follows",
			     store.expand(map[previous])));
	}
	previous = child;
    }

    return uri;
}

int
main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QString infile = "file:test-qt-widgets.ttl";
    if (argc > 1) {
	infile = QString("file:%1").arg(argv[1]);
    }

    BasicStore store;
    store.import(infile, BasicStore::ImportIgnoreDuplicates);

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

    QMainWindow *mw;
    foreach (QObject *o, uriObjectMap) {
	QMainWindow *hmw = dynamic_cast<QMainWindow *>(o);
	if (hmw) {
	    mw = hmw;
	    std::cerr << "showing main window" << std::endl;
	    mw->show();
	}
    }

    BasicStore store2;
    store2.setBaseUri(store.getBaseUri());
    store2.addPrefix("dq", dqPrefix);
    store2.addPrefix("qtype", qtypePrefix);
    ObjectUriMap rmap;
//    foreach (QString s, uriObjectMap.keys()) rmap[uriObjectMap[s]] = s;
    save(store2, rmap, mw);
    store2.save("test-qt-widgets-out.ttl");

    return app.exec();
}

