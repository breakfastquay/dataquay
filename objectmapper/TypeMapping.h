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

#ifndef _DATAQUAY_TYPE_MAPPING_H_
#define _DATAQUAY_TYPE_MAPPING_H_

#include "../Uri.h"

#include <QString>

namespace Dataquay
{

class TypeMapping //!!! not a good name
{
public:
    TypeMapping();
    TypeMapping(const TypeMapping &);
    TypeMapping &operator=(const TypeMapping &);
    ~TypeMapping();

    //!!! review these function names

    //!!!... document
    void setObjectTypePrefix(Uri prefix);
    Uri getObjectTypePrefix() const;

    void setPropertyPrefix(Uri prefix);
    Uri getPropertyPrefix() const;

    void setRelationshipPrefix(Uri prefix);
    Uri getRelationshipPrefix() const;

    void addTypeMapping(QString className, Uri uri);
    bool getTypeUriForClass(QString className, Uri &uri);
    bool getClassForTypeUri(Uri uri, QString &className);


    /**
     * Add a mapping between class name and the common parts of any
     * URIs that are automatically generated when storing instances of
     * that class that have no URI property defined.
     * 
     * For example, a mapping from "MyNamespace::MyClass" to
     * "http://mydomain.com/resource/" would ensure that automatically
     * generated "unique" URIs for instances that class all started
     * with that URI prefix.  Note that the prefix itself is also
     * subject to namespace prefix expansion when stored.
     *
     * (If no prefix mapping was given for this example, its generated
     * URIs would start with ":mynamespace_myclass_".)
     *
     * Generated URIs are only checked for uniqueness within the store
     * being exported to and cannot be guaranteed to be globally
     * unique.  For this reason, caution should be exercised in the
     * use of this function.
     *
     * Note that in principle the object mapper could use this when
     * loading, to infer class types for URIs in the store that have
     * no rdf:type.  In practice that does not happen -- the object
     * mapper will not generate a class for URIs without rdf:type.
     */
    void addTypeUriPrefixMapping(QString className, Uri prefix);

    bool getUriPrefixForClass(QString className, Uri &prefix);

    //!!! n.b. document that uris must be distinct (can't map to properties to same RDF predicate as we'd be unable to distinguish between them on reload -- we don't use the object type to distinguish which predicate is which)
    void addPropertyMapping(QString className, QString propertyName, Uri uri);

    bool getPropertyUri(QString className, QString propertyName, Uri &uri);
    bool getPropertyName(QString className, Uri propertyUri, QString &propertyName);

private:
    class D;
    D *m_d;
};

}

#endif
