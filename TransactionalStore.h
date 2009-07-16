/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_TRANSACTIONAL_STORE_H_
#define _RDF_TRANSACTIONAL_STORE_H_

#include "Transaction.h"

namespace Dataquay
{
	
/**
 * \class TransactionalStore TransactionalStore.h <dataquay/TransactionalStore.h>
 *
 * RDF data store implementing the Store interface, providing
 * transaction support as a wrapper around a non-transactional store
 * such as a BasicStore.  Access to the store (other than "admin"
 * roles such as addPrefix) is permitted only in the context of a
 * transaction.  You can start a transaction explicitly using
 * startTransaction. If you call a Store method directly on
 * TransactionalStore, it will be carried out using a single-operation
 * transaction.
 *
 * Asking to start a transaction will ensure you receive a Transaction
 * object for use for the duration of your transaction; this provides
 * the Store interface.  Call end() on that Transaction when complete.
 *
 * Transactions are serialised (one thread only may engage in a
 * transaction at a time).  The implementation is very simplistic and
 * is not really "atomic" in any sense other than serialisation.
 */
class TransactionalStore : public Store
{
public:
    /**
     * The "strictness" of a TransactionalStore controls how it
     * behaves when its Store functions are called directly, as
     * opposed to through a Transaction.  (Normally you should call
     * startTransaction on the store, then call Store methods through
     * the Transaction that was returned by that function.)
     *
     * If the TransactionalStore is Strict (the default), then calling
     * its Store functions directly will result in a new single-use
     * Transaction being created and used internally for each call.
     *
     * If the TransactionalStore is Relaxed, then calling its Store
     * functions directly will result in calls being made directly to
     * the underlying Store without any transactional integrity, even
     * if a Transaction is ongoing elsewhere.
     *
     * This is a very substantial difference because of Transaction's
     * simplistic implementation -- it effectively locks the store for
     * read or write by all other transactions until the transaction
     * is completed.  So a Strict datastore will be unable to carry
     * out any non-transactional activities a transaction is ongoing,
     * while a Relaxed datastore will be able to (at the expense of
     * transactional isolation).
     */
    enum Strictness {
        Strict,
        Relaxed
    };
    
    /**
     * Create a TransactionalStore operating on the given (presumably
     * non-transactional) data store.
     *
     * Nothing in the code prevents the given _underlying_ store being
     * used non-transactionally elsewhere at the same time.  Don't do
     * that: once you have set up a transactional store, you should
     * use it for all routine accesses to the underlying store.
     */
    TransactionalStore(Store *store, Strictness = Strict);
    
    ~TransactionalStore();

    /**
     * Start a transaction and obtain a Transaction through which to
     * carry out its operations.  Once the transaction is complete,
     * you must call delete the Transaction.
     */
    Transaction *startTransaction();

    // Store interface
    bool add(Triple t);
    bool remove(Triple t);
    void change(ChangeSet changes);
    void revert(ChangeSet changes);
    bool contains(Triple t) const;
    Triples match(Triple t) const;
    ResultSet query(QString sparql) const;
    Triple matchFirst(Triple t) const;
    Node queryFirst(QString sparql, QString bindingName) const;
    QUrl getUniqueUri(QString prefix) const;
    QUrl expand(QString uri) const;

private:
    class D;
    D *m_d;

    class TSTransaction : public Transaction
    {
    public:
        // Store interface
        bool add(Triple t);
        bool remove(Triple t);
        void change(ChangeSet changes);
        void revert(ChangeSet changes);
        bool contains(Triple t) const;
        Triples match(Triple t) const;
        ResultSet query(QString sparql) const;
        Triple matchFirst(Triple t) const;
        Node queryFirst(QString sparql, QString bindingName) const;
        QUrl getUniqueUri(QString prefix) const;
        QUrl expand(QString uri) const;

        // Transaction interface
        ChangeSet getChanges() const;
        
        TSTransaction(TransactionalStore::D *td);
        virtual ~TSTransaction();

    private:
        class D;
        D *m_d;
    };
};

}

#endif
