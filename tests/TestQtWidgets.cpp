
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
    virtual QObject *make() = 0;
};

template <typename T>
struct Maker : public MakerBase {
    virtual QObject *make() { return new T(); }
};

template <typename T>
struct Maker0 : public MakerBase {
    virtual QObject *make() { return new T(0); }
};

typedef QHash<QString, MakerBase *> MakerMap;

static MakerMap
makers()
{
    static MakerMap map;
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (map.empty()) {
#define MAP(x) map[qtypePrefix + #x] = new Maker<x>();
	MAP(QMainWindow);
	MAP(QFrame);
	MAP(QLabel);
	MAP(QGridLayout);
	MAP(QVBoxLayout);
	MAP(QMenu);
	MAP(QMenuBar);
#undef MAP
#define MAP(x) map[qtypePrefix + #x] = new Maker0<x>();
	MAP(QAction);
#undef MAP
    }
    return map;
}

#include <QApplication>

using namespace Dataquay;

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

    QHash<QString, QObject *> uriObjectMap;

    foreach (Triple t, candidates) {

	if (t.a.type != Node::URI || t.c.type != Node::URI) continue;

	QString u = t.c.value;
	if (!u.startsWith(qtypePrefix)) continue;

	if (!mo.contains(u)) {
	    std::cerr << "WARNING: Unknown object URI <"
		      << u.toStdString() << ">" << std::endl;
	    continue;
	}

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

    return app.exec();
}

