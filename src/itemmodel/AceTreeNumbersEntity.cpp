#include "AceTreeNumbersEntity.h"
#include "AceTreeEntity_p.h"

class AceTreeNumbersEntityPrivate : public AceTreeEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeNumbersEntity)
public:
    AceTreeNumbersEntityPrivate() : type_size(0), arr_to_bytes(nullptr), bytes_to_arr(nullptr) {
    }
    ~AceTreeNumbersEntityPrivate() = default;

    size_t type_size;
    QByteArray (*arr_to_bytes)(const QJsonArray &);
    QJsonArray (*bytes_to_arr)(const QByteArray &);
};

AceTreeNumbersEntity::AceTreeNumbersEntity(QObject *parent)
    : AceTreeEntity(*new AceTreeNumbersEntityPrivate(), parent) {
}

AceTreeNumbersEntity::~AceTreeNumbersEntity() {
}

bool AceTreeNumbersEntity::read(const QJsonValue &value) {
    Q_D(AceTreeNumbersEntity);
    if (!value.isArray())
        return false;
    d->m_treeItem->appendBytes(d->arr_to_bytes(value.toArray()));
    return true;
}

QJsonValue AceTreeNumbersEntity::write() const {
    Q_D(const AceTreeNumbersEntity);
    return d->bytes_to_arr(d->m_treeItem->bytes());
}

int AceTreeNumbersEntity::sizeImpl() const {
    Q_D(const AceTreeNumbersEntity);
    return d->m_treeItem->bytesSize() / int(d->type_size);
}

const char *AceTreeNumbersEntity::valuesImpl() const {
    Q_D(const AceTreeNumbersEntity);
    return d->m_treeItem->bytesData();
}

void AceTreeNumbersEntity::replaceImpl(int index, const char *data, int size) {
    Q_D(AceTreeNumbersEntity);
    d->m_treeItem->replaceBytes(index * d->type_size, QByteArray(data, size * d->type_size));
}

void AceTreeNumbersEntity::insertImpl(int index, const char *data, int size) {
    Q_D(AceTreeNumbersEntity);
    d->m_treeItem->insertBytes(index * d->type_size, QByteArray(data, size * d->type_size));
}

void AceTreeNumbersEntity::removeImpl(int index, int size) {
    Q_D(AceTreeNumbersEntity);
    d->m_treeItem->removeBytes(index * d->type_size, size * d->type_size);
}

void AceTreeNumbersEntity::initType(
    int type_size, QByteArray (*arr_to_bytes)(const QJsonArray &),
    QJsonArray (*bytes_to_arr)(const QByteArray &)) {
    Q_D(AceTreeNumbersEntity);
    d->type_size = type_size;
    d->arr_to_bytes = arr_to_bytes;
    d->bytes_to_arr = bytes_to_arr;
}

namespace AceTreePrivate {
    using NN = AceTreeEntityNumbersHelper<int>;
    class CC : public AceTreeNumbersEntity, public NN {};
    static const uintptr_t offset_n =
        intptr_t(static_cast<NN *>(reinterpret_cast<CC *>(255))) - 255;
} // namespace AceTreePrivate

AceTreeNumbersEntity *AceTreeNumbersEntity::get_entity_helper(const void *ptr) {
    return reinterpret_cast<AceTreeNumbersEntity *>(uintptr_t(ptr) - AceTreePrivate::offset_n);
}
