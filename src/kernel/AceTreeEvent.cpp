#include "AceTreeEvent.h"

#include "AceTreeItem_p.h"

bool AceTreeEvent::isChangeType(AceTreeEvent::Type type) {
    switch (type) {
        case DynamicDataChange:
        case RowsAboutToMove:
        case RowsAboutToRemove:
        case RecordAboutToRemove:
        case ElementAboutToRemove:
        case RootAboutToChange:
            return false;
            break;
        default:
            return true;
            break;
    }
}

AceTreeEvent::AceTreeEvent(Type type) : t(type) {
}

AceTreeEvent::~AceTreeEvent() {
}

void AceTreeEvent::clean() {
}

AceTreeItemEvent::AceTreeItemEvent(Type type, AceTreeItem *item)
    : AceTreeEvent(type), m_item(item) {
}

AceTreeItemEvent::~AceTreeItemEvent() {
}

AceTreeValueEvent::AceTreeValueEvent(AceTreeEvent::Type type, AceTreeItem *item, const QString &key,
                                     const QVariant &value, const QVariant &oldValue)
    : AceTreeItemEvent(type, item), k(key), v(value), oldv(oldValue) {
}

AceTreeValueEvent::~AceTreeValueEvent() {
}

AceTreeEvent *AceTreeValueEvent::clone() const {
    return new AceTreeValueEvent(t, m_item, k, v, oldv);
}

AceTreeBytesEvent::AceTreeBytesEvent(Type type, AceTreeItem *item, int index,
                                     const QByteArray &bytes, const QByteArray &oldBytes)
    : AceTreeItemEvent(type, item), m_index(index), b(bytes), oldb(oldBytes) {
}

AceTreeBytesEvent::~AceTreeBytesEvent() {
}

AceTreeEvent *AceTreeBytesEvent::clone() const {
    return new AceTreeBytesEvent(t, m_item, m_index, b, oldb);
}

AceTreeRowsEvent::AceTreeRowsEvent(Type type, AceTreeItem *item, int index)
    : AceTreeItemEvent(type, item), m_index(index) {
}

AceTreeRowsEvent::~AceTreeRowsEvent() {
}

AceTreeRowsMoveEvent::AceTreeRowsMoveEvent(Type type, AceTreeItem *item, int index, int count,
                                           int dest)
    : AceTreeRowsEvent(type, item, index), cnt(count), dest(dest) {
}

AceTreeRowsMoveEvent::~AceTreeRowsMoveEvent() {
}

AceTreeEvent *AceTreeRowsMoveEvent::clone() const {
    return new AceTreeRowsMoveEvent(t, m_item, m_index, cnt, dest);
}

AceTreeRowsInsDelEvent::AceTreeRowsInsDelEvent(Type type, AceTreeItem *item, int index,
                                               const QVector<AceTreeItem *> &children)
    : AceTreeRowsEvent(type, item, index), m_children(children) {
}

AceTreeRowsInsDelEvent::~AceTreeRowsInsDelEvent() {
}

AceTreeEvent *AceTreeRowsInsDelEvent::clone() const {
    return new AceTreeRowsInsDelEvent(t, m_item, m_index, m_children);
}

AceTreeRecordEvent::AceTreeRecordEvent(Type type, AceTreeItem *item, int seq, AceTreeItem *child)
    : AceTreeItemEvent(type, item), s(seq), m_child(child) {
}

AceTreeRecordEvent::~AceTreeRecordEvent() {
}

AceTreeEvent *AceTreeRecordEvent::clone() const {
    return new AceTreeRecordEvent(t, m_item, s, m_child);
}

AceTreeElementEvent::AceTreeElementEvent(Type type, AceTreeItem *item, const QString &key,
                                         AceTreeItem *child)
    : AceTreeItemEvent(type, item), k(key), m_child(child) {
}

AceTreeElementEvent::~AceTreeElementEvent() {
}

AceTreeEvent *AceTreeElementEvent::clone() const {
    return new AceTreeElementEvent(t, m_item, k, m_child);
}

AceTreeRootEvent::AceTreeRootEvent(Type type, AceTreeItem *root, AceTreeItem *oldRoot)
    : AceTreeEvent(type), r(root), oldr(oldRoot) {
}

AceTreeRootEvent::~AceTreeRootEvent() {
}

AceTreeEvent *AceTreeRootEvent::clone() const {
    return new AceTreeRootEvent(t, r, oldr);
}
