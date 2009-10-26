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

#ifndef _DATAQUAY_QOBJECT_MAPPER_H_
#define _DATAQUAY_QOBJECT_MAPPER_H_

#include <QUrl>

class QObject;

namespace Dataquay
{

class Store;

class QObjectMapper
{
public:
    /**
     * Create a QObjectMapper ready to load and store objects from and
     * to the given RDF store.
     */
    QObjectMapper(Store *s);

    ~QObjectMapper();

    /**
     * Load to the given object all QObject properties defined in this
     * object mapper's store for the given object URI.
     */
    void loadProperties(QObject *o, QUrl uri);

    //!!! how useful is this selection?
    enum PropertySelectionPolicy {
	PropertiesChangedFromDefault,
	PropertiesChangedFromUnparentedDefault, //!!!??? any use?
        AllStoredProperties,
	AllProperties
    };

    //!!! transaction/connection management?

    /**
     * Save to the object mapper's RDF store all properties for the
     * given object, as QObject property types associated with the
     * given object URI.
     */
    void storeProperties(QObject *o, QUrl uri, PropertySelectionPolicy);

    /**
     * Construct a QObject based on the properties of the given object
     * URI in the object mapper's store.  The type of class created
     * will be determined by the rdf:type for the URI.
     *!!!??? type prefix? how these map to class names?
     */
    QObject *loadObject(QUrl uri, QObject *parent);
    QObject *loadObjects(QUrl rootUri, QObject *parent);
    QObject *loadAllObjects(QObject *parent);

    enum URISourcePolicy {
	UseObjectProvidedURIs,
	CreateNewURIs
    };

    enum ObjectSelectionPolicy {
	ObjectsWithURIs,
	AllObjects
    };

    QUrl storeObject(QObject *o, URISourcePolicy, PropertySelectionPolicy);
    QUrl storeObjects(QObject *root, URISourcePolicy, ObjectSelectionPolicy,
                      PropertySelectionPolicy);

private:
    class D;
    D *m_d;
};

}

#endif
