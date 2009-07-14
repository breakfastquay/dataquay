/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "Triple.h"

#include <QDataStream>
#include <QTextStream>

namespace Dataquay
{

bool
operator==(const Triple &a, const Triple &b)
{
    if (a.a == b.a &&
        a.b == b.b &&
        a.c == b.c) return true;
    return false;
}

QDataStream &
operator<<(QDataStream &out, const Triple &t)
{
    return out << t.a << t.b << t.c;
}

QDataStream &
operator>>(QDataStream &in, Triple &t)
{
    return in >> t.a >> t.b >> t.c;
}

std::ostream &
operator<<(std::ostream &out, const Triple &t)
{
    return out << "( " << t.a << " " << t.b << " " << t.c << " )";
}

QTextStream &
operator<<(QTextStream &out, const Triple &t)
{
    return out << "( " << t.a << " " << t.b << " " << t.c << " )";
}

}

