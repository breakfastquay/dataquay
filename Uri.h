/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2010 Chris Cannam.
  
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

#ifndef _DATAQUAY_URI_H_
#define _DATAQUAY_URI_H_

namespace Dataquay {
class Uri;
}

// Declare this early, to avoid any problems with instantiation order
// arising from inclusion "races"
extern unsigned int qHash(const Dataquay::Uri &u);

#include <QString>
#include <QUrl>

#include <QMetaType>

#include <iostream>

class QDataStream;
class QTextStream;
class QVariant;

namespace Dataquay
{

/**
 * Uri represents a single URI.  It is a very thin immutable wrapper
 * around a string.  Its purpose is to allow us to distinguish between
 * abbreviated URIs (CURIEs) which may be subject to prefix expansion
 * (represented by strings) and full URIs (represented by Uri).
 *
 * In terms of the Turtle syntax, anything written within angle
 * brackets is a Uri, while a bare string in URI context is not: it
 * should be stored as a QString and converted to a Uri using
 * Store::expand().  For example, <http://xmlns.com/foaf/0.1/name> is
 * a Uri, while foaf:name is just a string.  Never store the latter
 * form in a Uri object without expanding it first.
 *
 * (Wherever a method in Dataquay accepts a URI as a QString type, it
 * is safe to assume that it will expand any prefixes used in the URI
 * before use.  Some methods may exist in alternative forms with Uri
 * or QString arguments; in this case the QString form does prefix
 * expansion, while the Uri form does not.)
 *
 * Dataquay uses Uri in preference to QUrl because the latter is
 * relatively slow to convert to and from string forms.  Uri imposes
 * no overhead over a string, it simply aids type safety.
 */
class Uri
{
public:
    /**
     * Construct an empty (invalid, null) URI.
     */
    Uri() {
    }

    /**
     * Construct a URI from the given string, which is expected to
     * contain the text of a complete well-formed URI.  To construct a
     * Uri from an abbreviated URI via prefix expansion, use
     * Store::expand() instead.  This constructor is intentionally
     * marked explicit; no silent conversion is available.
     */
    explicit Uri(const QString &s) : m_hash(0), m_uri(s) {
#ifndef NDEBUG
	checkComplete();
#endif
        makeHash();
    }

    /**
     * Construct a URI from the given QUrl, which is expected to
     * contain a complete well-formed URI.
     */
    explicit Uri(const QUrl &u) : m_hash(0), m_uri(u.toString()) {
#ifndef NDEBUG
	checkComplete();
#endif
        makeHash();
    }

    ~Uri() {
    }

    inline QString toString() const { return m_uri; }
    inline QUrl toUrl() const { return QUrl(m_uri); }
    inline int length() const { return m_uri.length(); }
    inline unsigned int hash() const { return m_hash; }

    QString scheme() const;

    inline bool operator==(const Uri &u) const {
        if (m_hash != u.m_hash) return false;
        else return urisEqual(u);
    }
    inline bool operator!=(const Uri &u) const { return !operator==(u); }
    inline bool operator<(const Uri &u) const { return m_uri < u.m_uri; }
    inline bool operator>(const Uri &u) const { return u < *this; }

    static QString metaTypeName();
    static int metaTypeId();
    static bool isUri(const QVariant &);
    
private:
    void checkComplete() const;
    bool urisEqual(const Uri &) const;
    void makeHash();
    unsigned int m_hash;
    QString m_uri;
};

typedef QList<Uri> UriList;

QDataStream &operator<<(QDataStream &out, const Uri &);
QDataStream &operator>>(QDataStream &in, Uri &);

std::ostream &operator<<(std::ostream &out, const Uri &);
QTextStream &operator<<(QTextStream &out, const Uri &);

}

Q_DECLARE_METATYPE(Dataquay::Uri)

#endif
