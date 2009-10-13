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

#ifndef _DATAQUAY_TRIPLE_H_
#define _DATAQUAY_TRIPLE_H_

#include "Node.h"

namespace Dataquay
{

/**
 * \class Triple Triple.h <dataquay/Triple.h>
 *
 * A triple made up of three Node objects.
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

    /**
     * Construct a triple of Node, URI, and an arbitrary node.
     * 
     * This constructor simplifies constructing complete RDF
     * statements -- with URI, URI, and URI or Literal nodes.
     */
    Triple(Node _a, QString b_uri, Node _c) :
        a(_a),
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
