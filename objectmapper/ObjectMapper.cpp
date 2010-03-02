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

#include "ObjectMapper.h"

#include "ObjectLoader.h"
#include "ObjectStorer.h"

#include "TypeMapping.h"

namespace Dataquay {

class
ObjectMapper::Network::D
{
public:
    ObjectLoader::NodeObjectMap nodeObjectMap;
    ObjectStorer::ObjectNodeMap objectNodeMap;
};

ObjectMapper::Network::Network() :
    m_d(new D())
{
}

ObjectMapper::Network::Network(const Network &n) :
    m_d(new D(*n.m_d))
{
}

ObjectMapper::Network &
ObjectMapper::Network::operator=(const Network &n)
{
    if (&n != this) {
        delete m_d;
        m_d = new D(*n.m_d);
    }
    return *this;
}

ObjectMapper::Network::~Network()
{
    delete m_d;
}

class
ObjectMapper::D
{
public:
    D(ObjectMapper *m, Store *s) :
        m_m(m),
        m_s(s)
    {
        m_loader = new ObjectLoader(m_s);
        m_storer = new ObjectStorer(m_s);
    }

    Store *getStore() {
        return m_s;
    }

    void setTypeMapping(const TypeMapping &tm) {
        m_tm = tm;
        m_loader->setTypeMapping(m_tm);
        m_storer->setTypeMapping(m_tm);
    }

    const TypeMapping &getTypeMapping() const {
        return m_tm;
    }

private:
    ObjectMapper *m_m;
    Store *m_s;
    TypeMapping m_tm;

    ObjectLoader *m_loader;
    ObjectStorer *m_storer;
};
    

}


	
