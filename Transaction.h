/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_TRANSACTION_H_
#define _RDF_TRANSACTION_H_

#include "Store.h"

namespace Dataquay
{

/**
 * \class Transaction Transaction.h <dataquay/Transaction.h>
 *
 * Interface for reading and writing an RDF Store within the context
 * of an atomic operation such as an editing command.  The Transaction
 * interface provides the same editing operations as the Store
 * interface; it subclasses from Store and may be interchanged with
 * Store in most contexts.
 */
class Transaction : public Store
{
public:
    /**
     * Return the ChangeSet applied so far by this transaction.  This
     * is undefined if the transaction has been rolled back (it may
     * return the changes prior to rollback, or an empty ChangeSet).
     */
    virtual ChangeSet getChanges() const = 0;

    /**
     * Roll back this transaction.  All changes made during the
     * transaction will be discarded.  You should not attempt to use
     * the Transaction object again (except to delete it) after this
     * call is made.  Any further call to the transaction's Store
     * interface will throw an RDFException.  When the transaction is
     * deleted, it will simply be discarded rather than being
     * committed.
     */
    virtual void rollback() = 0;

    /**
     * End the transaction which this store object was handling, and
     * (of course) delete the object.  You must delete this object
     * before beginning any other transaction, or that transaction
     * will be unable to begin.
     */
    virtual ~Transaction() { }
};

extern Transaction *const NoTransaction;

}

#endif
