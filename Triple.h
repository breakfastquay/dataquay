/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_TRIPLE_H_
#define _RDF_TRIPLE_H_

#include "Node.h"

namespace Dataquay
{

/**
 * \class Triple Triple.h <dataquay/Triple.h>
 */
class Triple
{
public:
    /**
     * Construct a triple of three Nothing nodes.
     */
    Triple() { }

    /**
     * Construct a triple of the three given nodes.
     *
     * Our triples may contain anything, including the Nothing node
     * type for undefined elements (used in wildcard matching, etc).
     *
     * However, in order to be inserted in the RDF store, a triple
     * must consist of URI, URI, and either URI or Literal types, in
     * that order.  (A complete RDF statement may have either URI or
     * blank node as its subject, but we don't handle blank nodes
     * properly in this wrapper.)  See the following constructor for
     * some simple assistance in constructing those.
     */
    Triple(Node _a, Node _b, Node _c) :
        a(_a), b(_b), c(_c) { }

    /**
     * Construct a triple of two URIs and an arbitrary node.
     * 
     * This constructor simplifies constructing complete RDF
     * statements -- with URI, URI, and URI or Literal nodes.
     */
    Triple(QString a_uri, QString b_uri, Node _c) :
        a(Node::URI, a_uri),
        b(Node::URI, b_uri),
        c(_c) { }

    ~Triple() { }

    Node a;
    Node b;
    Node c;
};

bool operator==(const Triple &a, const Triple &b);
bool operator!=(const Triple &a, const Triple &b);
    
QDataStream &operator<<(QDataStream &out, const Triple &);
QDataStream &operator>>(QDataStream &in, Triple &);

std::ostream &operator<<(std::ostream &out, const Triple &);
QTextStream &operator<<(QTextStream &out, const Triple &);

}
 
#endif
