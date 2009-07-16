/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "TransactionalCommand.h"
#include "TransactionalStore.h"

#include <memory>

using std::auto_ptr;

namespace Dataquay
{

TransactionalCommand::TransactionalCommand(TransactionalStore *store) :
    m_store(store),
    m_haveCs(false)
{
}

void
TransactionalCommand::execute()
{
    if (m_haveCs) {
        m_store->change(m_cs);
        return;
    }
    auto_ptr<Transaction> tx(m_store->startTransaction());
    performCommand(tx.get());
    m_cs = tx->getChanges();
    m_haveCs = true;
}

void
TransactionalCommand::unexecute()
{
    m_store->revert(m_cs);
}

}



