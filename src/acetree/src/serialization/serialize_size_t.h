#ifndef SERIALIZE_SIZE_T_H
#define SERIALIZE_SIZE_T_H

#include <QDataStream>
#include <QTextStream>

#if defined(__LP64__) || defined(_LP64)
#    if defined(Q_OS_LINUX) || defined(Q_OS_MAC)

inline QDataStream &operator>>(QDataStream &in, size_t &i) {
    return in >> reinterpret_cast<quint64 &>(i);
}

inline QDataStream &operator<<(QDataStream &out, size_t i) {
    return out << quint64(i);
}

inline QTextStream &operator>>(QTextStream &in, size_t &i) {
    return in >> reinterpret_cast<quint64 &>(i);
}

inline QTextStream &operator<<(QTextStream &out, size_t i) {
    return out << quint64(i);
}

#    endif
#endif


#endif // SERIALIZE_SIZE_T_H
