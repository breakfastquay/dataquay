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

#ifndef _DATAQUAY_URI_H_
#define _DATAQUAY_URI_H_

#include <QString>
#include <QUrl>

#include <QMetaType>

#include <iostream>

class QDataStream;
class QTextStream;
class QVariant;

namespace Dataquay
{

class Uri
{
public:
    Uri() {
    }
    explicit Uri(const QString &s) : m_uri(s) {
#ifndef NDEBUG
	checkComplete();
#endif
    }
    explicit Uri(const QUrl &u) : m_uri(u.toString()) {
#ifndef NDEBUG
	checkComplete();
#endif
    }
    ~Uri() {
    }

    QString toString() const { return m_uri; }
    QUrl toUrl() const { return QUrl(m_uri); }
    
    int length() const { return m_uri.length(); }
    QString scheme() const;

    bool operator==(const Uri &u) const;
    bool operator!=(const Uri &u) const { return !operator==(u); }
    bool operator<(const Uri &u) const { return m_uri < u.m_uri; }
    bool operator>(const Uri &u) const { return u < *this; }

    //!!! is there a nicer way to do this?
    static int metaTypeId();
    static bool isUri(const QVariant &);
    
private:
    void checkComplete() const;
    QString m_uri;
};

typedef QList<Uri> UriList;

QDataStream &operator<<(QDataStream &out, const Uri &);
QDataStream &operator>>(QDataStream &in, Uri &);

std::ostream &operator<<(std::ostream &out, const Uri &);
QTextStream &operator<<(QTextStream &out, const Uri &);

}

extern unsigned int qHash(const Dataquay::Uri &u);
Q_DECLARE_METATYPE(Dataquay::Uri)

#endif
