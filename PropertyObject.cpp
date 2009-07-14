/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#include "PropertyObject.h"

namespace Turbot
{

namespace RDF
{

const char *
PropertyNotFound::what() const throw()
{
    return QString
        ("Object property %1 (RDF property %2) not found for URI %3")
        .arg(m_prop).arg(m_propuri).arg(m_uri).toLocal8Bit().data();
}

}

}

