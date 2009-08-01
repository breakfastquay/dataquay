/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_TRANSACTIONAL_CONNECTION_H_
#define _RDF_TRANSACTIONAL_CONNECTION_H_

#include "Store.h"

namespace Dataquay
{
	
class TransactionalStore;
class Transaction;

class TransactionalConnection : public Store
{
public:
    TransactionalConnection(TransactionalStore *ts);
    ~TransactionalConnection();

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
    
    // My own interface
    void commit();
    void rollback();

private:
    //!!! want m_d
    TransactionalStore *m_ts;
    Transaction *m_tx;
    Store *getStore() const;
    void start();
};

}

#endif
