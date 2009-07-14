/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*- vi:set ts=8 sts=4 sw=4: */

#ifndef _DATAQUAY_INTERNAL_DEBUG_H_
#define _DATAQUAY_INTERNAL_DEBUG_H_

#include <QDebug>
#include <QTextStream>

#ifndef NDEBUG

#define DEBUG QDebug(QtDebugMsg) << "[dataquay] "

namespace Dataquay {

template <typename T>
inline QDebug &operator<<(QDebug &d, const T &t) {
    QString s;
    QTextStream ts(&s);
    ts << t;
    d << s;
    return d;
}

}

#else

namespace Dataquay {

class NoDebug
{
public:
    inline NoDebug() {}
    inline ~NoDebug(){}

    template <typename T>
    inline NoDebug &operator<<(const T &) { return *this; }
};

}

// For use only within Dataquay namespace
#define DEBUG NoDebug()

#endif /* !NDEBUG */

#endif /* !_DEBUG_H_ */

