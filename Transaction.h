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

#ifndef _DATAQUAY_TRANSACTION_H_
#define _DATAQUAY_TRANSACTION_H_

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
     * return the changes prior to rollback, or an empty ChangeSet, or
     * something else).
     *!!! FIX -- TransactionalStore depends on getChanges returning the changes prior to rollback, in leaveTransactionContext -- it's called after rollback if a transaction is being rolled back
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
