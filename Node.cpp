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

#include "Node.h"

#include "Debug.h"

#include <QDataStream>
#include <QTextStream>
#include <QTime>
#include <QByteArray>
#include <QMetaType>

namespace Dataquay
{

static const Uri encodedVariantTypeURI
("http://breakfastquay.com/dataquay/datatype/encodedvariant");

static const Uri xsdPrefix
("http://www.w3.org/2001/XMLSchema#");

// Type maps to be used when converting from Node to Variant and
// Variant to Node, respectively.  These are not symmetrical -- for
// example, we convert xsd:string to QString, but convert QString to
// an untyped literal ("literal" and "literal"^^xsd:string compare
// differently and most people just use "literal", so we're more
// likely to achieve interoperable results if we don't type plain
// strings).  Similarly we convert both double and float to
// xsd:decimal, but always convert xsd:decimal to double.

typedef QHash<Uri, QPair<int, Node::VariantEncoder *> > DatatypeMetatypeMap;
static DatatypeMetatypeMap datatypeMetatypeMap;

typedef QHash<int, QPair<Uri, Node::VariantEncoder *> > MetatypeDatatypeMap;
static MetatypeDatatypeMap metatypeDatatypeMap;

struct StandardVariantEncoder : public Node::VariantEncoder {
    QString fromVariant(const QVariant &v) {
        return v.toString();
    }
};

struct LongVariantEncoder : public StandardVariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<long>(s.toLong());
    }
};

struct ULongVariantEncoder : public StandardVariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<unsigned long>(s.toULong());
    }
};

struct DoubleVariantEncoder : public StandardVariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<double>(s.toDouble());
    }
};

struct StringVariantEncoder : public StandardVariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<QString>(s);
    }
};

struct BoolVariantEncoder : public Node::VariantEncoder {
    QString fromVariant(const QVariant &v) {
        return (v.toBool() ? "true" : "false");
    }
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<bool>((s == "true") ||
                                         (s == "1"));
    }
};

struct NodeMetatypeMapRegistrar {

    void registerXsd(QString name, int id, Node::VariantEncoder *enc) {
        datatypeMetatypeMap[Uri(xsdPrefix.toString() + name)] =
            QPair<int, Node::VariantEncoder *>(id, enc);
    }

    void registerXsd(int id, QString name, Node::VariantEncoder *enc) {
        metatypeDatatypeMap[id] = QPair<Uri, Node::VariantEncoder *>
            (Uri(xsdPrefix.toString() + name), enc);
    }

    NodeMetatypeMapRegistrar() {
        registerXsd("string", QMetaType::QString, new StringVariantEncoder());
        registerXsd("boolean", QMetaType::Bool, new BoolVariantEncoder());
        registerXsd("int", QMetaType::Int, new LongVariantEncoder());
        registerXsd("long", QMetaType::Long, new LongVariantEncoder());
        registerXsd("integer", QMetaType::Long, new LongVariantEncoder());
        registerXsd("unsignedInt", QMetaType::UInt, new ULongVariantEncoder());
        registerXsd("nonNegativeInteger", QMetaType::ULong, new ULongVariantEncoder());
        registerXsd("float", QMetaType::Float, new DoubleVariantEncoder());
        registerXsd("double", QMetaType::Double, new DoubleVariantEncoder());
        registerXsd("decimal", QMetaType::Double, new DoubleVariantEncoder());

//        registerXsd(QMetaType::QString, "", new StringVariantEncoder());
        registerXsd(QMetaType::Bool, "boolean", new BoolVariantEncoder());
        registerXsd(QMetaType::Int, "integer", new LongVariantEncoder());
        registerXsd(QMetaType::Long, "integer", new LongVariantEncoder());
        registerXsd(QMetaType::UInt, "integer", new ULongVariantEncoder());
        registerXsd(QMetaType::ULong, "integer", new ULongVariantEncoder());
        registerXsd(QMetaType::Float, "decimal", new DoubleVariantEncoder());
        registerXsd(QMetaType::Double, "decimal", new DoubleVariantEncoder());

//        registerXsd(QMetaType::Time, "duration");
//        registerXsd(QMetaType::Date, "date");
    }
};

static NodeMetatypeMapRegistrar registrar;
/*
static
QString
qTimeToXsdDuration(QTime t)
{
    if (t.hour() != 0) {
        return
            QString("PT%1H%2M%3S")
            .arg(t.hour())
            .arg(t.minute())
            .arg(t.second() + double(t.msec())/1000.0);
    } else if (t.minute() != 0) {
        return
            QString("PT%1M%2S")
            .arg(t.minute())
            .arg(t.second() + double(t.msec())/1000.0);
    } else {
        return
            QString("PT%1S")
            .arg(t.second() + double(t.msec())/1000.0);
    }
}
*/
Node
Node::fromVariant(const QVariant &v)
{
    DEBUG << "Node::fromVariant: QVariant type is " << v.userType()
          << " (" << int(v.userType()) << "), variant is " << v << endl;

    if (Uri::isUri(v)) {
        return Node(v.value<Uri>());
    }

    int id = v.userType();
    
    MetatypeDatatypeMap::const_iterator i = metatypeDatatypeMap.find(id);

    if (i != metatypeDatatypeMap.end()) {
        
        Node n;
        n.type = Literal;
        n.datatype = i.value().first;

        if (i.value().second) {

            // encoder present
            n.value = i.value().second->fromVariant(v);

        } else {

            // datatype present, but no encoder: extract as string
            n.value = v.toString();
        }

        return n;

    } else {

        // no datatype defined
        QByteArray b;
        QDataStream ds(&b, QIODevice::WriteOnly);
        ds << v;
        
        Node n;
        n.type = Literal;
        n.datatype = encodedVariantTypeURI;
        n.value = QString::fromAscii(b.toPercentEncoding());        
        return n;
    }

    /*
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
        n.datatype = pfx + "integer";
        n.value = v.toString();
        break;

    case QMetaType::UInt:
        n.datatype = pfx + "integer";
        n.value = v.toString();
        break;

    case QVariant::String:
        // It seems to be inadvisable to encode strings as
        // ^^xsd:string, because "literal" and "literal"^^xsd:string
        // compare differently and most people just use "literal".
        // (Similarly we write integer and decimal instead of the
        // "machine" types int, double etc, because they seem more
        // useful from a practical interoperability perspective -- the
        // fact that we can't actually handle arbitrary numerical
        // lengths we'll have to treat as an "implementation issue")
        n.datatype = "";
        n.value = v.toString();
        break;

    case QVariant::Double:
        n.datatype = pfx + "decimal";
        n.value = v.toString();
        break;

    case QMetaType::Float:
        n.datatype = pfx + "decimal";
        n.value = v.toString();
        break;

    case QVariant::Time:
        n.datatype = pfx + "duration";
        n.value = qTimeToXsdDuration(v.toTime());
        break;
        
    default:
        } else {
        }
    }
    */
}

QVariant
Node::toVariant() const
{
    if (type == URI) {
        return QVariant::fromValue(Uri(value));
    } else if (type == Nothing || type == Blank) {
        return QVariant();
    }

    if (datatype == Uri()) {
        return QVariant::fromValue<QString>(value);
    }

    DatatypeMetatypeMap::const_iterator i = datatypeMetatypeMap.find(datatype);

    if (i != datatypeMetatypeMap.end()) {
        
        if (i.value().second) {

            // encoder present
            return i.value().second->toVariant(value);

        } else {

            // datatype present, but no encoder: can do nothing
            // interesting with this, encode as string
            return QVariant::fromValue<QString>(value);
        }

    } else {

        // no datatype defined
        if (datatype == encodedVariantTypeURI) {
            QByteArray benc = value.toAscii();
            QByteArray b = QByteArray::fromPercentEncoding(benc);
            QDataStream ds(&b, QIODevice::ReadOnly);
            QVariant v;
            ds >> v;
            return v;
        } else {
            return QVariant::fromValue<QString>(value);
        }
    }

        

/*

    Uri dtUri(datatype);

    static const QString pfx = "http://www.w3.org/2001/XMLSchema#";

#define DATATYPE(x) \
    static const Uri x ## Uri(pfx + #x)

    DATATYPE(string);
    DATATYPE(boolean);
    DATATYPE(int);
    DATATYPE(long);
    DATATYPE(integer);
    DATATYPE(double);
    DATATYPE(decimal);
    DATATYPE(float);
    DATATYPE(unsignedInt);
    DATATYPE(nonNegativeInteger);

#undef DATATYPE

    DEBUG << "Node::toVariant: datatype = " << datatype << endl;

    if (dtUri == stringUri) {
        return QVariant::fromValue<QString>(value);
    }
    if (dtUri == booleanUri) {
        return QVariant::fromValue<bool>((value == "true") ||
                                         (value == "1"));
    }
    if (dtUri == intUri) {
        return QVariant::fromValue<int>(value.toInt());
    }
    if (dtUri == longUri || dtUri == integerUri) {
        return QVariant::fromValue<long>(value.toLong());
    }
    if (dtUri == doubleUri || dtUri == decimalUri) {
        return QVariant::fromValue<double>(value.toDouble());
    }
    if (dtUri == floatUri) {
        return QVariant::fromValue<float>(value.toFloat());
    }
    if (dtUri == unsignedIntUri || dtUri == nonNegativeIntegerUri) {
        return QVariant::fromValue<unsigned>(value.toUInt());
    }
    if (dtUri == encodedVariantTypeURI) {
        QByteArray benc = value.toAscii();
        QByteArray b = QByteArray::fromPercentEncoding(benc);
        QDataStream ds(&b, QIODevice::ReadOnly);
        QVariant v;
        ds >> v;
        return v;
    }        
    return QVariant::fromValue<QString>(value);
*/
}

bool
operator==(const Node &a, const Node &b)
{
    if (a.type == Node::Nothing &&
        b.type == Node::Nothing) return true;
    if (a.type == b.type &&
        a.value == b.value &&
        a.datatype == b.datatype) return true;
    return false;
}

bool
operator!=(const Node &a, const Node &b)
{
    return !operator==(a, b);
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
        if (n.value.contains("://") || n.value.startsWith('#')) {
            out << "<" << n.value.toStdString() << ">";
        } else if (n.value == "") {
            out << "[empty-uri]";
        } else {
            out << n.value.toStdString();
        }
        break;
    case Node::Literal:
        out << "\"" << n.value.toStdString() << "\"";
        if (n.datatype != Uri()) out << "^^" << n.datatype.toString().toStdString();
        break;
    case Node::Blank:
        out << "[blank " << n.value.toStdString() << "]";
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
        if (n.value.contains("://") || n.value.startsWith('#')) {
            out << "<" << n.value << ">";
        } else if (n.value == "") {
            out << "[empty-uri]";
        } else {
            out << n.value;
        }
        break;
    case Node::Literal:
        out << "\"" << n.value << "\"";
        if (n.datatype != Uri()) out << "^^" << n.datatype.toString();
        break;
    case Node::Blank:
        out << "[blank " << n.value << "]";
        break;
    }
    return out;
}

}



