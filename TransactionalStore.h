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
 * such as a BasicStore.
 *
 * Write access to the store is permitted only in the context of a
 * transaction.  If you call a modifying function directly on
 * TransactionalStore, the store will either throw RDFException (if
 * set to NoAutoTransaction) or create a single-use Transaction object
 * for the duration of that modification (if set to AutoTransaction).
 * Note that the latter behaviour will deadlock if a transaction is in
 * progress already.
 *
 * Read access may be carried out through a Transaction, in which case
 * the read state will reflect the changes made so far in the pending
 * transaction, or directly on the TransactionalStore, in which case
 * the read will be isolated from any pending transaction.
 *
 * Call startTransaction to obtain a new Transaction object and start
 * its transaction; use the Transaction's Store interface for all
 * accesses associated with that transaction; delete the Transaction
 * object once done, to finish and commit the transaction; or call
 * Transaction::rollback() if you decide you do not wish to commit it.
 */
class TransactionalStore : public Store
{
public:
    /**
     * DirectWriteBehaviour controls how TransactionalStore responds
     * when called directly (not through a Transaction) for a write
     * operation (add, remove, change, or revert).
     *
     * NoAutoTransaction (the default) means that an RDF exception
     * will be thrown whenever a write is attempted without a
     * transaction.
     *
     * AutoTransaction means that a Transaction object will be
     * created, used for the single access, and then closed.  This may
     * cause a deadlock if another transaction is already ongoing
     * elsewhere.
     */
    enum DirectWriteBehaviour {
        NoAutoTransaction,
        AutoTransaction
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
    TransactionalStore(Store *store, DirectWriteBehaviour dwb = NoAutoTransaction);
    
    ~TransactionalStore();

    /**
     * Start a transaction and obtain a Transaction through which to
     * carry out its operations.  Once the transaction is complete,
     * you must delete the Transaction object to finish the
     * transaction.
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
