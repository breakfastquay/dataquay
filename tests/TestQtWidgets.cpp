
#include "../BasicStore.h"
#include "../PropertyObject.h"
#include "../TransactionalStore.h"
#include "../Connection.h"
#include "../RDFException.h"
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
    QObject *layout = 0;
    QObject *layoutOf = 0;
    
    PropertyObject pod(&store, "dq:", objectUri);
    if (pod.hasProperty("parent")) {
	parent = load(store, map, pod.getProperty("parent").toUrl());
    }
    if (pod.hasProperty("layout")) {
	layout = load(store, map, pod.getProperty("layout").toUrl());
    }
    if (pod.hasProperty("layout_of")) {
	layoutOf = load(store, map, pod.getProperty("layout_of").toUrl());
    }
    if (pod.hasProperty("follows")) {
	load(store, map, pod.getProperty("follows").toUrl());
    }

    Triple typeTriple = store.matchFirst(Triple(objectUri, "a", Node()));
    if (typeTriple == Triple() || typeTriple.c.type != Node::URI) {
	return 0;
    }

    QString type = typeTriple.c.value;
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

int
main(int argc, char **argv)
{
    QApplication app(argc, argv);
    
    BasicStore store;
    store.import("file:test-qt-widgets.ttl", BasicStore::ImportIgnoreDuplicates);

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

    foreach (QObject *o, uriObjectMap) {
	QMainWindow *mw = dynamic_cast<QMainWindow *>(o);
	if (mw) {
	    std::cerr << "showing main window" << std::endl;
	    mw->show();
	}
    }
/*
	QObject *obj = mo[u]->make();
	if (!obj) {
	    std::cerr << "construction failed: " << t.c.value.toStdString() << std::endl;
	    continue;
	}
	uriObjectMap[t.a.value] = obj;

	std::cerr << "object's properties:" << std::endl;
	for (int i = 0; i < obj->metaObject()->propertyCount(); ++i) {
	    if (i == obj->metaObject()->propertyOffset()) {
		std::cerr << "[properties for this class:]" << std::endl;
	    }
	    std::cerr << obj->metaObject()->property(i).name() << ", type "
		      << obj->metaObject()->property(i).typeName()
		      << ", scriptable "
		      << obj->metaObject()->property(i).isScriptable()
		      << ", writable "
		      << obj->metaObject()->property(i).isWritable() << std::endl;
	}
	
	PropertyObject po(&store, "qtype:", t.a.value);
	QStringList sl = po.getProperties();
	for (int i = 0; i < sl.size(); ++i) {
	    QByteArray ba = sl[i].toLocal8Bit();
	    std::cerr << "property: " << sl[i].toStdString() << std::endl;
	    std::cerr << "index = " << obj->metaObject()->indexOfProperty(ba.data()) << std::endl;
	    QVariant value = po.getProperty(sl[i]);
	    if (!obj->setProperty(ba.data(), value)) {
		std::cerr << "property set failed for " << ba.data() << " to " << value.toString().toStdString() << std::endl;
	    } else {
		std::cerr << "property set succeeded" << std::endl;
	    }
	}
    }

    foreach (QString uri, uriObjectMap.keys()) {

	QObject *obj = uriObjectMap[uri];

	PropertyObject pod(&store, "dq:", uri);
	QStringList sl = pod.getProperties();
	foreach (QString property, sl) {
	    if (property == "parent") {
		QString parent = pod.getProperty(property).toUrl().toString();
		if (!uriObjectMap.contains(parent)) {
		    std::cerr << "unknown parent \"" << parent.toStdString() << "\"" << std::endl;
		} else {
		    obj->setParent(uriObjectMap[parent]);
		}
	    } else if (property == "layout_of") {
		QString target = pod.getProperty(property).toUrl().toString();
		if (uriObjectMap.contains(target)) {
		    QObject *tobj = uriObjectMap[target];
		    if (dynamic_cast<QWidget *>(tobj) &&
			dynamic_cast<QLayout *>(obj)) {
			dynamic_cast<QWidget *>(tobj)->setLayout
			    (dynamic_cast<QLayout *>(obj));
			std::cerr << "added layout to widget" << std::endl;
		    }
		}
	    } else if (property == "layout") {
		QString layout = pod.getProperty(property).toUrl().toString();
		if (!uriObjectMap.contains(layout)) {
		    std::cerr << "unknown layout \"" << layout.toStdString() << "\"" << std::endl;
		} else {
		    QLayout *qlayout = dynamic_cast<QLayout *>(uriObjectMap[layout]);
		    if (!qlayout) {
			std::cerr << "not a layout \"" << layout.toStdString() << "\"" << std::endl;
		    } else {
			if (dynamic_cast<QWidget *>(obj)) {
			    qlayout->addWidget(dynamic_cast<QWidget *>(obj));
			    std::cerr << "added to layout" << std::endl;
//			} else if (dynamic_cast<QLayout *>(obj)) {
//			    qlayout->addLayout(dynamic_cast<QLayout *>(obj));
			}
		    }			    
		}
	    }
	}
    }
    
    foreach (QString uri, uriObjectMap.keys()) {

	QObject *obj = uriObjectMap[uri];

	QWidget *w = dynamic_cast<QWidget *>(obj);
	if (w) w->show();
	else std::cerr << "not a qwidget: " << uri.toStdString() << std::endl;

	QLabel *l = dynamic_cast<QLabel *>(obj);
	if (l) {
	    std::cerr << "This is a label, text is: " << l->text().toStdString() << std::endl;
	}
    }
*/
    return app.exec();
}

