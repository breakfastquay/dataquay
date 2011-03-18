/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2010 Chris Cannam.
  
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

#include "Connection.h"

#include "TransactionalStore.h"
#include "Transaction.h"

namespace Dataquay
{

class Connection::D
{
public:
    D(TransactionalStore *ts);
    ~D();

    bool add(Triple t);
    bool remove(Triple t);
    void change(ChangeSet changes);
    void revert(ChangeSet changes);
    bool contains(Triple t) const;
    Triples match(Triple t) const;
    ResultSet query(QString sparql) const;
    Triple matchFirst(Triple t) const;
    Node queryFirst(QString sparql, QString bindingName) const;
    Uri getUniqueUri(QString prefix) const;
    Node addBlankNode();
    Uri expand(QString uri) const;
    
    void commit();
    void commit(ChangeSet &cs);
    void rollback();

private:
    TransactionalStore *m_ts;
    Transaction *m_tx;

    Store *getStore() const;
    void start();
};

Connection::D::D(TransactionalStore *ts) :
    m_ts(ts),
    m_tx(NoTransaction)
{
}

Connection::D::~D()
{
    commit();
}

bool
Connection::D::add(Triple t)
{
    start();
    return m_tx->add(t);
}

bool
Connection::D::remove(Triple t)
{
    start();
    return m_tx->remove(t);
}

void
Connection::D::change(ChangeSet cs)
{
    start();
    return m_tx->change(cs);
}

void
Connection::D::revert(ChangeSet cs)
{
    start();
    return m_tx->revert(cs);
}

void
Connection::D::start()
{
    if (m_tx != NoTransaction) return;
    m_tx = m_ts->startTransaction();
}

void
Connection::D::commit()
{
    if (m_tx) {
        m_tx->commit();
        delete m_tx;
        m_tx = NoTransaction;
    }
}

void
Connection::D::commit(ChangeSet &cs)
{
    if (m_tx) {
        m_tx->commit();
        cs = m_tx->getCommittedChanges();
        delete m_tx;
        m_tx = NoTransaction;
    }
}

void
Connection::D::rollback()
{
    if (m_tx) {
	m_tx->rollback();
	delete m_tx;
	m_tx = NoTransaction;
    }
}

Store *
Connection::D::getStore() const
{
    if (m_tx) return m_tx;
    else return m_ts;
}

bool
Connection::D::contains(Triple t) const
{
    return getStore()->contains(t);
}

Triples
Connection::D::match(Triple t) const
{
    return getStore()->match(t);
}

ResultSet
Connection::D::query(QString sparql) const
{
    return getStore()->query(sparql);
}

Triple
Connection::D::matchFirst(Triple t) const
{
    return getStore()->matchFirst(t);
}

Node
Connection::D::queryFirst(QString sparql, QString bindingName) const
{
    return getStore()->queryFirst(sparql, bindingName);
}

Uri
Connection::D::getUniqueUri(QString prefix) const
{
    return getStore()->getUniqueUri(prefix);
}

Node
Connection::D::addBlankNode()
{
    start();
    return m_tx->addBlankNode();
}

Uri
Connection::D::expand(QString uri) const
{
    return getStore()->expand(uri);
}

Connection::Connection(TransactionalStore *ts) :
    m_d(new D(ts))
{
    connect(ts, SIGNAL(transactionCommitted(const ChangeSet &)),
            this, SIGNAL(transactionCommitted(const ChangeSet &)));

    connect(ts, SIGNAL(transactionCommitted()),
            this, SIGNAL(transactionCommitted()));
}

Connection::~Connection()
{
    delete m_d;
}

bool
Connection::add(Triple t)
{
    return m_d->add(t);
}

bool 
Connection::remove(Triple t)
{
    return m_d->remove(t);
}

void 
Connection::change(ChangeSet changes)
{
    m_d->change(changes);
}

void 
Connection::revert(ChangeSet changes)
{
    m_d->revert(changes);
}

bool 
Connection::contains(Triple t) const
{
    return m_d->contains(t);
}

Triples 
Connection::match(Triple t) const
{
    return m_d->match(t);
}

ResultSet 
Connection::query(QString sparql) const
{
    return m_d->query(sparql);
}

Triple 
Connection::matchFirst(Triple t) const
{
    return m_d->matchFirst(t);
}

Node 
Connection::queryFirst(QString sparql, QString bindingName) const
{
    return m_d->queryFirst(sparql, bindingName);
}

Uri 
Connection::getUniqueUri(QString prefix) const
{
    return m_d->getUniqueUri(prefix);
}

Node
Connection::addBlankNode()
{
    return m_d->addBlankNode();
}

Uri 
Connection::expand(QString uri) const
{
    return m_d->expand(uri);
}

void 
Connection::commit()
{
    m_d->commit();
}

void 
Connection::commit(ChangeSet &cs)
{
    m_d->commit(cs);
}

void 
Connection::rollback()
{
    m_d->rollback();
}

}

