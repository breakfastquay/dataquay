/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "Node.h"

#include "system/Debug.h"

#include <QDataStream>
#include <QTextStream>
#include <QUrl>
#include <QByteArray>

namespace Turbot
{

namespace RDF
{

static const QString encodedVariantTypeURI = "http://breakfastquay.com/rdf/datatype/encodedvariant";

Node
Node::fromVariant(QVariant v)
{
    static const QString pfx = "http://www.w3.org/2001/XMLSchema#";
    Node n;
    n.type = Literal;

    DEBUG << "Node::fromVariant: QVariant type is " << v.type() << " (" << int(v.type()) << "), variant is " << v << endl;
    
    switch (v.type()) {

    case QVariant::Url:
        n.type = URI;
        n.value = v.toUrl().toString();
        break;

    case QVariant::Bool:
        n.datatype = pfx + "boolean";
        n.value = (v.toBool() ? "true" : "false");
        break;

    case QVariant::Int:
        n.datatype = pfx + "int";
        n.value = v.toString();
        break;

    case QVariant::String:
        n.datatype = pfx + "string";
        n.value = v.toString();
        break;

    case QVariant::Double:
        n.datatype = pfx + "double";
        n.value = v.toString();
        break;
        
    default:
    {
        QByteArray b;
        QDataStream ds(&b, QIODevice::WriteOnly);
        ds << v;
        n.datatype = encodedVariantTypeURI;
        n.value = QString::fromAscii(b.toPercentEncoding());
    }
    }

    return n;
}

QVariant
Node::toVariant() const
{
    if (type == URI) {
        return QVariant::fromValue(QUrl(value));
    } else if (type == Nothing || type == Blank) {
        return QVariant();
    }

    static const QString pfx = "http://www.w3.org/2001/XMLSchema#";
    DEBUG << "Node::toVariant: datatype = " << datatype << endl;
    if (datatype.startsWith(pfx)) {
        if (datatype == pfx + "boolean") {
            return QVariant::fromValue<bool>((value == "true") ||
                                             (value == "1"));
        } else if (datatype == pfx + "int") {
            return QVariant::fromValue<int>(value.toInt());
        } else if (datatype == pfx + "string") {
            return QVariant::fromValue<QString>(value);
        } else if (datatype == pfx + "double") {
            return QVariant::fromValue<double>(value.toDouble());
        }
    }
    if (datatype == encodedVariantTypeURI) {
        QByteArray benc = value.toAscii();
        QByteArray b = QByteArray::fromPercentEncoding(benc);
        QDataStream ds(&b, QIODevice::ReadOnly);
        QVariant v;
        ds >> v;
        return v;
    }        
    return QVariant::fromValue<QString>(value);
}

bool
operator==(const Node &a, const Node &b)
{
    if (a.type == b.type &&
        a.value == b.value &&
        a.datatype == b.datatype) return true;
    return false;
}

QDataStream &
operator<<(QDataStream &out, const Node &n)
{
    return out << (int)n.type << n.value << n.datatype;
}

QDataStream &
operator>>(QDataStream &in, Node &n)
{
    int t;
    in >> t >> n.value >> n.datatype;
    n.type = (Node::Type)t;
    return in;
}

std::ostream &
operator<<(std::ostream &out, const Node &n)
{
    switch (n.type) {
    case Node::Nothing:
        out << "[]";
        break;
    case Node::URI:
        //!!! is there any way to distinguish between prefixed and expanded
        // URIs?  should we be passing around full URIs with <> already?
        if (n.value.startsWith("http:") || n.value.startsWith("file:")) {
            out << "<" << n.value.toStdString() << ">";
        } else if (n.value == "") {
            out << "[empty-uri]";
        } else {
            out << n.value.toStdString();
        }
        break;
    case Node::Literal:
        out << "\"" << n.value.toStdString() << "\"";
        if (n.datatype != "") out << "^^" << n.datatype.toStdString();
        break;
    case Node::Blank:
        out << "[blank]";
        break;
    }
    return out;
}

QTextStream &
operator<<(QTextStream &out, const Node &n)
{
    switch (n.type) {
    case Node::Nothing:
        out << "[]";
        break;
    case Node::URI:
        //!!! is there any way to distinguish between prefixed and expanded
        // URIs?  should we be passing around full URIs with <> already?
        if (n.value.startsWith("http:") || n.value.startsWith("file:")) {
            out << "<" << n.value << ">";
        } else if (n.value == "") {
            out << "[empty-uri]";
        } else {
            out << n.value;
        }
        break;
    case Node::Literal:
        out << "\"" << n.value << "\"";
        if (n.datatype != "") out << "^^" << n.datatype;
        break;
    case Node::Blank:
        out << "[blank]";
        break;
    }
    return out;
}

}

}

