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

#ifndef _DATAQUAY_NODE_H_
#define _DATAQUAY_NODE_H_

#include <QString>
#include <QVariant>

#include "Uri.h"

class QDataStream;
class QTextStream;

namespace Dataquay
{

/**
 * \class Node Node.h <dataquay/Node.h>
 *
 * A single RDF node, with conversions to and from variant types.
 */
class Node
{
public:
    /**
     * Node type.
     */
    enum Type { Nothing, URI, Literal, Blank };

    /**
     * Construct a node with no node type (used for example as an
     * undefined node when pattern matching a triple).
     */
    Node() : type(Nothing), value() { }

    /**
     * Construct a node with a URI node type and the given URI.
     *
     * Note that URIs using namespace prefixes will need to be
     * expanded before they can safely be represented in a Uri.  Call
     * Store::expand() to achieve this.  In general you should ensure
     * that URIs are expanded when placed in a Node object rather than
     * being stored in prefixed form.
     *
     * (One basic principle of this RDF library is that we use QString
     * to represent URIs that may be local or namespace prefixed, and
     * Uri to represent expanded or canonical URIs.)
     */
    Node(Uri u) : type(URI), value(u.toString()) { }

    /**
     * Construct a node with the given node type and value, and with
     * no defined data type URI.
     */
    Node(Type t, QString v) : type(t), value(v) { }

    /**
     * Construct a node with the given node type, value, and data type
     * URI.
     */
    Node(Type t, QString v, QString dt) : type(t), value(v), datatype(dt) { }

    Node(const Node &n) :
        type(n.type), value(n.value), datatype(n.datatype) {
    }

    Node &operator=(const Node &n) {
        type = n.type; value = n.value; datatype = n.datatype;
        return *this;
    }

    ~Node() { }

    /**
     * Convert a QVariant to a Node.
     *
     * Simple QVariant types (integer, etc) are converted to literal
     * Nodes whose values are encoded as XSD datatypes, with the
     * node's value storing the XSD representation and the node's
     * datatype storing the XSD datatype URI.
     *
     * QVariants containing Uris are converted to URI nodes.  Note
     * that URIs using namespace prefixes will need to be expanded
     * before they can safely be represented in a Uri or URL variant.
     * Call Store::expand() to achieve this.  In general you should
     * ensure that URIs are expanded when placed in a Node object
     * rather than being stored in prefixed form.
     *
     * Other QVariants including complex structures are converted into
     * literals containing an encoded representation which may be
     * converted back again using toVariant but cannot be directly
     * read from or interchanged using the node's value.  These types
     * are given a specific fixed datatype URI.
     */
    static Node fromVariant(QVariant v);

    /**
     * Convert a Node to a QVariant.
     *
     * See fromVariant for details of the conversion.
     *
     * Note that URI nodes are returned as variants with user type
     * corresponding to Uri, not as QString variants.  This may result
     * in invalid Uris if the URIs were not properly expanded on
     * construction (see the notes about fromVariant).
     */
    QVariant toVariant() const;

    bool operator<(const Node &n) const {
        if (type != n.type) return type < n.type;
        if (value != n.value) return value < n.value;
        if (datatype != n.datatype) return datatype < n.datatype;
        return false;
    }
    
    Type type;
    QString value;
    QString datatype;
};

/**
 * A list of node types.
 */
typedef QList<Node> Nodes;

bool operator==(const Node &a, const Node &b);
bool operator!=(const Node &a, const Node &b);
    
QDataStream &operator<<(QDataStream &out, const Node &);
QDataStream &operator>>(QDataStream &in, Node &);

std::ostream &operator<<(std::ostream &out, const Node &);
QTextStream &operator<<(QTextStream &out, const Node &);

}

#endif

