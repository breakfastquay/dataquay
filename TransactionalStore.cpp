/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "TransactionalStore.h"
#include "RDFException.h"

#include <QMutex>
#include <QUrl>

#include <iostream>
#include <memory> // auto_ptr

using std::auto_ptr;

namespace Dataquay
{
	
class TransactionalStore::D
{
public:
    D(Store *store,
      TransactionStrictness strictness,
      TransactionlessBehaviour txless) :
        m_store(store),
        m_strictness(strictness),
        m_txless(txless),
        m_mutex(QMutex::Recursive), // important: lock in both startTx & startOp
        m_currentTx(0) {
    }
    
    ~D() {
        if (m_currentTx != 0) {
            std::cerr << "WARNING: TransactionalStore deleted with transaction ongoing" << std::endl;
        }
    }

    Transaction *startTransaction() {
        m_mutex.lock(); // not a QMutexLocker, see below
        if (m_currentTx != 0) {
            m_mutex.unlock();
            throw RDFException("ERROR: Attempt to start transaction when another transaction from the same thread is already in train");
        }
        Transaction *tx = new TSTransaction(this);
        m_currentTx = tx;
        // leave with mutex locked for this thread
        return tx;
    }

    void endTransaction(Transaction *tx) {
        startOperation(tx);
        if (tx != m_currentTx) throw RDFException("Transaction integrity error");
        endOperation(tx);
        m_currentTx = 0;
        m_mutex.unlock();
    }
    
    bool add(Transaction *tx, Triple t) {
        startOperation(tx);
        bool result = m_store->add(t);
        endOperation(tx);
        return result;
    }

    bool remove(Transaction *tx, Triple t) {
        startOperation(tx);
        bool result = m_store->remove(t);
        endOperation(tx);
        return result;
    }

    bool contains(const Transaction *tx, Triple t) const {
        startOperation(tx);
        bool result = m_store->contains(t);
        endOperation(tx);
        return result;
    }

    Triples match(const Transaction *tx, Triple t) const {
        startOperation(tx);
        Triples result = m_store->match(t);
        endOperation(tx);
        return result;
    }

    ResultSet query(const Transaction *tx, QString sparql) const {
        startOperation(tx);
        ResultSet result = m_store->query(sparql);
        endOperation(tx);
        return result;
    }

    Triple matchFirst(const Transaction *tx, Triple t) const {
        startOperation(tx);
        Triple result = m_store->matchFirst(t);
        endOperation(tx);
        return result;
    }

    Node queryFirst(const Transaction *tx, QString sparql,
                        QString bindingName) const {
        startOperation(tx);
        Node result = m_store->queryFirst(sparql, bindingName);
        endOperation(tx);
        return result;
    }

    QUrl getUniqueUri(const Transaction *tx, QString prefix) const {
        startOperation(tx);
        QUrl result = m_store->getUniqueUri(prefix);
        endOperation(tx);
        return result;
    }

    QUrl expand(QString uri) const {
        return m_store->expand(uri);
    }
        
    bool hasRelaxedRead() const {
        return m_strictness == TxStrictWrite || m_strictness == TxRelaxed;
    }

    bool hasRelaxedWrite() const {
        return m_strictness == TxRelaxed;
    }

    bool hasWrap() const {
        return m_txless == NoTxWrap;
    }

    Store *getStore() { return m_store; }
    const Store *getStore() const { return m_store; }
    
private:
    Store *m_store;
    TransactionStrictness m_strictness;
    TransactionlessBehaviour m_txless;
    mutable QMutex m_mutex;
    const Transaction *m_currentTx;

    void startOperation(const Transaction *tx) const {
        // This will succeed immediately if the mutex is already held
        // by this thread from a startTransaction call (because it is
        // a recursive mutex).  If another thread is performing a
        // transaction, then we have to block until the whole
        // transaction is complete
        m_mutex.lock();
        if (tx != m_currentTx) {
            throw RDFException("Transaction integrity error");
        }
    }

    void endOperation(const Transaction *tx) const {
        if (tx != m_currentTx) {
            throw RDFException("Transaction integrity error");
        }            
        m_mutex.unlock();
    }

};

class TransactionalStore::TSTransaction::D
{
public:
    D(Transaction *tx, TransactionalStore::D *td) : m_tx(tx), m_td(td) { }
    ~D() { m_td->endTransaction(m_tx); }
    
    bool add(Triple t) {
        if (m_td->add(m_tx, t)) {
            m_changes.push_back(Change(AddTriple, t));
            return true;
        } else {
            return false;
        }
    }

    bool remove(Triple t) {
        if (m_td->remove(m_tx, t)) {
            m_changes.push_back(Change(RemoveTriple, t));
            return true;
        } else {
            return false;
        }
    }

    bool contains(Triple t) const { return m_td->contains(m_tx, t); }
    Triples match(Triple t) const { return m_td->match(m_tx, t); }
    ResultSet query(QString sparql) const { return m_td->query(m_tx, sparql); }
    Triple matchFirst(Triple t) const { return m_td->matchFirst(m_tx, t); }
    Node queryFirst(QString sparql, QString bindingName) const {
        return m_td->queryFirst(m_tx, sparql, bindingName);
    }
    QUrl getUniqueUri(QString prefix) const {
        return m_td->getUniqueUri(m_tx, prefix);
    }
    QUrl expand(QString uri) const {
        return m_td->expand(uri);
    }
    ChangeSet getChanges() const { return m_changes; }
        
private:
    Transaction *m_tx;
    TransactionalStore::D *m_td;
    ChangeSet m_changes;
};

TransactionalStore::TransactionalStore(Store *store,
                                       TransactionStrictness strictness,
                                       TransactionlessBehaviour txless) :
    m_d(new D(store, strictness, txless))
{
}

TransactionalStore::~TransactionalStore()
{
    delete m_d;
}

Transaction *
TransactionalStore::startTransaction()
{
    return m_d->startTransaction();
}

bool
TransactionalStore::add(Triple t)
{
    if (m_d->hasRelaxedWrite()) {
        m_d->getStore()->add(t);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::add() called without Transaction");
    }
    // auto_ptr here is very useful to ensure destruction on exceptions
    auto_ptr<Transaction> tx(startTransaction());
    return tx->add(t);
}

bool
TransactionalStore::remove(Triple t)
{
    if (m_d->hasRelaxedWrite()) {
        m_d->getStore()->remove(t);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::remove() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    return tx->remove(t);
}

void
TransactionalStore::change(ChangeSet cs)
{
    if (m_d->hasRelaxedWrite()) {
        m_d->getStore()->change(cs);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::change() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    tx->change(cs);
}

void
TransactionalStore::revert(ChangeSet cs)
{
    if (m_d->hasRelaxedWrite()) {
        m_d->getStore()->revert(cs);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::revert() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    tx->revert(cs);
}

bool
TransactionalStore::contains(Triple t) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->contains(t);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::contains() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->contains(t);
}
    
Triples
TransactionalStore::match(Triple t) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->match(t);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::match() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->match(t);
}

ResultSet
TransactionalStore::query(QString s) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->query(s);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::query() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->query(s);
}

Triple
TransactionalStore::matchFirst(Triple t) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->matchFirst(t);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::matchFirst() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->matchFirst(t);
}

Node
TransactionalStore::queryFirst(QString s, QString b) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->queryFirst(s, b);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::queryFirst() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->queryFirst(s, b);
}

QUrl
TransactionalStore::getUniqueUri(QString prefix) const
{
    if (m_d->hasRelaxedRead()) {
        return m_d->getStore()->getUniqueUri(prefix);
    } else if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::getUniqueUri() called without Transaction");
    }
    auto_ptr<const Transaction> tx
        (const_cast<TransactionalStore *>(this)->startTransaction());
    return tx->getUniqueUri(prefix);
}

QUrl
TransactionalStore::expand(QString uri) const
{
    return m_d->expand(uri);
}

TransactionalStore::TSTransaction::TSTransaction(TransactionalStore::D *td) :
    m_d(new D(this, td))
{
}

TransactionalStore::TSTransaction::~TSTransaction()
{
    delete m_d;
}

bool
TransactionalStore::TSTransaction::add(Triple t)
{
    return m_d->add(t);
}

bool
TransactionalStore::TSTransaction::remove(Triple t)
{
    return m_d->remove(t);
}

void
TransactionalStore::TSTransaction::change(ChangeSet cs)
{
    // this is all atomic anyway (as it's part of the transaction), so
    // unlike BasicStore we don't need a lock here
    for (int i = 0; i < cs.size(); ++i) {
        ChangeType type = cs[i].first;
        switch (type) {
        case AddTriple:
            if (!add(cs[i].second)) {
                throw RDFException("Change add failed due to duplication");
            }
            break;
        case RemoveTriple:
            if (!remove(cs[i].second)) {
                throw RDFException("Change remove failed due to absence");
            }
            break;
        }
    }
}

void
TransactionalStore::TSTransaction::revert(ChangeSet cs)
{
    // this is all atomic anyway (as it's part of the transaction), so
    // unlike BasicStore we don't need a lock here
    for (int i = cs.size()-1; i >= 0; --i) {
        ChangeType type = cs[i].first;
        switch (type) {
        case AddTriple:
            if (!remove(cs[i].second)) {
                throw RDFException("Change revert add failed due to absence");
            }
            break;
        case RemoveTriple:
            if (!add(cs[i].second)) {
                throw RDFException("Change revert remove failed due to duplication");
            }
            break;
        }
    }
}

bool
TransactionalStore::TSTransaction::contains(Triple t) const
{
    return m_d->contains(t);
}

Triples
TransactionalStore::TSTransaction::match(Triple t) const
{
    return m_d->match(t);
}

ResultSet
TransactionalStore::TSTransaction::query(QString sparql) const
{
    return m_d->query(sparql);
}

Triple
TransactionalStore::TSTransaction::matchFirst(Triple t) const
{
    return m_d->matchFirst(t);
}

Node
TransactionalStore::TSTransaction::queryFirst(QString sparql,
                                                  QString bindingName) const
{
    return m_d->queryFirst(sparql, bindingName);
}

QUrl
TransactionalStore::TSTransaction::getUniqueUri(QString prefix) const
{
    return m_d->getUniqueUri(prefix);
}

QUrl
TransactionalStore::TSTransaction::expand(QString uri) const
{
    return m_d->expand(uri);
}

ChangeSet
TransactionalStore::TSTransaction::getChanges() const
{
    return m_d->getChanges();
}

}

