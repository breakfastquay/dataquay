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

namespace Dataquay {

class ObjectBuilder
{
public:
    static ObjectBuilder *getInstance();

    template <typename T>
    void registerWithDefaultConstructor() {
        m_map[T::staticMetaObject.className()] = new Builder0<T>();
    }

    template <typename T, typename Parent>
    void registerWithParentConstructor() {
        m_map[T::staticMetaObject.className()] = new Builder1<T, Parent>();
    }

    bool knows(QString className) {
        return m_map.contains(className);
    }

    QObject *build(QString className) {
        if (!knows(className)) return 0;
        return m_map[className]->build(0);
    }

    QObject *build(QString className, QObject *parent) {
        if (!knows(className)) return 0;
        return m_map[className]->build(parent);
    }

private:
    ObjectBuilder() {
        registerWithParentConstructor<QObject, QObject>();
    }
    ~ObjectBuilder() {
        for (BuilderMap::iterator i = m_map.begin(); i != m_map.end(); ++i) {
            delete *i;
        }
    }

    struct BuilderBase {
        virtual QObject *build(QObject *) = 0;
    };

    template <typename T> 
    struct Builder0 : public BuilderBase {
        virtual QObject *build(QObject *) {
            T *t = new T();
            return t;
        }
    };

    template <typename T, typename Parent> 
    struct Builder1 : public BuilderBase {
        virtual QObject *build(QObject *p) {
            return new T(qobject_cast<Parent *>(p));
        }
    };

    typedef QHash<QString, BuilderBase *> BuilderMap;
    BuilderMap m_map;
};

}

#endif
