#include "AceTreeNumbersEntity.h"
#include "AceTreeEntity_p.h"

#include <QMetaMethod>
#include <QRegularExpression>

class AceTreeNumbersEntityPrivate : public AceTreeEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeNumbersEntity)
public:
    AceTreeNumbersEntityPrivate() : type_size(0), arr_to_bytes(nullptr), bytes_to_arr(nullptr) {
    }
    ~AceTreeNumbersEntityPrivate() = default;

    void init_deferred() override;

    struct Signals {
        QMetaMethod replaced;
        QMetaMethod inserted;
        QMetaMethod removed;
    };

    Signals *_signals;

    void event(AceTreeEvent *event) override;

    size_t type_size;
    QByteArray (*arr_to_bytes)(const QJsonArray &);
    QJsonArray (*bytes_to_arr)(const QByteArray &);
};

using NumbersSignals = AceTreeNumbersEntityPrivate::Signals;
using NumbersSignalIndexes = QHash<const QMetaObject *, QSharedPointer<NumbersSignals>>;
Q_GLOBAL_STATIC(NumbersSignalIndexes, numbersIndexes)

static NumbersSignals *findNumbersSignal(const QMetaObject *metaObject) {
    auto it = numbersIndexes->find(metaObject);
    if (it != numbersIndexes->end()) {
        return it->data();
    }

    NumbersSignals s;
    QHash<QString, QPair<QMetaMethod *, bool (*)(QMetaMethod *)>> fields{
        {
            "replaced",
            {
                &s.replaced,
                [](QMetaMethod *method) {
                    return QRegularExpression(R"(replaced\(int,QVector<\w+>\))")
                        .match(method->methodSignature())
                        .hasMatch(); //
                },
            },
        },
        {
            "inserted",
            {
                &s.inserted,
                [](QMetaMethod *method) {
                    return QRegularExpression(R"(inserted\(int,QVector<\w+>\))")
                        .match(method->methodSignature())
                        .hasMatch(); //
                },
            },
        },
        {"removed",
         {
             &s.removed,
             [](QMetaMethod *method) {
                 return method->methodSignature() == "removed(int,int)"; //
             },
         }},
    };

    int methodCount = metaObject->methodCount();

    for (int i = 0; i < methodCount; ++i) {
        QMetaMethod method = metaObject->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            auto it1 = fields.find(method.name());
            if (it1 == fields.end())
                continue;
            auto &ref = it1.value();
            if (!ref.second(&method))
                continue;
            *ref.first = method;
        }
    }

    it = numbersIndexes->insert(metaObject, QSharedPointer<NumbersSignals>::create(s));
    return it->data();
}

void AceTreeNumbersEntityPrivate::init_deferred() {
    AceTreeEntityPrivate::init_deferred();

    Q_Q(AceTreeNumbersEntity);
    _signals = findNumbersSignal(q->metaObject());
}

void AceTreeNumbersEntityPrivate::event(AceTreeEvent *event) {
    AceTreeEntityPrivate::event(event);

    Q_Q(AceTreeNumbersEntity);
    auto doIt = [q](AceTreeBytesEvent *e, QMetaMethod &mm) {
        if (!mm.isValid()) {
            return;
        }
        const auto &index = e->index();
        const auto &bytes = e->bytes();
        mm.invoke(q,                                                                 //
                  QGenericArgument("int", &index),                                   //
                  QGenericArgument(QMetaType::typeName(mm.parameterType(1)), &bytes) //
        );
    };
    auto doIt2 = [q](AceTreeBytesEvent *e, QMetaMethod &mm) {
        if (!mm.isValid()) {
            return;
        }
        const auto &index = e->index();
        const auto &count = e->bytes().size();
        mm.invoke(q,                               //
                  QGenericArgument("int", &index), //
                  QGenericArgument("int", &count)  //
        );
    };

    switch (event->type()) {
        case AceTreeEvent::BytesReplace: {
            doIt(static_cast<AceTreeBytesEvent *>(event), _signals->replaced);
            break;
        }
        case AceTreeEvent::BytesInsert: {
            doIt(static_cast<AceTreeBytesEvent *>(event), _signals->inserted);
            break;
        }
        case AceTreeEvent::BytesRemove: {
            doIt2(static_cast<AceTreeBytesEvent *>(event), _signals->removed);
            break;
        }
        default:
            break;
    }
}

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

void AceTreeNumbersEntity::initType(int type_size, QByteArray (*arr_to_bytes)(const QJsonArray &),
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
