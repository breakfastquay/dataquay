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

#ifndef _DATAQUAY_CONTAINER_BUILDER_H_
#define _DATAQUAY_CONTAINER_BUILDER_H_

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Dataquay {

/**
 * \class ContainerBuilder ContainerBuilder.h <dataquay/ContainerBuilder.h>
 *
 * Singleton object factory capable of constructing new objects of
 * classes that are subclassed from QObject, given the class name as a
 * string, and optionally a parent object.  To be capable of
 * construction using ContainerBuilder, a class must be declared using
 * Q_OBJECT as well as subclassed from QObject.
 *
 * All object classes need to be registered with the builder before
 * they can be constructed; the only class that ContainerBuilder is able
 * to construct without registration is QObject itself.
 *
 * This class permits code to construct new objects dynamically,
 * without needing to know anything about them except for their class
 * names, and without needing their definitions to be visible.  (The
 * definitions must be visible when the object classes are registered,
 * but not when the objects are constructed.)
 */
class ContainerBuilder
{
public:
    /**
     * Retrieve the single global instance of ContainerBuilder.
     */
    static ContainerBuilder *getInstance();

    enum ContainerKind {
        UnknownKind = 0,
        SequenceKind,   // e.g. list
        SetKind         // e.g. set
        // perhaps also something for maps
    };

    /**
     * Register type T, a subclass of QObject, as a class that can be
     * constructed by calling a single-argument constructor whose
     * argument is of pointer-to-Parent type, where Parent is also a
     * subclass of QObject.
     *
     * For example, calling registerWithParentConstructor<QWidget,
     * QWidget>() declares that QWidget is a subclass of QObject that
     * may be built using QWidget::QWidget(QWidget *parent).  A
     * subsequent call to ContainerBuilder::build("QWidget", parent)
     * would return a new QWidget built with that constructor (since
     * "QWidget" is the class name of QWidget returned by its meta
     * object).
     *!!! update the above
     */
    /*
    template <typename T, typename Parent>
    void registerClass(QString pointerName, QString listName) {
        m_builders[T::staticMetaObject.className()] = new Builder1<T, Parent>();
        registerExtractor<T>(pointerName, listName);
    }
    */
    template <typename T, typename Container>
    void registerContainer(QString typeName, QString containerName,
                           ContainerKind kind) {
        registerContainerExtractor<T, Container>
            (typeName, containerName, kind);
    }

    bool canExtractContainer(QString containerName) {
        return m_containerExtractors.contains(containerName);
    }

    bool canInjectContainer(QString containerName) {
        return m_containerExtractors.contains(containerName);
    }

    QString getTypeNameForContainer(QString containerName) {
        if (!canExtractContainer(containerName)) return QString();
        return m_containerExtractors[containerName]->getTypeName();
    }

    ContainerKind getContainerKind(QString containerName) {
        if (!canExtractContainer(containerName)) return UnknownKind;
        return m_containerExtractors[containerName]->getKind();
    }

    QVariantList extractContainer(QString containerName, const QVariant &v) {
        if (!canExtractContainer(containerName)) return QVariantList();
        return m_containerExtractors[containerName]->extract(v);
    }

    QVariant injectContainer(QString containerName, const QVariantList &vl) {
        if (!canInjectContainer(containerName)) return QVariant();
        return m_containerExtractors[containerName]->inject(vl);
    }

private:
    ContainerBuilder() {
        registerContainer<QString, QStringList>
            ("QString", "QStringList", SequenceKind);
    }
    ~ContainerBuilder() {
        for (ContainerExtractorMap::iterator i = m_containerExtractors.begin();
             i != m_containerExtractors.end(); ++i) {
            delete *i;
        }
    }

    template <typename T, typename Container>
    void
    registerContainerExtractor(QString typeName, QString containerName,
                               ContainerKind kind) {
        m_containerExtractors[containerName] =
            new ContainerExtractor<T, Container>(typeName, kind);
    }

    struct ContainerExtractorBase {
        virtual QVariantList extract(const QVariant &v) = 0;
        virtual QVariant inject(const QVariantList &) = 0;
        virtual QString getTypeName() const = 0;
        virtual ContainerKind getKind() const = 0;
    };

    template <typename T, typename Container>
    struct ContainerExtractor : public ContainerExtractorBase
    {
        ContainerExtractor(QString typeName, ContainerKind kind) :
            m_typeName(typeName), m_kind(kind) { }

        virtual QVariantList extract(const QVariant &v) {
            Container tl = v.value<Container>();
            QVariantList vl;
            foreach (const T &t, tl) vl << QVariant::fromValue<T>(t);
            return vl;
        }
        virtual QVariant inject(const QVariantList &vl) {
            Container tl;
            foreach (const QVariant &v, vl) tl << v.value<T>();
            return QVariant::fromValue<Container>(tl);
        }
        virtual QString getTypeName() const {
            return m_typeName;
        }
        virtual ContainerKind getKind() const {
            return m_kind;
        }

        QString m_typeName;
        ContainerKind m_kind;
    };

    typedef QHash<QString, ContainerExtractorBase *> ContainerExtractorMap;
    ContainerExtractorMap m_containerExtractors;
};

}

#endif
