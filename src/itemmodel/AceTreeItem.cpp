#include "AceTreeItem.h"
#include "AceTreeItem_p.h"

#include "AceTreeModel_p.h"

#include <QDataStream>
#include <QDebug>
#include <QtEndian>

// #define ENABLE_DEBUG_COUNT

#define myWarning(func) (qWarning().nospace() << "AceTreeItem::" << (func) << "():").space()

static const char SIGN_TREE_ITEM[] = "item";

static bool validateArrayQueryArguments(int index, int size) {
    return index >= 0 && index <= size;
}

static bool validateArrayRemoveArguments(int index, int &count, int size) {
    return (index >= 0 && index <= size)                                 // index bound
           && ((count = qMin(count, size - index)) > 0 && count <= size) // count bound
        ;
}

#ifdef ENABLE_DEBUG_COUNT
static int item_count = 0;
#endif

// Item
AceTreeItemPrivate::AceTreeItemPrivate() {
#ifdef ENABLE_DEBUG_COUNT
    item_count++;
    qDebug() << "AceTreeItemPrivate construct, left" << item_count;
#endif

    allowDelete = false;
    status = AceTreeItem::Root;
    m_managed = false;
    parent = nullptr;
    model = nullptr;
    m_index = 0;
    maxRecordSeq = 0;
}

AceTreeItemPrivate::~AceTreeItemPrivate() {
    Q_Q(AceTreeItem);

    // Clear subscribers
    for (const auto &sub : subscribers) {
        sub->d->m_treeItem = nullptr;
    }

    if (model) {
        if (!allowDelete) {
            qFatal("AceTreeItem::~AceTreeItem(): deleting a managed item may cause crash!!!");
        }

        auto d = model->d_func();
        if (!d->is_destruct)
            model->d_func()->removeIndex(m_index);
    } else {
        switch (status) {
            case AceTreeItem::Row:
                parent->removeRow(q);
                break;
            case AceTreeItem::Record:
                parent->removeRecord(q);
                break;
            case AceTreeItem::Element:
                parent->removeElement(q);
                break;
            default:
                break;
        }
    }

    qDeleteAll(vector);
    qDeleteAll(set);
    qDeleteAll(records);

#ifdef ENABLE_DEBUG_COUNT
    item_count--;
    qDebug() << "AceTreeItemPrivate destroy, left" << item_count << q;
#endif
}

void AceTreeItemPrivate::init() {
}

bool AceTreeItemPrivate::testModifiable(const char *func) const {
    Q_Q(const AceTreeItem);

    if (m_managed) {
        myWarning(func) << "the item is now obsolete" << q;
        return false;
    }

    if (model && !model->isWritable()) {
        myWarning(func) << "the model is now not writable" << q;
        return false;
    }

    return true;
}

bool AceTreeItemPrivate::testInsertable(const char *func, const AceTreeItem *item) const {
    Q_Q(const AceTreeItem);
    if (!item) {
        myWarning(func) << "item is null";
        return false;
    }

    if (item == q) {
        myWarning(func) << "item cannot be its child itself" << item;
        return false;
    }

    // Validate
    if (!item->isFree()) {
        myWarning(func) << "item is not free" << item;
        return false;
    }

    return true;
}

void AceTreeItemPrivate::sendEvent(AceTreeEvent *e) {
    for (const auto &sub : qAsConst(subscribers))
        sub->event(e);
    if (model)
        model->d_func()->event_helper(e);
}

void AceTreeItemPrivate::changeManaged(bool managed) {
    Q_Q(AceTreeItem);
    propagate(q, [managed](AceTreeItem *item) {
        item->d_func()->m_managed = managed; //
    });
}

// void AceTreeItemPrivate::execute(AceTreeEvent *e, bool undo) {
//     switch (e->type()) {
//         case AceTreeEvent::PropertyChange: {
//             auto e1 = static_cast<AceTreeValueEvent *>(e);
//             auto item = e1->parent();
//             item->d_func()->setProperty_helper(e1->key(), undo ? e1->oldValue() : e1->value());
//             break;
//         }
//         case AceTreeEvent::BytesReplace: {
//             auto e1 = static_cast<AceTreeBytesEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::BytesInsert:
//         case AceTreeEvent::BytesRemove: {
//             auto e1 = static_cast<AceTreeBytesEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::RowsMove: {
//             auto e1 = static_cast<AceTreeRowsMoveEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::RowsInsert:
//         case AceTreeEvent::RowsRemove: {
//             auto e1 = static_cast<AceTreeRowsInsDelEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::RecordAdd:
//         case AceTreeEvent::RecordRemove: {
//             auto e1 = static_cast<AceTreeRecordEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::ElementAdd:
//         case AceTreeEvent::ElementRemove: {
//             auto e1 = static_cast<AceTreeElementEvent *>(e);
//             break;
//         }
//         case AceTreeEvent::RootChange: {
//             auto e1 = static_cast<AceTreeRootEvent *>(e);
//             break;
//         }
//         default:
//             break;
//     }
// }

void AceTreeItemPrivate::setProperty_helper(const QString &key, const QVariant &value) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    QVariant oldValue;
    auto it = properties.find(key);
    if (it == properties.end()) {
        if (!value.isValid())
            return;
        properties.insert(key, value);
    } else {
        oldValue = it.value();
        if (!value.isValid())
            properties.erase(it);
        else if (oldValue == value)
            return;
        else
            it.value() = value;
    }

    // Propagate signal
    AceTreeValueEvent e(AceTreeEvent::PropertyChange, q, key, value, oldValue);
    sendEvent(&e);
}

void AceTreeItemPrivate::replaceBytes_helper(int index, const QByteArray &bytes) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    auto len = bytes.size();
    auto oldBytes = byteArray.mid(index, len);

    // Do change
    int newSize = index + len;
    if (newSize > byteArray.size())
        byteArray.resize(newSize);
    byteArray.replace(index, len, bytes);

    // Propagate signal
    AceTreeBytesEvent e(AceTreeEvent::BytesReplace, q, index, bytes, oldBytes);
    sendEvent(&e);
}

void AceTreeItemPrivate::insertBytes_helper(int index, const QByteArray &bytes) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    // Do change
    byteArray.insert(index, bytes);

    // Propagate signal
    AceTreeBytesEvent e(AceTreeEvent::BytesInsert, q, index, bytes);
    sendEvent(&e);
}

void AceTreeItemPrivate::removeBytes_helper(int index, int size) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    auto bytes = byteArray.mid(index, size);

    // Do change
    byteArray.remove(index, size);

    // Propagate signal
    AceTreeBytesEvent e(AceTreeEvent::BytesRemove, q, index, bytes);
    sendEvent(&e);
}

void AceTreeItemPrivate::insertRows_helper(int index, const QVector<AceTreeItem *> &items) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    // Do change
    vector.insert(vector.begin() + index, items.size(), nullptr);
    for (int i = 0; i < items.size(); ++i) {
        auto item = items[i];
        auto d = item->d_func();
        vector[index + i] = item;
        d->parent = q;

        // Update status
        d->status = AceTreeItem::Row;
        if (d->m_managed)
            d->changeManaged(false);
    }

    // Propagate signal
    AceTreeRowsInsDelEvent e(AceTreeEvent::RowsInsert, q, index, items);
    sendEvent(&e);
}

/**
 * @brief Move item inside the array
 *
 */
template <template <class> class Array, class T>
static void arrayMove(Array<T> &arr, int index, int count, int dest) {
    decltype(typename std::remove_reference<decltype(arr)>::type()) tmp;
    tmp.resize(count);
    std::copy(arr.begin() + index, arr.begin() + index + count, tmp.begin());

    // Do change
    int correctDest;
    if (dest > index) {
        correctDest = dest - count;
        auto sz = correctDest - index;
        for (int i = 0; i < sz; ++i) {
            arr[index + i] = arr[index + count + i];
        }
    } else {
        correctDest = dest;
        auto sz = index - dest;
        for (int i = sz - 1; i >= 0; --i) {
            arr[dest + count + i] = arr[dest + i];
        }
    }
    std::copy(tmp.begin(), tmp.end(), arr.begin() + correctDest);
}

void AceTreeItemPrivate::moveRows_helper(int index, int count, int dest) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    // Pre-Propagate signal
    AceTreeRowsMoveEvent e1(AceTreeEvent::RowsAboutToMove, q, index, count, dest);
    sendEvent(&e1);

    // Do change
    arrayMove(vector, index, count, dest);

    // Propagate signal
    AceTreeRowsMoveEvent e2(AceTreeEvent::RowsMove, q, index, count, dest);
    sendEvent(&e2);
}

void AceTreeItemPrivate::removeRows_helper(int index, int count) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    QVector<AceTreeItem *> tmp;
    tmp.resize(count);
    std::copy(vector.begin() + index, vector.begin() + index + count, tmp.begin());

    // Pre-Propagate signal
    AceTreeRowsInsDelEvent e1(AceTreeEvent::RowsAboutToRemove, q, index, tmp);
    sendEvent(&e1);

    // Do change
    for (const auto &item : qAsConst(tmp)) {
        auto d = item->d_func();
        d->parent = nullptr;

        // Update status
        d->status = AceTreeItem::Root;
        if (model)
            d->changeManaged(true);
    }
    vector.erase(vector.begin() + index, vector.begin() + index + count);

    // Propagate signal
    AceTreeRowsInsDelEvent e2(AceTreeEvent::RowsRemove, q, index, tmp);
    sendEvent(&e2);
}

void AceTreeItemPrivate::addRecord_helper(int seq, AceTreeItem *item) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    // Do change
    auto d = item->d_func();
    d->parent = q;

    records.insert(seq, item);
    recordIndexes.insert(item, seq);

    // Update status
    d->status = AceTreeItem::Record;
    if (d->m_managed)
        d->changeManaged(false);

    // Propagate signal
    AceTreeRecordEvent e(AceTreeEvent::RecordAdd, q, seq, item);
    sendEvent(&e);
}

void AceTreeItemPrivate::removeRecord_helper(int seq) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    auto it = records.find(seq);
    auto item = it.value();
    auto d = item->d_func();

    // Pre-Propagate signal
    AceTreeRecordEvent e1(AceTreeEvent::RecordAboutToRemove, q, seq, item);
    sendEvent(&e1);

    // Do change
    d->parent = nullptr;
    records.erase(it);
    recordIndexes.remove(item);

    // Update status
    d->status = AceTreeItem::Root;
    if (model)
        d->changeManaged(true);

    // Propagate signal
    AceTreeRecordEvent e2(AceTreeEvent::RecordRemove, q, seq, item);
    sendEvent(&e2);
}

void AceTreeItemPrivate::addElement_helper(const QString &key, AceTreeItem *item) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    // Do change
    auto d = item->d_func();
    d->parent = q;
    set.insert(key, item);
    setIndexes.insert(item, key);

    // Update status
    d->status = AceTreeItem::Element;
    if (d->m_managed)
        d->changeManaged(false);

    // Propagate signal
    AceTreeElementEvent e(AceTreeEvent::ElementAdd, q, key, item);
    sendEvent(&e);
}

void AceTreeItemPrivate::removeElement_helper(const QString &key) {
    Q_Q(AceTreeItem);
    AceTreeModelPrivate::InterruptGuard _guard(model);

    auto it = set.find(key);
    auto item = it.value();
    auto d = item->d_func();

    // Pre-Propagate signal
    AceTreeElementEvent e1(AceTreeEvent::ElementAboutToRemove, q, key, item);
    sendEvent(&e1);

    // Do change
    d->parent = nullptr;
    set.erase(it);
    setIndexes.remove(item);

    // Update status
    d->status = AceTreeItem::Root;
    if (model)
        d->changeManaged(true);

    // Propagate signal
    AceTreeElementEvent e2(AceTreeEvent::ElementRemove, q, key, item);
    sendEvent(&e2);
}

AceTreeItem *AceTreeItemPrivate::read_helper(QDataStream &in, bool user) {
    // Read head
    char sign[sizeof(SIGN_TREE_ITEM) - 1];
    in.readRawData(sign, sizeof(sign));
    if (memcmp(SIGN_TREE_ITEM, sign, sizeof(sign)) != 0) {
        return nullptr;
    }

    auto item = new AceTreeItem();
    auto d = item->d_func();

    // Read index
    size_t tmp;
    in >> (user ? tmp : d->m_index);

    // Read properties
    AceTreePrivate::operator>>(in, d->properties);
    if (in.status() != QDataStream::Ok) {
        myWarning(__func__) << "read properties failed";
        goto abort;
    }

    // Read byte array
    in >> d->byteArray;
    if (in.status() != QDataStream::Ok) {
        myWarning(__func__) << "read byte arrray failed";
        goto abort;
    }

    int size;

    // Read vector
    in >> size;
    d->vector.reserve(size);
    for (int i = 0; i < size; ++i) {
        auto child = read_helper(in, user);
        if (!child) {
            myWarning(__func__) << "read vector item failed";
            goto abort;
        }
        child->d_func()->parent = item;
        d->vector.append(child);
    }

    // Read record table
    in >> size;
    d->records.reserve(size);
    d->recordIndexes.reserve(size);
    for (int i = 0; i < size; ++i) {
        int seq;
        in >> seq;
        auto child = read_helper(in, user);
        if (!child) {
            myWarning(__func__) << "read record item failed";
            goto abort;
        }
        child->d_func()->parent = item;
        d->records.insert(seq, child);
        d->recordIndexes.insert(child, seq);
    }
    in >> d->maxRecordSeq;

    // Read set
    in >> size;
    d->set.reserve(size);
    d->setIndexes.reserve(size);
    for (int i = 0; i < size; ++i) {
        QByteArray key;
        in >> key;
        if (in.status() != QDataStream::Ok) {
            myWarning(__func__) << "read set key failed";
            goto abort;
        }

        QString keyStr = QString::fromUtf8(key);
        auto child = read_helper(in, user);
        if (!child) {
            myWarning(__func__) << "read set item failed";
            goto abort;
        }
        child->d_func()->parent = item;
        d->set.insert(keyStr, child);
        d->setIndexes.insert(child, keyStr);
    }

    return item;

abort:
    delete item;
    return nullptr;
}

void AceTreeItemPrivate::write_helper(QDataStream &out, bool user) const {
    out.writeRawData(SIGN_TREE_ITEM, sizeof(SIGN_TREE_ITEM) - 1);

    auto d = this;

    // Write index
    out << (user ? size_t(0) : d->m_index);

    // Write properties
    AceTreePrivate::operator<<(out, d->properties);

    // Write byte array
    out << d->byteArray;

    // Write vector
    out << d->vector.size();
    for (const auto &item : d->vector) {
        item->d_func()->write_helper(out, user);
    }

    // Write record table
    out << d->records.size();
    for (auto it = d->records.begin(); it != d->records.end(); ++it) {
        out << it.key();
        it.value()->d_func()->write_helper(out, user);
    }
    out << d->maxRecordSeq;

    // Write set
    out << d->set.size();
    for (auto it = d->set.begin(); it != d->set.end(); ++it) {
        out << it.key().toUtf8();
        it.value()->d_func()->write_helper(out, user);
    }
}

AceTreeItem *AceTreeItemPrivate::clone_helper(bool user) const {
    auto d = this;
    auto item = new AceTreeItem();

    auto d2 = item->d_func();
    if (!user) {
        d2->m_index = d->m_index;
    }
    d2->dynamicData = d->dynamicData;
    d2->properties = d->properties;
    d2->byteArray = d->byteArray;

    d2->vector.reserve(d->vector.size());
    for (auto &child : d->vector) {
        auto newChild = child->d_func()->clone_helper(user);
        newChild->d_func()->parent = item;

        d2->vector.append(newChild);
    }

    d2->records.reserve(d->records.size());
    d2->recordIndexes.reserve(d->recordIndexes.size());
    for (auto it = d->records.begin(); it != d->records.end(); ++it) {
        auto newChild = it.value()->d_func()->clone_helper(user);
        newChild->d_func()->parent = item;

        d2->records.insert(it.key(), newChild);
        d2->recordIndexes.insert(newChild, it.key());
    }
    d2->maxRecordSeq = d->maxRecordSeq;

    d2->set.reserve(d->set.size());
    d2->setIndexes.reserve(d->setIndexes.size());
    for (auto it = d->set.begin(); it != d->set.end(); ++it) {
        auto newChild = it.value()->d_func()->clone_helper(user);
        newChild->d_func()->parent = item;

        d2->set.insert(it.key(), newChild);
        d2->setIndexes.insert(newChild, it.key());
    }

    return item;
}

AceTreeItemPrivate *AceTreeItemPrivate::get(AceTreeItem *item) {
    return item->d_func();
}

void AceTreeItemPrivate::propagate(AceTreeItem *item,
                                   const std::function<void(AceTreeItem *)> &func) {
    func(item);
    auto d = item->d_func();
    for (const auto &child : qAsConst(d->vector))
        propagate(child, func);
    for (const auto &child : qAsConst(d->set))
        propagate(child, func);
    for (const auto &child : qAsConst(d->records))
        propagate(child, func);
}

void AceTreeItemPrivate::forceDeleteItem(AceTreeItem *item) {
    if (!item)
        return;

    AceTreeItemPrivate::propagate(item, [](AceTreeItem *item) {
        item->d_func()->allowDelete = true; //
    });
    delete item;
}

AceTreeItem::AceTreeItem() : AceTreeItem(*new AceTreeItemPrivate()) {
}

AceTreeItem::~AceTreeItem() {
}

bool AceTreeItem::addSubscriber(AceTreeItemSubscriber *sub) {
    Q_D(AceTreeItem);
    auto res = d->subscribers.append(sub).second;
    if (res) {
        sub->d->m_treeItem = this;
    }
    return res;
}

bool AceTreeItem::removeSubscriber(AceTreeItemSubscriber *sub) {
    Q_D(AceTreeItem);
    auto res = d->subscribers.remove(sub);
    if (res) {
        sub->d->m_treeItem = nullptr;
    }
    return res;
}

QList<AceTreeItemSubscriber *> AceTreeItem::subscribers() const {
    Q_D(const AceTreeItem);
    return d->subscribers.values();
}

int AceTreeItem::subscriberCount() const {
    Q_D(const AceTreeItem);
    return d->subscribers.size();
}

bool AceTreeItem::hasSubscriber(AceTreeItemSubscriber *sub) const {
    Q_D(const AceTreeItem);
    return d->subscribers.contains(sub);
}

AceTreeItem::Status AceTreeItem::status() const {
    Q_D(const AceTreeItem);
    return d->status;
}

QVariant AceTreeItem::dynamicData(const QString &key) const {
    Q_D(const AceTreeItem);
    return d->dynamicData.value(key);
}

void AceTreeItem::setDynamicData(const QString &key, const QVariant &value) {
    Q_D(AceTreeItem);

    QVariant oldValue;
    auto it = d->dynamicData.find(key);
    if (it == d->dynamicData.end()) {
        if (!value.isValid())
            return;
        d->dynamicData.insert(key, value);
    } else {
        oldValue = it.value();
        if (!value.isValid())
            d->dynamicData.erase(it);
        else if (oldValue == value)
            return;
        else
            it.value() = value;
    }

    // Propagate signal
    AceTreeValueEvent e(AceTreeEvent::DynamicDataChange, this, key, value, oldValue);
    d->sendEvent(&e);
}

QStringList AceTreeItem::dynamicDataKeys() const {
    Q_D(const AceTreeItem);
    return d->dynamicData.keys();
}

QVariantHash AceTreeItem::dynamicDataMap() const {
    Q_D(const AceTreeItem);
    return d->dynamicData;
}

AceTreeItem *AceTreeItem::parent() const {
    Q_D(const AceTreeItem);
    return d->parent;
}

AceTreeModel *AceTreeItem::model() const {
    Q_D(const AceTreeItem);
    return d->model;
}

size_t AceTreeItem::index() const {
    Q_D(const AceTreeItem);
    return d->m_index;
}

bool AceTreeItem::isFree() const {
    Q_D(const AceTreeItem);

    // The item is not free if it has parent or model

    return !d->model && !d->parent;
}

bool AceTreeItem::isManaged() const {
    Q_D(const AceTreeItem);
    return d->m_managed;
}

bool AceTreeItem::isWritable() const {
    Q_D(const AceTreeItem);
    return d->model ? d->model->isWritable() : true;
}

QVariant AceTreeItem::property(const QString &key) const {
    Q_D(const AceTreeItem);
    return d->properties.value(key, {});
}

bool AceTreeItem::setProperty(const QString &key, const QVariant &value) {
    Q_D(AceTreeItem);
    if (!d->testModifiable(__func__))
        return false;

    d->setProperty_helper(key, value);
    return true;
}

QStringList AceTreeItem::propertyKeys() const {
    Q_D(const AceTreeItem);
    return d->properties.keys();
}

QVariantHash AceTreeItem::propertyMap() const {
    Q_D(const AceTreeItem);
    return d->properties;
}

bool AceTreeItem::replaceBytes(int index, const QByteArray &bytes) {
    Q_D(AceTreeItem);
    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayQueryArguments(index, d->byteArray.size()) || bytes.isEmpty()) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }

    d->replaceBytes_helper(index, bytes);
    return true;
}

bool AceTreeItem::insertBytes(int start, const QByteArray &bytes) {
    Q_D(AceTreeItem);
    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayQueryArguments(start, d->byteArray.size()) || bytes.isEmpty()) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }

    d->insertBytes_helper(start, bytes);
    return true;
}

bool AceTreeItem::removeBytes(int start, int size) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayRemoveArguments(start, size, d->byteArray.size())) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }

    d->removeBytes_helper(start, size);
    return true;
}

QByteArray AceTreeItem::bytes() const {
    Q_D(const AceTreeItem);
    return d->byteArray;
}

const char *AceTreeItem::bytesData() const {
    Q_D(const AceTreeItem);
    return d->byteArray.constData();
}

QByteArray AceTreeItem::midBytes(int start, int len) const {
    Q_D(const AceTreeItem);
    return d->byteArray.mid(start, len);
}

int AceTreeItem::bytesIndexOf(const QByteArray &bytes, int start) const {
    Q_D(const AceTreeItem);
    return d->byteArray.indexOf(bytes, start);
}

int AceTreeItem::bytesSize() const {
    Q_D(const AceTreeItem);
    return d->byteArray.size();
}

bool AceTreeItem::insertRows(int index, const QVector<AceTreeItem *> &items) {
    Q_D(AceTreeItem);
    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayQueryArguments(index, d->vector.size()) || items.isEmpty()) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }
    for (const auto &item : items) {
        if (!d->testInsertable(__func__, item)) {
            return false;
        }
    }

    if (d->model) {
        for (const auto &item : items) {
            d->model->d_func()->propagate_model(item);
        }
    }

    d->insertRows_helper(index, items);
    return true;
}

bool AceTreeItem::moveRows(int index, int count, int dest) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayRemoveArguments(index, count, d->vector.size()) ||
        (dest >= index && dest <= index + count) // dest bound
    ) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }

    d->moveRows_helper(index, count, dest);
    return true;
}

bool AceTreeItem::removeRows(int index, int count) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!validateArrayRemoveArguments(index, count, d->vector.size())) {
        myWarning(__func__) << "invalid parameters";
        return false;
    }

    d->removeRows_helper(index, count);
    return true;
}

bool AceTreeItem::removeRow(AceTreeItem *item) {
    Q_D(AceTreeItem);
    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!item) {
        myWarning(__func__) << "trying to remove a null row from" << this;
        return false;
    }
    int index;
    if (item->parent() != this || (index = d->vector.indexOf(item)) < 0) {
        myWarning(__func__) << "item" << item << "is not a row of" << this;
        return false;
    }

    d->removeRows_helper(index, 1);
    return true;
}

AceTreeItem *AceTreeItem::row(int index) const {
    Q_D(const AceTreeItem);
    return (index >= 0 && index < d->vector.size()) ? d->vector.at(index) : nullptr;
}

QVector<AceTreeItem *> AceTreeItem::rows() const {
    Q_D(const AceTreeItem);
    return d->vector;
}

int AceTreeItem::rowIndexOf(AceTreeItem *item) const {
    Q_D(const AceTreeItem);
    return (item && item->parent() == this) ? d->vector.indexOf(item) : -1;
}

int AceTreeItem::rowCount() const {
    Q_D(const AceTreeItem);
    return d->vector.size();
}

int AceTreeItem::addRecord(AceTreeItem *item) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return -1;

    // Validate
    if (!d->testInsertable(__func__, item))
        return -1;

    d->model->d_func()->propagate_model(item);

    auto seq = ++d->maxRecordSeq;
    d->addRecord_helper(seq, item);
    return seq;
}

bool AceTreeItem::removeRecord(int seq) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!d->records.contains(seq)) {
        myWarning(__func__) << "seq num" << seq << "doesn't exists in" << this;
        return false;
    }

    d->removeRecord_helper(seq);
    return true;
}

bool AceTreeItem::removeRecord(AceTreeItem *item) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!item) {
        myWarning(__func__) << "trying to remove a null record from" << this;
        return false;
    }
    int seq = d->recordIndexes.value(item, -1);
    if (seq < 0) {
        myWarning(__func__) << "seq num" << seq << "doesn't exists in" << this;
        return false;
    }

    d->removeRecord_helper(seq);
    return true;
}

AceTreeItem *AceTreeItem::record(int seq) {
    Q_D(const AceTreeItem);
    return d->records.value(seq, nullptr);
}

int AceTreeItem::recordSequenceOf(AceTreeItem *item) const {
    Q_D(const AceTreeItem);
    return d->recordIndexes.value(item, -1);
}

QList<int> AceTreeItem::records() const {
    Q_D(const AceTreeItem);
    auto seqs = d->records.keys();
    std::sort(seqs.begin(), seqs.end());
    return seqs;
}

QMap<int, AceTreeItem *> AceTreeItem::recordMap() const {
    Q_D(const AceTreeItem);
    QMap<int, AceTreeItem *> res;
    for (auto it = d->records.begin(); it != d->records.end(); ++it) {
        res.insert(it.key(), it.value());
    }
    return res;
}

int AceTreeItem::recordCount() const {
    Q_D(const AceTreeItem);
    return d->records.size();
}

int AceTreeItem::maxRecordSequence() const {
    Q_D(const AceTreeItem);
    return d->maxRecordSeq;
}

bool AceTreeItem::addElement(const QString &key, AceTreeItem *item) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!d->testInsertable(__func__, item))
        return false;

    if (d->set.contains(key)) {
        myWarning(__func__) << "key" << key << "already exists in" << this;
        return false;
    }

    d->model->d_func()->propagate_model(item);

    d->addElement_helper(key, item);
    return true;
}

bool AceTreeItem::removeElement(const QString &key) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!d->set.contains(key)) {
        myWarning(__func__) << "key" << key << "doesn't exist in" << this;
        return false;
    }

    d->removeElement_helper(key);
    return true;
}

bool AceTreeItem::removeElement(AceTreeItem *item) {
    Q_D(AceTreeItem);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!item) {
        myWarning(__func__) << "trying to remove a null element from" << this;
        return false;
    }

    auto it = d->setIndexes.find(item);
    if (item->parent() != this || it == d->setIndexes.end()) {
        myWarning(__func__) << "item" << item << "is not an element of" << this;
        return false;
    }

    d->removeElement_helper(it.value());
    return true;
}

AceTreeItem *AceTreeItem::element(const QString &key) {
    Q_D(const AceTreeItem);
    return d->set.value(key, nullptr);
}

QString AceTreeItem::elementKeyOf(AceTreeItem *item) const {
    Q_D(const AceTreeItem);
    return d->setIndexes.value(item);
}

QStringList AceTreeItem::elementKeys() const {
    Q_D(const AceTreeItem);
    return d->set.keys();
}

QList<AceTreeItem *> AceTreeItem::elements() const {
    Q_D(const AceTreeItem);
    return d->set.values();
}

QHash<QString, AceTreeItem *> AceTreeItem::elementHash() const {
    Q_D(const AceTreeItem);
    return d->set;
}

QMap<QString, AceTreeItem *> AceTreeItem::elementMap() const {
    Q_D(const AceTreeItem);
    QMap<QString, AceTreeItem *> res;
    for (auto it = d->set.begin(); it != d->set.end(); ++it) {
        res.insert(it.key(), it.value());
    }
    return res;
}

int AceTreeItem::elementCount() const {
    Q_D(const AceTreeItem);
    return d->set.size();
}

AceTreeItem *AceTreeItem::read(QDataStream &in) {
    return AceTreeItemPrivate::read_helper(in, true);
}

void AceTreeItem::write(QDataStream &out) const {
    Q_D(const AceTreeItem);
    return d->write_helper(out, true);
}

AceTreeItem *AceTreeItem::clone() const {
    Q_D(const AceTreeItem);
    return d->clone_helper(true);
}

AceTreeItem::AceTreeItem(AceTreeItemPrivate &d) : d_ptr(&d) {
    d.q_ptr = this;

    d.init();
}


AceTreeItemSubscriberPrivate::AceTreeItemSubscriberPrivate(AceTreeItemSubscriber *q) : q(q) {
    m_treeItem = nullptr;
}

AceTreeItemSubscriberPrivate::~AceTreeItemSubscriberPrivate() {
    if (m_treeItem)
        m_treeItem->removeSubscriber(q);
}

AceTreeItemSubscriber::AceTreeItemSubscriber() : d(new AceTreeItemSubscriberPrivate(this)) {
}

AceTreeItemSubscriber::~AceTreeItemSubscriber() {
    delete d;
}

AceTreeItem *AceTreeItemSubscriber::treeItem() const {
    return d->m_treeItem;
}

void AceTreeItemSubscriber::event(AceTreeEvent *e) {
    Q_UNUSED(e)
}

namespace AceTreePrivate {

    //    static int readBlock(QDataStream &stream, char *data, int len) {
    //        // Disable reads on failure in transacted stream
    //        if (stream.status() != QDataStream::Ok && stream.device()->isTransactionStarted())
    //            return -1;

    //        const int readResult = stream.device()->read(data, len);
    //        if (readResult != len)
    //            stream.setStatus(QDataStream::ReadPastEnd);
    //        return readResult;
    //    }

    //    QDataStream &operator>>(QDataStream &in, size_t &i) {
    //        i = 0;
    //        if (readBlock(in, reinterpret_cast<char *>(&i), sizeof(i)) != sizeof(i)) {
    //            i = 0;
    //        } else {
    //            if (in.byteOrder() != static_cast<QDataStream::ByteOrder>(QSysInfo::ByteOrder)) {
    //                i = qbswap(i);
    //            }
    //        }
    //        return in;
    //    }

    //    QDataStream &operator<<(QDataStream &out, size_t i) {
    //        if (out.byteOrder() != static_cast<QDataStream::ByteOrder>(QSysInfo::ByteOrder)) {
    //            i = qbswap(i);
    //        }
    //        if (out.device()->write((char *) &i, sizeof(i)) != sizeof(i))
    //            out.setStatus(QDataStream::WriteFailed);
    //        return out;
    //    }

    QDataStream &operator>>(QDataStream &in, QString &s) {
        QByteArray arr;
        in >> arr;
        if (in.status() == QDataStream::Ok) {
            s = QString::fromUtf8(arr);
        }
        return in;
    }

    QDataStream &operator<<(QDataStream &out, const QString &s) {
        return out << s.toUtf8();
    }

    QDataStream &operator>>(QDataStream &in, QVariantHash &s) {
        int size;
        in >> size;
        s.reserve(size);
        for (int i = 0; i < size; ++i) {
            QString key;
            AceTreePrivate::operator>>(in, key);
            if (in.status() != QDataStream::Ok) {
                break;
            }
            QVariant val;
            in >> val;
            if (in.status() != QDataStream::Ok) {
                break;
            }
            s.insert(key, val);
        }
        return in;
    }

    QDataStream &operator<<(QDataStream &out, const QVariantHash &s) {
        out << s.size();
        for (auto it = s.begin(); it != s.end(); ++it) {
            AceTreePrivate::operator<<(out, it.key());
            if (it->type() == QVariant::String) {
            }
            out << it.value();
        }
        return out;
    }

} // namespace AceTreePrivate
