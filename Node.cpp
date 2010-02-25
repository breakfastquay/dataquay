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
// xsd:decimal, but always convert xsd:decimal to double.  However, we
// do currently impose symmetry for user-provided types (that is, our
// registerDatatype method adds the datatype to both maps).

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
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<bool>((s == "true") ||
                                         (s == "1"));
    }
    QString fromVariant(const QVariant &v) {
        return (v.toBool() ? "true" : "false");
    }
};

struct UriVariantEncoder : public Node::VariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<Uri>(Uri(s));
    }
    QString fromVariant(const QVariant &v) {
        return v.value<Uri>().toString();
    }
};

struct QUrlVariantEncoder : public Node::VariantEncoder {
    QVariant toVariant(const QString &s) {
        return QVariant::fromValue<QUrl>(QUrl(s));
    }
    QString fromVariant(const QVariant &v) {
        return v.value<QUrl>().toString();
    }
};

void
Node::registerDatatype(Uri datatype, QString typeName, VariantEncoder *enc) {
    QByteArray ba = typeName.toLocal8Bit();
    int id = QMetaType::type(ba.data());
    if (id <= 0) {
        std::cerr << "WARNING: Node::registerDatatype: Type name \""
                  << typeName.toStdString() << "\" is unknown to QMetaType, "
                  << "cannot register it here" << std::endl;
        return;
    }
    datatypeMetatypeMap[datatype] = QPair<int, Node::VariantEncoder *>(id, enc);
    metatypeDatatypeMap[id] = QPair<Uri, Node::VariantEncoder *>(datatype, enc);
}
    
Uri
Node::getDatatype(QString typeName)
{
    QByteArray ba = typeName.toLocal8Bit();
    int id = QMetaType::type(ba.data());
    if (id <= 0) return Uri();
    if (!metatypeDatatypeMap.contains(id)) return Uri();
    return metatypeDatatypeMap[id].first;
}

QString
Node::getVariantTypeName(Uri datatype)
{
    if (!datatypeMetatypeMap.contains(datatype)) return "";
    return QMetaType::typeName(datatypeMetatypeMap[datatype].first);
}

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

        registerXsd(QMetaType::Bool, "boolean", new BoolVariantEncoder());
        registerXsd(QMetaType::Int, "integer", new LongVariantEncoder());
        registerXsd(QMetaType::Long, "integer", new LongVariantEncoder());
        registerXsd(QMetaType::UInt, "integer", new ULongVariantEncoder());
        registerXsd(QMetaType::ULong, "integer", new ULongVariantEncoder());
        registerXsd(QMetaType::Float, "decimal", new DoubleVariantEncoder());
        registerXsd(QMetaType::Double, "decimal", new DoubleVariantEncoder());

        // Not necessary in normal use, because URIs are stored in URI
        // nodes and handled separately rather than being stored in
        // literal nodes... but necessary if an untyped literal is
        // presented for conversion via toVariant(Uri::metaTypeId())
        metatypeDatatypeMap[Uri::metaTypeId()] =
            QPair<Uri, Node::VariantEncoder *>(Uri(), new UriVariantEncoder());

        // Similarly, although no datatype is associated with QUrl, it
        // could be presented when trying to convert a URI Node using
        // an explicit variant type target (e.g. to assign RDF URIs to
        // QUrl properties rather than Uri ones)
        metatypeDatatypeMap[QMetaType::QUrl] =
            QPair<Uri, Node::VariantEncoder *>(Uri(), new QUrlVariantEncoder());

        // QString is a known variant type, but has no datatype when
        // writing (we write strings as untyped literals because
        // that's what seems most useful).  We already registered it
        // with xsd:string for reading.
        metatypeDatatypeMap[QMetaType::QString] =
            QPair<Uri, Node::VariantEncoder *>(Uri(), new StringVariantEncoder());

//        registerXsd(QMetaType::Time, "duration");
//        registerXsd(QMetaType::Date, "date");
    }
};

static NodeMetatypeMapRegistrar registrar;
/*!!! move this (and converse) to examples/ as example of encoder registration
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
    if (v.type() == QVariant::Url) {
        return Node(Node::URI, v.toUrl().toString());
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

        // no datatype defined: use opaque encoding for "unknown" type
        QByteArray b;
        QDataStream ds(&b, QIODevice::WriteOnly);
        ds << v;
        
        Node n;
        n.type = Literal;
        n.datatype = encodedVariantTypeURI;
        n.value = QString::fromAscii(b.toPercentEncoding());        
        return n;
    }
}

QVariant
Node::toVariant() const
{
    if (type == URI) {
        return QVariant::fromValue(Uri(value));
    }

    if (type == Nothing || type == Blank) {
        return QVariant();
    }

    if (datatype == Uri()) {
        return QVariant::fromValue<QString>(value);
    }
    
    if (datatype == encodedVariantTypeURI) {
        // Opaque encoding used for "unknown" types.  If this is
        // encoding is in use, we must decode from it even if the type
        // is actually known
        QByteArray benc = value.toAscii();
        QByteArray b = QByteArray::fromPercentEncoding(benc);
        QDataStream ds(&b, QIODevice::ReadOnly);
        QVariant v;
        ds >> v;
        return v;
    }
        
    DatatypeMetatypeMap::const_iterator i = datatypeMetatypeMap.find(datatype);

    if (i != datatypeMetatypeMap.end()) {
        
        // known datatype

        if (i.value().second) {
            
            // encoder present
            return i.value().second->toVariant(value);
            
        } else {
            
            // datatype present, but no encoder: can do nothing
            // interesting with this, convert as string
            return QVariant::fromValue<QString>(value);
        }
        
    } else {
        
        // unknown datatype, but not "unknown type" encoding;
        // convert as a string
        return QVariant::fromValue<QString>(value);
    }
}

QVariant
Node::toVariant(int metatype) const
{
    MetatypeDatatypeMap::const_iterator i = metatypeDatatypeMap.find(metatype);

    if (i == metatypeDatatypeMap.end()) {
        std::cerr << "WARNING: Node::toVariant: Unsupported metatype id "
                  << metatype << " (\"" << QMetaType::typeName(metatype)
                  << "\"), register it with registerDatatype please"
                  << std::endl;
        return QVariant();
    }

    if (i.value().second) {

        // encoder present
        return i.value().second->toVariant(value);

    } else {

        // datatype present, but no encoder: can do nothing
        // interesting with this, convert as string
        return QVariant::fromValue<QString>(value);
    }
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
        if (n.datatype != Uri()) out << "^^" << n.datatype;
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
        if (n.datatype != Uri()) out << "^^" << n.datatype;
        break;
    case Node::Blank:
        out << "[blank " << n.value << "]";
        break;
    }
    return out;
}

}



