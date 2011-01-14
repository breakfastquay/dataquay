
Dataquay
========

Dataquay is a free open source library that provides a friendly C++
API for the popular Redland RDF data store using Qt4 classes and
containers.

Dataquay is intended to be simple to use and easy to integrate. It is
principally intended for use in Qt-based applications that would like
to use an RDF datastore as backing for in-memory project data, to
avoid having to provide application data-specific file formats and to
make it easy to augment the data with descriptive metadata pulled in
from external sources. Dataquay is also intended to be useful for
applications whose primary purpose is not related to RDF but that have
ad-hoc RDF needs for metadata management.

Dataquay provides these features:

 * Conversion between arbitrary data types and RDF nodes using the
   Node class and QVariant types. Data are converted to XSD datatypes
   where possible, using an easily extended mechanism.

 * Simple and flexible storage, query, and file I/O (natively using
   the Turtle format) functions provided by the Store and BasicStore
   classes.

 * Straightforward transactional interface via TransactionalStore and
   Transaction. Transactions are atomic at the library level, and are
   isolated from any non-transactional queries occurring at the same
   time. The transactional implementation is designed to be simple
   rather than scalable, intended for use in encapsulating single-user
   editing operations: it is probably not wise to use Dataquay as a
   store for server applications, although it might work.

 * Optional ODBC-style transactional Connection interface.

 * ObjectMapper, a facility which can take care of a complete object
   hierarchy, map it to the RDF store, and synchronise in both
   directions any changes to the hierarchy or the store. Arbitrary
   mappings between C++ class and property names and RDF URIs can be
   specified.  The mapping is flexible enough to allow you to load
   many publicly generated Linked Data sources directly into object
   class hierarchies, if you wish (although this is not usually an
   ideal way to handle unpredictable data sources).

 * API documentation, thread safety, documentation of threading
   requirements, readable code, and basic unit tests included.

 * BSD licensing. You can use Dataquay cost-free in commercial or open
   source applications and modify it as you like with no particular
   requirements except acknowledgement in your copyright notes. We do
   ask that you let us know of any bugs, fixes and enhancements you
   might find -- particularly for serious bugs -- but you have no
   obligation to do so.

Dataquay is a small library: most of the serious work is done by Dave
Beckett's excellent Redland C libraries with David Robillard's Trees
store and Turtle I/O implementations. You will need to have the
Raptor, Rasqal and Redland libraries installed in order to build and
use Dataquay.


Compiling Dataquay
------------------

Run qmake then make.  There is no install step yet.

Dataquay requires Qt 4.6 or newer.

The default debug build will print a lot of information to standard
error during run time; this can be very helpful for debugging your
application, but it can quickly become excessive.  Define NDEBUG in
the build to eliminate this output.

A test program is included in the tests/ directory -- this runs a
series of unit tests and will bail out if any fails.  To use it, run
qmake and make in the tests/ directory and then run ./test-dataquay .

Note that this is a pre-1.0 release and the API is still subject to
change.


Chris Cannam
chris.cannam@breakfastquay.com
