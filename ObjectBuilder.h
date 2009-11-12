/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009 Chris Cannam.
  
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

#ifndef _DATAQUAY_OBJECT_BUILDER_H_
#define _DATAQUAY_OBJECT_BUILDER_H_

#include <QHash>
#include <QString>
#include <QVariant>

namespace Dataquay {

/**
 * \class ObjectBuilder ObjectBuilder.h <dataquay/ObjectBuilder.h>
 *
 * Singleton object factory capable of constructing new objects of
 * classes that are subclassed from QObject, given the class name as a
 * string, and optionally a parent object.  To be capable of
 * construction using ObjectBuilder, a class must be declared using
 * Q_OBJECT as well as subclassed from QObject.
 *
 * All object classes need to be registered with the builder before
 * they can be constructed; the only class that ObjectBuilder is able
 * to construct without registration is QObject itself.
 *
 * This class permits code to construct new objects dynamically,
 * without needing to know anything about them except for their class
 * names, and without needing their definitions to be visible.  (The
 * definitions must be visible when the object classes are registered,
 * but not when the objects are constructed.)
 */
class ObjectBuilder
{
public:
    /**
     * Retrieve the single global instance of ObjectBuilder.
     */
    static ObjectBuilder *getInstance();

    /**
     * Register type T, a subclass of QObject, as a class that can be
     * constructed by calling a single-argument constructor whose
     * argument is of pointer-to-Parent type, where Parent is also a
     * subclass of QObject.
     *
     * For example, calling registerWithParentConstructor<QWidget,
     * QWidget>() declares that QWidget is a subclass of QObject that
     * may be built using QWidget::QWidget(QWidget *parent).  A
     * subsequent call to ObjectBuilder::build("QWidget", parent)
     * would return a new QWidget built with that constructor (since
     * "QWidget" is the class name of QWidget returned by its meta
     * object).
     *!!! update the above
     */
    template <typename T, typename Parent>
    void registerClass(QString pointerName) {
        m_builders[T::staticMetaObject.className()] = new Builder1<T, Parent>();
        registerExtractor<T>(pointerName);
    }

    /**
     * Register type T, a subclass of QObject, as a class that can be
     * constructed by calling a zero-argument constructor.
     *!!! update the above
     */
    template <typename T>
    void registerClass(QString pointerName) {
        m_builders[T::staticMetaObject.className()] = new Builder0<T>();
        registerExtractor<T>(pointerName);
    }

    /**
     * Register type T, a subclass of QObject, as a class that can be
     * constructed by calling a single-argument constructor whose
     * argument is of pointer-to-Parent type, where Parent is also a
     * subclass of QObject.
     *
     * For example, calling registerWithParentConstructor<QWidget,
     * QWidget>() declares that QWidget is a subclass of QObject that
     * may be built using QWidget::QWidget(QWidget *parent).  A
     * subsequent call to ObjectBuilder::build("QWidget", parent)
     * would return a new QWidget built with that constructor (since
     * "QWidget" is the class name of QWidget returned by its meta
     * object).
     */
    template <typename T, typename Parent>
    void registerClass() {
        m_builders[T::staticMetaObject.className()] = new Builder1<T, Parent>();
    }

    /**
     * Register type T, a subclass of QObject, as a class that can be
     * constructed by calling a zero-argument constructor.
     */
    template <typename T>
    void registerClass() {
        m_builders[T::staticMetaObject.className()] = new Builder0<T>();
    }

    /**
     * Return true if the class whose class name (according to its
     * meta object) is className has been registered.
     */
    bool knows(QString className) {
        return m_builders.contains(className);
    }

    /**
     * Return a new object whose class name (according to its meta
     * object) is className, with the given parent (cast
     * appropriately) passed to its single argument constructor.
     */
    QObject *build(QString className, QObject *parent) {
        if (!knows(className)) return 0;
        return m_builders[className]->build(parent);
    }

    /**
     * Return a new object whose class name (according to its meta
     * object) is className, constructed with no parent.
     */
    QObject *build(QString className) {
        if (!knows(className)) return 0;
        return m_builders[className]->build(0);
    }

    bool canExtract(QString pointerName) {
        return m_extractors.contains(pointerName);
    }

    bool canExtractList(QString listName) {
        return m_listExtractors.contains(listName);
    }

    bool canExtractMap(QString mapName) {
        return m_mapExtractors.contains(mapName);
    }
    
    QObject *extract(QString pointerName, QVariant &v) {
        if (!canExtract(pointerName)) return 0;
        return m_extractors[pointerName]->extract(v);
    }

    QList<QObject *> extractList(QString listName, QVariant &v) {
        if (!canExtractList(listName)) return QList<QObject *>();
        return m_listExtractors[listName]->extractList(v);
    }

    QMap<QString, QObject *> extractMap(QString mapName, QVariant &v) {
        if (!canExtractMap(mapName)) return QMap<QString, QObject *>();
        return m_mapExtractors[mapName]->extractMap(v);
    }

private:
    ObjectBuilder() {
        registerClass<QObject, QObject>("QObject*");
    }
    ~ObjectBuilder() {
        for (BuilderMap::iterator i = m_builders.begin();
             i != m_builders.end(); ++i) {
            delete *i;
        }
        for (ExtractorMap::iterator i = m_extractors.begin();
             i != m_extractors.end(); ++i) {
            delete *i;
        }
        for (ExtractorMap::iterator i = m_listExtractors.begin();
             i != m_listExtractors.end(); ++i) {
            delete *i;
        }
        for (ExtractorMap::iterator i = m_mapExtractors.begin();
             i != m_mapExtractors.end(); ++i) {
            delete *i;
        }
    }

    template <typename T>
    void
    registerExtractor(QString pointerName) {
        ExtractorBase *e = new Extractor<T *>();
        m_extractors[pointerName] = 
        m_listExtractors[QString("QList<%1>").arg(pointerName)] = e;
        m_mapExtractors[QString("QMap<QString,%1>").arg(pointerName)] = e;
    }

    struct BuilderBase {
        virtual QObject *build(QObject *) = 0;
    };

    template <typename T> struct Builder0 : public BuilderBase {
        virtual QObject *build(QObject *) {
            return new T();
        }
    };

    template <typename T, typename Parent> struct Builder1 : public BuilderBase {
        virtual QObject *build(QObject *p) {
            return new T(qobject_cast<Parent *>(p));
        }
    };

    typedef QHash<QString, BuilderBase *> BuilderMap;
    BuilderMap m_builders;

    struct ExtractorBase {
        virtual QObject *extract(QVariant &v) = 0;
        virtual QList<QObject *> extractList(QVariant &v) = 0;
        virtual QMap<QString, QObject *> extractMap(QVariant &v) = 0;
    };

    template <typename Pointer> struct Extractor : public ExtractorBase {
        virtual QObject *extract(QVariant &v) {
            return v.value<Pointer>();
        }
        virtual QList<QObject *> extractList(QVariant &v) {
            return v.value<QList<Pointer> >();
/*
            QVariantList vl = v.toList();
            QObjectList ol;
            foreach (QVariant iv, vl) ol.push_back(extract(iv));
            return ol;
*/
        }
        virtual QMap<QString, QObject *> extractMap(QVariant &v) {
            QVariantMap vm = v.toMap();
            QMap<QString, QObject *> om;
            foreach (QString key, vm.keys()) om[key] = extract(vm[key]);
            return om;
        }
    };

    typedef QHash<QString, ExtractorBase *> ExtractorMap;
    ExtractorMap m_extractors;
    ExtractorMap m_listExtractors;
    ExtractorMap m_mapExtractors;
};

}

#endif
