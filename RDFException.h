/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Dataquay

    A C++/Qt library for simple RDF datastore management with Redland.
    Copyright 2009-2011 Chris Cannam.
  
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

#ifndef _DATAQUAY_EXCEPTION_H_
#define _DATAQUAY_EXCEPTION_H_

#include <QString>
#include <exception>

#include "Uri.h"

namespace Dataquay
{

/**
 * \class RDFException RDFException.h <dataquay/RDFException.h>
 *
 * RDFException is an exception that results from incorrect usage of
 * the RDF store interface or unsuitable data provided to a function.
 * For example, this exception would be thrown in response to trying
 * to add an incomplete triple to the store.
 */
class RDFException : virtual public std::exception
{
public:
    RDFException(QString message) throw() : m_message(message) { }
    RDFException(QString message, QString data) throw() {
        m_message = QString("%1 [with string \"%2\"]").arg(message).arg(data);
    }
    RDFException(QString message, Uri uri) throw() {
        m_message = QString("%1 [with URI <%2>]").arg(message).arg(uri.toString());
    }
    virtual ~RDFException() throw() { }
    virtual const char *what() const throw() {
        return m_message.toLocal8Bit().data();
    }
    
protected:
    QString m_message;
};

/**
 * \class RDFInternalError RDFException.h <dataquay/RDFException.h>
 *
 * RDFInternalError is an exception that results from an internal
 * error in the RDF store.
 */
class RDFInternalError : virtual public RDFException
{
public:
    RDFInternalError(QString message, QString data = "") throw() :
        RDFException(message, data) { }
    RDFInternalError(QString message, Uri data) throw() :
        RDFException(message, data) { }
};

/**
 * \class RDFTransactionError RDFException.h <dataquay/RDFException.h>
 *
 * RDFTransactionError is an exception that results from incorrect use
 * of a Transaction, for example using a Transaction object after it
 * has been committed.
 */
class RDFTransactionError : virtual public RDFException
{
public:
    RDFTransactionError(QString message, QString data = "") throw() :
        RDFException(message, data) { }
};

/**
 * \class RDFDuplicateImportException RDFException.h <dataquay/RDFException.h>
 *
 * RDFDuplicateImportException is an exception that results from an
 * import into a store from an RDF document in ImportFailOnDuplicates
 * mode, where the document contains a triple that already exists in
 * the store.
 */
class RDFDuplicateImportException : virtual public RDFException
{
public:
    RDFDuplicateImportException(QString message, QString data = "") throw() :
        RDFException(message, data) { }
};

}

#endif

