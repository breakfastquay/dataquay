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

#include "Connection.h"

#include "TransactionalStore.h"
#include "Transaction.h"

namespace Dataquay
{

Connection::Connection(TransactionalStore *ts) :
    m_ts(ts),
    m_tx(NoTransaction)
{
}

Connection::~Connection()
{
    commit();
}

bool
Connection::add(Triple t)
{
    start();
    return m_tx->add(t);
}

bool
Connection::remove(Triple t)
{
    start();
    return m_tx->remove(t);
}

void
Connection::change(ChangeSet cs)
{
    start();
    return m_tx->change(cs);
}

void
Connection::revert(ChangeSet cs)
{
    start();
    return m_tx->revert(cs);
}

void
Connection::start()
{
    if (m_tx != NoTransaction) return;
    m_tx = m_ts->startTransaction();
}

void
Connection::commit()
{
    delete m_tx;
    m_tx = NoTransaction;
}

void
Connection::rollback()
{
    if (m_tx) {
	m_tx->rollback();
	delete m_tx;
	m_tx = NoTransaction;
    }
}

Store *
Connection::getStore() const
{
    if (m_tx) return m_tx;
    else return m_ts;
}

bool
Connection::contains(Triple t) const
{
    return getStore()->contains(t);
}

Triples
Connection::match(Triple t) const
{
    return getStore()->match(t);
}

ResultSet
Connection::query(QString sparql) const
{
    return getStore()->query(sparql);
}

Triple
Connection::matchFirst(Triple t) const
{
    return getStore()->matchFirst(t);
}

Node
Connection::queryFirst(QString sparql, QString bindingName) const
{
    return getStore()->queryFirst(sparql, bindingName);
}

QUrl
Connection::getUniqueUri(QString prefix) const
{
    return getStore()->getUniqueUri(prefix);
}

QUrl
Connection::expand(QString uri) const
{
    return getStore()->expand(uri);
}

}

