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

#include "Uri.h"

#include <QDataStream>
#include <QTextStream>
#include <QVariant>
#include <QMutex>

#include <iostream>

#ifndef NDEBUG
#include <QRegExp>
#endif

#include "Debug.h"

namespace Dataquay
{

struct UriRegistrar {
    static int type;
    static QMutex mutex;
    static void registerType() {
        mutex.lock();
        if (type == 0) {
            type = qRegisterMetaType<Uri>("Dataquay::Uri");
            qRegisterMetaTypeStreamOperators<Uri>("Dataquay::Uri");
        }
        mutex.unlock();
    }
    UriRegistrar() {
        registerType();
    }
};

int UriRegistrar::type = 0;
QMutex UriRegistrar::mutex;
static UriRegistrar uriRegistrar;

QString
Uri::metaTypeName()
{
    return "Dataquay::Uri";
}

int
Uri::metaTypeId()
{
    UriRegistrar::registerType();
    if (uriRegistrar.type <= 0) {
	DEBUG << "Uri::metaTypeId: No meta type available -- did static registration fail?" << endl;
	return 0;
    }
    return uriRegistrar.type;
}

void
Uri::checkComplete() const
{
#ifndef NDEBUG
    if (!m_uri.isEmpty() && m_uri[0] != '#' &&
	!m_uri.contains(QRegExp("^[a-zA-Z]+://")) &&
        !m_uri.startsWith("file:")) { // file uri may be relative: file:x.wav
	std::cerr << "WARNING: URI <" << m_uri.toStdString()
		  << "> is not complete; lacks scheme" << std::endl;
    }
#endif
}

QString
Uri::scheme() const
{
    int index = m_uri.indexOf(':');
    if (index < 0) return "";
    return m_uri.left(index);
}

bool
Uri::operator==(const Uri &u) const
{
    const QString &other = u.m_uri;
    int len = length();
    if (len != other.length()) return false;
    for (int i = len - 1; i >= 0; --i) {
	if (m_uri.at(i) != other.at(i)) return false;
    }
    return true;
}

bool
Uri::isUri(const QVariant &v)
{
    return (v.type() == QVariant::UserType && v.userType() == metaTypeId());
}

QDataStream &operator<<(QDataStream &out, const Uri &u) {
    return out << u.toString();
}

QDataStream &operator>>(QDataStream &in, Uri &u) {
    QString s;
    in >> s;
    u = Uri(s);
    return in;
}

std::ostream &operator<<(std::ostream &out, const Uri &u) {
    return out << u.toString().toStdString();
}

QTextStream &operator<<(QTextStream &out, const Uri &u) {
    return out << u.toString();
}

}

unsigned int qHash(const Dataquay::Uri &u)
{
    return qHash(u.toString());
}



