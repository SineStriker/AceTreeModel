#ifndef ACETREENUMBERSENTITY_H
#define ACETREENUMBERSENTITY_H

#include <QJsonArray>

#include "AceTreeEntity.h"

class AceTreeNumbersEntityPrivate;

class ACETREE_EXPORT AceTreeNumbersEntity : public AceTreeEntity {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeNumbersEntity)
public:
    explicit AceTreeNumbersEntity(QObject *parent = nullptr);
    ~AceTreeNumbersEntity();

public:
    bool read(const QJsonValue &value) override;
    QJsonValue write() const override;

protected:
    int sizeImpl() const;
    const char *valuesImpl() const;
    void replaceImpl(int index, const char *data, int size);
    void insertImpl(int index, const char *data, int size);
    void removeImpl(int index, int size);

    void initType(int type_size, QByteArray (*arr_to_bytes)(const QJsonArray &),
                  QJsonArray (*bytes_to_arr)(const QByteArray &));

private:
    static AceTreeNumbersEntity *get_entity_helper(const void *ptr);

    template <class T>
    friend class AceTreeEntityNumbersHelper;
};

template <class T>
class AceTreeEntityNumbersHelper {
    static_assert(sizeof(T) >= 1, "T should be a complete type");

public:
    AceTreeEntityNumbersHelper();

    int size() const;
    QVector<T> mid(int index, int size) const;
    QVector<T> values() const;
    void prepend(const QVector<T> &values);
    void append(const QVector<T> &values);
    void replace(int index, const QVector<T> &values);
    void insert(int index, const QVector<T> &values);
    void remove(int index, int count);

private:
    const AceTreeNumbersEntity *to_entity() const;
    AceTreeNumbersEntity *to_entity();
};

#define ACE_TREE_DECLARE_NUMBERS_SIGNALS(T)                                                        \
    void replaced(int index, const QVector<T> &values);                                            \
    void inserted(int index, const QVector<T> &values);                                            \
    void removed(int index, int count);

template <class T>
AceTreeEntityNumbersHelper<T>::AceTreeEntityNumbersHelper() {
    AceTreeNumbersEntity *e = to_entity();
    e->initType(
        sizeof(T),
        [](const QJsonArray &arr) -> QByteArray {
            QByteArray bytes;
            bytes.reserve(arr.size() * int(sizeof(T)));
            for (const auto &item : qAsConst(arr)) {
                auto num = static_cast<T>(item.toDouble());
                bytes.append(reinterpret_cast<const char *>(&num), int(sizeof(num)));
            }
            return bytes;
        },
        [](const QByteArray &bytes) -> QJsonArray {
            QJsonArray arr;
            auto begin = reinterpret_cast<const T *>(bytes);
            auto end = begin + (bytes.size() / int(sizeof(T)));
            for (auto it = begin; it != end; ++it) {
                arr.append(static_cast<double>(*it));
            }
            return arr;
        });
}

template <class T>
int AceTreeEntityNumbersHelper<T>::size() const {
    return to_entity()->sizeImpl();
}

template <class T>
QVector<T> AceTreeEntityNumbersHelper<T>::mid(int index, int size) const {
    if (index < 0 || index >= this->size()) {
        return {};
    }
    size = qMin(this->size() - index, size);
    return {to_entity()->valuesImpl() + index * int(sizeof(T)), size * int(sizeof(T))};
}

template <class T>
QVector<T> AceTreeEntityNumbersHelper<T>::values() const {
    return {to_entity()->valuesImpl(), size()};
}

template <class T>
void AceTreeEntityNumbersHelper<T>::prepend(const QVector<T> &values) {
    insert(0, values);
}

template <class T>
void AceTreeEntityNumbersHelper<T>::append(const QVector<T> &values) {
    insert(size(), values);
}

template <class T>
void AceTreeEntityNumbersHelper<T>::replace(int index, const QVector<T> &values) {
    to_entity()->replaceImpl(index, reinterpret_cast<const char *>(values.constData()),
                             values.size());
}

template <class T>
void AceTreeEntityNumbersHelper<T>::insert(int index, const QVector<T> &values) {
    to_entity()->insertImpl(index, reinterpret_cast<const char *>(values.constData()),
                            values.size());
}

template <class T>
void AceTreeEntityNumbersHelper<T>::remove(int index, int count) {
    to_entity()->removeImpl(index, count);
}

template <class T>
const AceTreeNumbersEntity *AceTreeEntityNumbersHelper<T>::to_entity() const {
    return AceTreeNumbersEntity::get_entity_helper(this);
}

template <class T>
AceTreeNumbersEntity *AceTreeEntityNumbersHelper<T>::to_entity() {
    return AceTreeNumbersEntity::get_entity_helper(this);
}

#endif // ACETREENUMBERSENTITY_H
