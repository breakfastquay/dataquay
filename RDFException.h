/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

#ifndef _RDF_EXCEPTION_H_
#define _RDF_EXCEPTION_H_

#include <QString>
#include <exception>

namespace Turbot
{

namespace RDF
{

class RDFException : virtual public std::exception
{
public:
    RDFException(QString message, QString data = "") throw() :
        m_message(message), m_data(data) { }
    virtual ~RDFException() throw() { }
    virtual const char *what() const throw() {
        if (m_data == "") {
            return m_message.toLocal8Bit().data();
        } else {
            return QString("%1 [%2]").arg(m_message).arg(m_data).toLocal8Bit().data();
        }
    }
    
protected:
    QString m_message;
    QString m_data;
};

}

}

#endif

