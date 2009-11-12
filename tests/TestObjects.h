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

#ifndef _TEST_OBJECTS_H_
#define _TEST_OBJECTS_H_

#include <QObject>
#include <QMetaType>
#include <QStringList>

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

    Q_PROPERTY(A *a READ getA WRITE setA STORED true)

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

    Q_PROPERTY(QStringList strings READ getStrings WRITE setStrings STORED true)
    Q_PROPERTY(QList<float> floats READ getFloats WRITE setFloats STORED true)
    Q_PROPERTY(QList<B *> bees READ getBees WRITE setBees STORED true)

public:
    C(QObject *parent = 0) : QObject(parent) { }

    QStringList getStrings() const { return m_strings; }
    void setStrings(QStringList sl) { m_strings = sl; }

    QList<float> getFloats() const { return m_floats; }
    void setFloats(QList<float> fl) { m_floats = fl; }

    QList<B *> getBees() const { return m_bees; }
    void setBees(QList<B *> bl) { m_bees = bl; }

private:
    QStringList m_strings;
    QList<float> m_floats;
    QList<B *> m_bees;
};

Q_DECLARE_METATYPE(C*)
Q_DECLARE_METATYPE(QList<float>)
Q_DECLARE_METATYPE(QList<B*>)

#endif
