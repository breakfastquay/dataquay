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
 * startTransaction.
 * 
 * If you call a Store method directly on TransactionalStore, the
 * results depend on the TransactionStrictness and
 * TransactionlessBehaviour settings.
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
     * TransactionStrictness controls the extent to which
     * TransactionalStore permits access without a Transaction, that
     * is, by calling Store methods directly on the TransactionalStore
     * object.
     *
     * If TransactionStrictness is TxStrict, all read or write access
     * to the TransactionalStore must use a Transaction.  Accesses
     * directly to the store will either fail or be handled through a
     * single-use Transaction, depending on the current
     * TransactionlessBehaviour setting.
     *
     * If TransactionStrictness is TxStrictWrite (the default), all
     * modifying accesses to the TransactionalStore must use a
     * Transaction, but read-only access directly to the
     * TransactionalStore is permitted.  Modifying accesses directly
     * to the store will either fail or be handled through a
     * single-use Transaction, depending on the current
     * TransactionlessBehaviour setting.
     *
     * If TransactionStrictness is TxRelaxed, full access will be
     * permitted directly to the TransactionalStore without any
     * transaction as well as through a Transaction, with obvious
     * consequences for transactional integrity.
     */
    enum TransactionStrictness {
        TxStrict,
        TxStrictWrite,
        TxRelaxed
    };
    
    /**
     * TransactionlessBehaviour controls how TransactionalStore
     * responds when called directly (not through a Transaction) in a
     * context in which a transaction is required by the current
     * TransactionStrictness setting.
     *
     * If TransactionlessBehaviour is NoTxWrap, a Transaction object
     * will be created, used for the single access, and then closed.
     * This may cause a deadlock if another transaction is already
     * ongoing elsewhere.
     *
     * If TransactionlessBehaviour is NoTxFail (the default), an RDF
     * exception will be thrown whenever a transaction is required but
     * not used.
     */
    enum TransactionlessBehaviour {
        NoTxWrap,
        NoTxFail
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
    TransactionalStore(Store *store,
                       TransactionStrictness = TxStrictWrite,
                       TransactionlessBehaviour = NoTxFail);
    
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
