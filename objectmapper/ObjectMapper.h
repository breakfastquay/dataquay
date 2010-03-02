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

#ifndef _DATAQUAY_OBJECT_MAPPER_H_
#define _DATAQUAY_OBJECT_MAPPER_H_

#include <QObject>

namespace Dataquay
{

class Store;
class TypeMapping;

class ObjectMapper : public QObject
{
    Q_OBJECT

public:
    class Network
    {
    public:
        Network();
        Network(const Network &);
        Network &operator=(const Network &);
        ~Network();

    private:
        friend class ObjectMapper;
        class D;
        D *m_d;
    };
        
    ObjectMapper(Store *s);
    ~ObjectMapper();

    Store *getStore();

    void setTypeMapping(const TypeMapping &);
    const TypeMapping &getTypeMapping() const;

    const Network &getNetwork() const;

public slots:
    void addToNetwork(QObject *);
    void removeFromNetwork(QObject *);
    void remap(QObject *); //!!! poor
    void unmap(QObject *); //!!! poor
    void commit();

private:
    ObjectMapper(const ObjectMapper &);
    ObjectMapper &operator=(const ObjectMapper &);

    class D;
    D *m_d;
};

}

#endif
