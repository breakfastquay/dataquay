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
     * Return the ChangeSet applied so far by this transaction.
     */
    virtual ChangeSet getChanges() const = 0;

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
