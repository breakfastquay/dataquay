/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "TransactionalConnection.h"

#include "TransactionalStore.h"
#include "Transaction.h"

namespace Dataquay
{

TransactionalConnection::TransactionalConnection(TransactionalStore *ts) :
    m_ts(ts),
    m_tx(NoTransaction)
{
}

TransactionalConnection::~TransactionalConnection()
{
    commit();
}

bool
TransactionalConnection::add(Triple t)
{
    start();
    return m_tx->add(t);
}

bool
TransactionalConnection::remove(Triple t)
{
    start();
    return m_tx->remove(t);
}

void
TransactionalConnection::change(ChangeSet cs)
{
    start();
    return m_tx->change(cs);
}

void
TransactionalConnection::revert(ChangeSet cs)
{
    start();
    return m_tx->revert(cs);
}

void
TransactionalConnection::start()
{
    if (m_tx != NoTransaction) return;
    m_tx = m_ts->startTransaction();
}

void
TransactionalConnection::commit()
{
    delete m_tx;
    m_tx = NoTransaction;
}

void
TransactionalConnection::rollback()
{
    if (m_tx) {
	m_tx->rollback();
	delete m_tx;
	m_tx = NoTransaction;
    }
}

Store *
TransactionalConnection::getStore() const
{
    if (m_tx) return m_tx;
    else return m_ts;
}

bool
TransactionalConnection::contains(Triple t) const
{
    return getStore()->contains(t);
}

Triples
TransactionalConnection::match(Triple t) const
{
    return getStore()->match(t);
}

ResultSet
TransactionalConnection::query(QString sparql) const
{
    return getStore()->query(sparql);
}

Triple
TransactionalConnection::matchFirst(Triple t) const
{
    return getStore()->matchFirst(t);
}

Node
TransactionalConnection::queryFirst(QString sparql, QString bindingName) const
{
    return getStore()->queryFirst(sparql, bindingName);
}

QUrl
TransactionalConnection::getUniqueUri(QString prefix) const
{
    return getStore()->getUniqueUri(prefix);
}

QUrl
TransactionalConnection::expand(QString uri) const
{
    return getStore()->expand(uri);
}

}

