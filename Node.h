/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _DATAQUAY_NODE_H_
#define _DATAQUAY_NODE_H_

#include <QString>
#include <QUrl>
#include <QVariant>

class QDataStream;
class QTextStream;

namespace Dataquay
{

/**
 * \class Node Node.h <dataquay/Node.h>
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
     * expanded before they can safely be represented in a QUrl.  Call
     * Store::expand() to achieve this.  In general you should ensure
     * that URIs are expanded when placed in a Node object rather than
     * being stored in prefixed form.
     *
     * (One basic principle of this RDF library is that we use QString
     * to represent URIs that may be local or namespace prefixed, and
     * QUrl to represent expanded or canonical URIs.)
     */
    Node(QUrl u) : type(URI), value(u.toString()) { }

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

    ~Node() { }

    //!!! probably nicer to have template to<typename> and
    //!!! from<typename>, no? at least in addition to these

    /**
     * Convert a QVariant to a Node.
     *
     * Simple QVariant types (integer, etc) are converted to literal
     * Nodes whose values are encoded as XSD datatypes, with the
     * node's value storing the XSD representation and the node's
     * datatype storing the XSD datatype URI.
     *
     * QVariants containing QUrls are converted to URI nodes.  Note
     * that URIs using namespace prefixes will need to be expanded
     * before they can safely be represented in a QUrl or URL variant.
     * Call Store::expand() to achieve this.  In general you should
     * ensure that URIs are expanded when placed in a Node object
     * rather than being stored in prefixed form.
     *
     * Other QVariants including complex structures are converted into
     * literals containing an encoded representation which may be
     * converted back again using toVariant but cannot be directly
     * read from the node's value.  These types are given a specific
     * fixed datatype URI.
     */
    static Node fromVariant(QVariant v);

    /**
     * Convert a Node to a QVariant.
     *
     * See fromVariant for details of the conversion.
     *
     * Note that URI nodes are returned as string variants, not QUrl
     * variants, because QUrl is not a suitable class for representing
     * URIs (as discussed in the fromVariant documentation).
     *
     **!!!!! This won't do -- not while fromVariant is silently
     *       converting all strings to literal nodes...
     */
    QVariant toVariant() const;
    
    Type type;
    QString value;
    QString datatype;
};

typedef QList<Node> Nodes;

bool operator==(const Node &a, const Node &b);
bool operator!=(const Node &a, const Node &b);
    
QDataStream &operator<<(QDataStream &out, const Node &);
QDataStream &operator>>(QDataStream &in, Node &);

std::ostream &operator<<(std::ostream &out, const Node &);
QTextStream &operator<<(QTextStream &out, const Node &);

}

#endif

