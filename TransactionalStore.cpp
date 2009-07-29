/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "TransactionalStore.h"
#include "RDFException.h"
#include "Debug.h"

#include <QMutex>
#include <QMutexLocker>
#include <QUrl>

#include <iostream>
#include <memory> // auto_ptr

using std::auto_ptr;

namespace Dataquay
{
	
class TransactionalStore::D
{
    /**
     * Current store context.
     *
     * TxContext: changes pending for the transaction are present in the
     * store (committing the transaction would be a no-op;
     * non-transactional queries would return incorrect results).
     *
     * NonTxContext: changes pending for the transaction are absent from
     * the store (store is ready for non-transactional queries;
     * committing the transaction would require reapplying changes).
     */
    enum Context { TxContext, NonTxContext };

public:
    D(Store *store, DirectWriteBehaviour dwb) :
        m_store(store),
        m_dwb(dwb),
        m_currentTx(NoTransaction),
        m_context(NonTxContext) {
    }
    
    ~D() {
        if (m_currentTx != NoTransaction) {
            std::cerr << "WARNING: TransactionalStore deleted with transaction ongoing" << std::endl;
        }
    }

    Transaction *startTransaction() {
        QMutexLocker locker(&m_mutex);
        DEBUG << "TransactionalStore::startTransaction" << endl;
        if (m_currentTx != NoTransaction) {
            throw RDFException("ERROR: Attempt to start transaction when another transaction from the same thread is already in train");
        }
        Transaction *tx = new TSTransaction(this);
        m_currentTx = tx;
        return tx;
    }

    void endTransaction(Transaction *tx) {
        QMutexLocker locker(&m_mutex);
        DEBUG << "TransactionalStore::endTransaction" << endl;
        if (tx != m_currentTx) {
            throw RDFException("Transaction integrity error");
        }
        enterTransactionContext();
        // The store is now in transaction context, which means its
        // changes have been committed; resetting m_currentTx now
        // ensures they will remain committed.  Reset m_context as
        // well for symmetry with the initial state in the
        // constructor, though it shouldn't be necessary
        m_currentTx = NoTransaction;
        m_context = NonTxContext;
    }

    class Operation
    {
    public:
        Operation(const D *d, const Transaction *tx) :
            m_d(d), m_tx(tx) {
            m_d->startOperation(m_tx);
        }
        ~Operation() {
            m_d->endOperation(m_tx);
        }
    private:
        const D *m_d;
        const Transaction *m_tx;
    };

    class NonTransactionalAccess
    {
    public:
        NonTransactionalAccess(D *d) :
            m_d(d) {
            m_d->startNonTransactionalAccess();
        }
        ~NonTransactionalAccess() {
            m_d->endNonTransactionalAccess();
        }
    private:
        D *m_d;
    };

    bool add(Transaction *tx, Triple t) {
        Operation op(this, tx);
        bool result = m_store->add(t);
        return result;
    }

    bool remove(Transaction *tx, Triple t) {
        Operation op(this, tx);
        bool result = m_store->remove(t);
        return result;
    }

    bool contains(const Transaction *tx, Triple t) const {
        Operation op(this, tx);
        bool result = m_store->contains(t);
        return result;
    }

    Triples match(const Transaction *tx, Triple t) const {
        Operation op(this, tx);
        Triples result = m_store->match(t);
        return result;
    }

    ResultSet query(const Transaction *tx, QString sparql) const {
        Operation op(this, tx);
        ResultSet result = m_store->query(sparql);
        return result;
    }

    Triple matchFirst(const Transaction *tx, Triple t) const {
        Operation op(this, tx);
        Triple result = m_store->matchFirst(t);
        return result;
    }

    Node queryFirst(const Transaction *tx, QString sparql,
                        QString bindingName) const {
        Operation op(this, tx);
        Node result = m_store->queryFirst(sparql, bindingName);
        return result;
    }

    QUrl getUniqueUri(const Transaction *tx, QString prefix) const {
        Operation op(this, tx);
        QUrl result = m_store->getUniqueUri(prefix);
        return result;
    }

    QUrl expand(QString uri) const {
        return m_store->expand(uri);
    }
        
    bool hasWrap() const {
        return m_dwb == AutoTransaction;
    }

    void startNonTransactionalAccess() {
        // This is only called from the containing TransactionalStore
        // when it wants to carry out a non-transactional read access.
        // Hence, it needs to take a lock and hold it until
        // endNonTransactionalAccess is called
        m_mutex.lock();
        DEBUG << "TransactionalStore::startNonTransactionalAccess" << endl;
        if (m_context == NonTxContext) {
            return;
        }
        if (m_currentTx == NoTransaction) {
            m_context = NonTxContext;
            return;
        }
        ChangeSet cs = m_currentTx->getChanges();
        if (!cs.empty()) {
            try {
                m_store->revert(cs);
            } catch (RDFException e) {
                throw RDFException(QString("Failed to leave transaction context.  Has the store been modified non-transactionally while a transaction was in progress?  Original error is: %1").arg(e.what()));
            }
        }
        m_context = NonTxContext;
        // return with mutex held
    }

    void endNonTransactionalAccess() {
        // We can remain in NonTxContext, since every transactional
        // access checks this via enterTransactionContext before doing
        // any work; this way is quicker if we may have multiple
        // non-transactional reads happening together.
        DEBUG << "TransactionalStore::endNonTransactionalAccess" << endl;
        m_mutex.unlock();
    }

    Store *getStore() { return m_store; }
    const Store *getStore() const { return m_store; }
    
private:
    // Most things are mutable here because the TransactionalStore
    // manipulates the Store extensively when entering and leaving
    // transaction context, which can happen on any supposedly
    // read-only access
    mutable Store *m_store;
    DirectWriteBehaviour m_dwb;
    mutable QMutex m_mutex;
    const Transaction *m_currentTx;
    mutable Context m_context;

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
        enterTransactionContext();
    }

    void endOperation(const Transaction *tx) const {
        if (tx != m_currentTx) {
            throw RDFException("Transaction integrity error");
        }            
        m_mutex.unlock();
    }

    void enterTransactionContext() const {
        // This is always called from within this class, with the
        // mutex held already (compare leaveTransactionContext())
        if (m_context == TxContext) {
            return;
        }
        if (m_currentTx == NoTransaction) {
            return;
        }
        ChangeSet cs = m_currentTx->getChanges();
        if (!cs.empty()) {
            DEBUG << "TransactionalStore::enterTransactionContext: replaying" << endl;
            try {
                m_store->change(cs);
            } catch (RDFException e) {
                throw RDFException(QString("Failed to enter transaction context.  Has the store been modified non-transactionally while a transaction was in progress?  Original error is: %1").arg(e.what()));
            }
        }
        m_context = TxContext;
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

TransactionalStore::TransactionalStore(Store *store, DirectWriteBehaviour dwb) :
    m_d(new D(store, dwb))
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
    if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::add() called without Transaction");
    }
    // auto_ptr here is very useful to ensure destruction on exceptions
    auto_ptr<Transaction> tx(startTransaction());
    return tx->add(t);
}

bool
TransactionalStore::remove(Triple t)
{
    if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::remove() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    return tx->remove(t);
}

void
TransactionalStore::change(ChangeSet cs)
{
    if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::change() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    tx->change(cs);
}

void
TransactionalStore::revert(ChangeSet cs)
{
    if (!m_d->hasWrap()) {
        throw RDFException("TransactionalStore::revert() called without Transaction");
    }
    auto_ptr<Transaction> tx(startTransaction());
    tx->revert(cs);
}

bool
TransactionalStore::contains(Triple t) const
{
    D::NonTransactionalAccess ntxa(m_d);
    bool result = m_d->getStore()->contains(t);
    return result;
}
    
Triples
TransactionalStore::match(Triple t) const
{
    D::NonTransactionalAccess ntxa(m_d);
    Triples result = m_d->getStore()->match(t);
    return result;
}

ResultSet
TransactionalStore::query(QString s) const
{
    D::NonTransactionalAccess ntxa(m_d);
    ResultSet result = m_d->getStore()->query(s);
    return result;
}

Triple
TransactionalStore::matchFirst(Triple t) const
{
    D::NonTransactionalAccess ntxa(m_d);
    Triple result = m_d->getStore()->matchFirst(t);
    return result;
}

Node
TransactionalStore::queryFirst(QString s, QString b) const
{
    D::NonTransactionalAccess ntxa(m_d);
    Node result = m_d->getStore()->queryFirst(s, b);
    return result;
}

QUrl
TransactionalStore::getUniqueUri(QString prefix) const
{
    D::NonTransactionalAccess ntxa(m_d);
    QUrl result = m_d->getStore()->getUniqueUri(prefix);
    return result;
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

