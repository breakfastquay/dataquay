/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _TURBOT_TRANSACTIONAL_COMMAND_H_
#define _TURBOT_TRANSACTIONAL_COMMAND_H_

#include "Transaction.h"

#include "base/Command.h"

namespace Turbot
{

namespace RDF
{
    
class TransactionalStore;

class TransactionalCommand : public Command
{
public:
    virtual void execute();
    virtual void unexecute();
    
protected:
    TransactionalCommand(TransactionalStore *store);
    virtual void performCommand(Transaction *tx) = 0;

private:
    TransactionalStore *m_store;
    ChangeSet m_cs;
    bool m_haveCs;
};

}

}

#endif
