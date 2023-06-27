#include "Operations.h"

#include <QDataStream>

#include "AceTreeItem_p.h"
#include "AceTreeModel_p.h"

namespace Operations {

    bool PropertyChangeOp::read(QDataStream &in) {
        in >> parent;
        AceTreePrivate::operator>>(in, key);
        in >> oldValue;
        in >> newValue;
        return in.status() == QDataStream::Ok;
    }

    bool PropertyChangeOp::write(QDataStream &out) const {
        out << parent;
        AceTreePrivate::operator<<(out, key);
        out << oldValue;
        out << newValue;
        return out.status() == QDataStream::Ok;
    }

    bool BytesReplaceOp::read(QDataStream &in) {
        in >> parent >> index >> oldBytes >> newBytes;
        return in.status() == QDataStream::Ok;
    }

    bool BytesReplaceOp::write(QDataStream &out) const {
        out << parent << index << oldBytes << newBytes;
        return out.status() == QDataStream::Ok;
    }

    bool BytesInsertRemoveOp::read(QDataStream &in) {
        in >> parent >> index >> bytes;
        return in.status() == QDataStream::Ok;
    }

    bool BytesInsertRemoveOp::write(QDataStream &out) const {
        out << parent << index << bytes;
        return out.status() == QDataStream::Ok;
    }

    RowsInsertOp::~RowsInsertOp() {
        qDeleteAll(children);
    }

    bool RowsInsertOp::read(QDataStream &in) {
        int size;
        in >> parent >> index >> size;
        children.reserve(size);
        in.skipRawData(sizeof(size_t) * size + sizeof(qint64));
        for (int i = 0; i < size; ++i) {
            auto item = AceTreeItem::read(in);
            if (!item) {
                in.setStatus(QDataStream::ReadCorruptData);
                qDeleteAll(children);
                children.clear();
                return false;
            }
            children.append(item);
        }
        return true;
    }

    bool RowsInsertOp::write(QDataStream &out) const {
        out << parent << index << children.size();
        for (const auto &child : qAsConst(children)) {
            out << child->index();
        }

        out.skipRawData(sizeof(qint64));
        auto &dev = *out.device();
        auto pos = dev.pos();

        for (const auto &child : qAsConst(children)) {
            child->write(out);
            if (out.status() != QDataStream::Ok) {
                return false;
            }
        }

        auto pos1 = dev.pos();
        dev.seek(pos - sizeof(qint64));
        out << (pos1 - pos);
        dev.seek(pos1);

        return true;
    }

    bool RowsInsertOp::readBrief(QDataStream &in) {
        int size;
        in >> parent >> index >> size;
        childrenIds.reserve(size);
        for (int i = 0; i < size; ++i) {
            size_t id;
            in >> id;
            childrenIds.append(id);
        }

        qint64 delta;
        in >> delta;
        in.skipRawData(delta);

        return in.status() == QDataStream::Ok;
    }

    bool RowsRemoveOp::read(QDataStream &in) {
        int size;
        in >> parent >> index >> size;
        children.reserve(size);
        for (int i = 0; i < size; ++i) {
            size_t id;
            in >> id;
            if (in.status() != QDataStream::Ok) {
                children.clear();
                return false;
            }
            children.append(id);
        }
        return in.status() == QDataStream::Ok;
    }

    bool RowsRemoveOp::write(QDataStream &out) const {
        out << parent << index << children.size();
        for (const auto &id : qAsConst(children)) {
            out << id;
        }
        return out.status() == QDataStream::Ok;
    }

    bool RowsMoveOp::read(QDataStream &in) {
        in >> parent >> index >> count >> dest;
        return in.status() == QDataStream::Ok;
    }

    bool RowsMoveOp::write(QDataStream &out) const {
        out << parent << index << count << dest;
        return out.status() == QDataStream::Ok;
    }

    RecordAddOp::~RecordAddOp() {
        delete child;
    }

    bool RecordAddOp::read(QDataStream &in) {
        in >> parent >> seq;
        in.skipRawData(sizeof(size_t) + sizeof(qint64));
        auto item = AceTreeItem::read(in);
        if (!item) {
            in.setStatus(QDataStream::ReadCorruptData);
            return false;
        }
        child = item;
        return true;
    }

    bool RecordAddOp::write(QDataStream &out) const {
        out << parent << seq << child->index();

        out.skipRawData(sizeof(qint64));
        auto &dev = *out.device();
        auto pos = dev.pos();

        child->write(out);

        auto pos1 = dev.pos();
        dev.seek(pos - sizeof(qint64));
        out << (pos1 - pos);
        dev.seek(pos1);

        return out.status() == QDataStream::Ok;
    }

    bool RecordAddOp::readBrief(QDataStream &in) {
        in >> parent >> seq >> childId;

        qint64 delta;
        in >> delta;
        in.skipRawData(delta);

        return in.status() == QDataStream::Ok;
    }

    bool RecordRemoveOp::read(QDataStream &in) {
        in >> parent >> seq >> child;
        return in.status() == QDataStream::Ok;
    }

    bool RecordRemoveOp::write(QDataStream &out) const {
        out << parent << seq << child;
        return out.status() == QDataStream::Ok;
    }

    ElementAddOp::~ElementAddOp() {
        delete child;
    }

    bool ElementAddOp::read(QDataStream &in) {
        in >> parent;
        AceTreePrivate::operator>>(in, key);
        in.skipRawData(sizeof(size_t) + sizeof(qint64));
        auto item = AceTreeItem::read(in);
        if (!item) {
            in.setStatus(QDataStream::ReadCorruptData);
            return false;
        }
        child = item;
        return true;
    }

    bool ElementAddOp::write(QDataStream &out) const {
        out << parent;
        AceTreePrivate::operator<<(out, key);
        out << child->index();

        out.skipRawData(sizeof(qint64));
        auto &dev = *out.device();
        auto pos = dev.pos();

        child->write(out);

        auto pos1 = dev.pos();
        dev.seek(pos - sizeof(qint64));
        out << (pos1 - pos);
        dev.seek(pos1);

        return out.status() == QDataStream::Ok;
    }

    bool ElementAddOp::readBrief(QDataStream &in) {
        in >> parent;
        AceTreePrivate::operator>>(in, key);
        in >> childId;

        qint64 delta;
        in >> delta;
        in.skipRawData(delta);

        return in.status() == QDataStream::Ok;
    }

    bool ElementRemoveOp::read(QDataStream &in) {
        in >> parent;
        AceTreePrivate::operator>>(in, key);
        in >> child;
        return in.status() == QDataStream::Ok;
    }

    bool ElementRemoveOp::write(QDataStream &out) const {
        out << parent;
        AceTreePrivate::operator<<(out, key);
        out << child;
        return out.status() == QDataStream::Ok;
    }

    RootChangeOp::~RootChangeOp() {
        delete newRoot;
    }

    bool RootChangeOp::read(QDataStream &in) {
        size_t id;
        in >> oldRoot >> id;
        if (id != 0) {
            auto item = AceTreeItem::read(in);
            if (!item) {
                in.setStatus(QDataStream::ReadCorruptData);
                return false;
            }
            newRoot = item;
        }
        return true;
    }

    bool RootChangeOp::write(QDataStream &out) const {
        // Write old root id
        out << oldRoot;

        // Write new root id
        if (newRoot) {
            out << newRoot->index();

            // Write new root
            newRoot->write(out);
        } else {
            out << size_t(0);
        }
        return out.status() == QDataStream::Ok;
    }

    bool RootChangeOp::readBrief(QDataStream &in) {
        in >> oldRoot >> newRootId;
        return in.status() == QDataStream::Ok;
    }

    BaseOp *toOp(AceTreeEvent *e) {
        BaseOp *res = nullptr;
        switch (e->type()) {
            case AceTreeEvent::PropertyChange: {
                auto event = static_cast<AceTreeValueEvent *>(e);
                auto op = new PropertyChangeOp();
                op->parent = event->parent()->index();
                op->key = event->key();
                op->oldValue = event->oldValue();
                op->newValue = event->value();
                res = op;
                break;
            }
            case AceTreeEvent::BytesReplace: {
                auto event = static_cast<AceTreeBytesEvent *>(e);
                auto op = new BytesReplaceOp();
                op->parent = event->parent()->index();
                op->index = event->index();
                op->oldBytes = event->oldBytes();
                op->newBytes = event->bytes();
                res = op;
                break;
            }
            case AceTreeEvent::BytesInsert:
            case AceTreeEvent::BytesRemove: {
                auto event = static_cast<AceTreeBytesEvent *>(e);
                auto op = new BytesInsertRemoveOp(e->type() == AceTreeEvent::BytesInsert);
                op->parent = event->parent()->index();
                op->index = event->index();
                op->bytes = event->bytes();
                res = op;
                break;
            }
            case AceTreeEvent::RowsInsert: {
                auto event = static_cast<AceTreeRowsInsDelEvent *>(e);
                auto op = new RowsInsertOp();
                op->parent = event->parent()->index();
                op->index = event->index();

                const auto &children = event->children();
                for (const auto &child : qAsConst(children)) {
                    op->children.append(AceTreeItemPrivate::get(child)->clone_helper(false));
                }
                res = op;
                break;
            }
            case AceTreeEvent::RowsMove: {
                auto event = static_cast<AceTreeRowsMoveEvent *>(e);
                auto op = new RowsMoveOp();
                op->parent = event->parent()->index();
                op->index = event->index();
                op->count = event->count();
                op->dest = event->destination();
                res = op;
                break;
            }
            case AceTreeEvent::RowsRemove: {
                auto event = static_cast<AceTreeRowsInsDelEvent *>(e);
                auto op = new RowsRemoveOp();
                op->parent = event->parent()->index();
                op->index = event->index();
                const auto &children = event->children();
                for (const auto &child : qAsConst(children)) {
                    op->children.append(child->index());
                }
                res = op;
                break;
            };
            case AceTreeEvent::RecordAdd: {
                auto event = static_cast<AceTreeRecordEvent *>(e);
                auto op = new RecordAddOp();
                op->parent = event->parent()->index();
                op->seq = event->sequence();
                op->child = AceTreeItemPrivate::get(event->child())->clone_helper(false);
                res = op;
                break;
            }
            case AceTreeEvent::RecordRemove: {
                auto event = static_cast<AceTreeRecordEvent *>(e);
                auto op = new RecordRemoveOp();
                op->parent = event->parent()->index();
                op->seq = event->sequence();
                op->child = event->child()->index();
                res = op;
                break;
            }
            case AceTreeEvent::ElementAdd: {
                auto event = static_cast<AceTreeElementEvent *>(e);
                auto op = new ElementAddOp();
                op->parent = event->parent()->index();
                op->key = event->key();
                op->child = AceTreeItemPrivate::get(event->child())->clone_helper(false);
                res = op;
                break;
            }
            case AceTreeEvent::ElementRemove: {
                auto event = static_cast<AceTreeElementEvent *>(e);
                auto op = new ElementRemoveOp();
                op->parent = event->parent()->index();
                op->key = event->key();
                op->child = event->child()->index();
                res = op;
                break;
            }
            case AceTreeEvent::RootChange: {
                auto event = static_cast<AceTreeRootEvent *>(e);
                auto op = new RootChangeOp();
                op->oldRoot = AceTreeItemPrivate::getId(event->oldRoot());
                op->newRoot = event->root()
                                  ? AceTreeItemPrivate::get(event->root())->clone_helper(false)
                                  : nullptr;
                res = op;
                break;
            }
            default:
                break;
        }
        return res;
    }

    AceTreeEvent *fromOp(BaseOp *baseOp, AceTreeModel *model, bool brief) {
        AceTreeEvent *res = nullptr;
        switch (baseOp->c) {
            case PropertyChange: {
                auto op = static_cast<PropertyChangeOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;
                auto e = new AceTreeValueEvent(AceTreeEvent::PropertyChange, item, op->key,
                                               op->newValue, op->oldValue);
                res = e;
                break;
            }
            case BytesReplace: {
                auto op = static_cast<BytesReplaceOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;
                auto e = new AceTreeBytesEvent(AceTreeEvent::BytesReplace, item, op->index,
                                               op->newBytes, op->oldBytes);
                res = e;
                break;
            }
            case BytesInsert:
            case BytesRemove: {
                auto op = static_cast<BytesInsertRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;
                auto e = new AceTreeBytesEvent(op->c == BytesInsert ? AceTreeEvent::BytesInsert
                                                                    : AceTreeEvent::BytesRemove,
                                               item, op->index, op->bytes);
                res = e;
                break;
            }
            case RowsInsert: {
                auto op = static_cast<RowsInsertOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                QVector<AceTreeItem *> children;
                if (brief) {
                    // Use Brief
                    children.reserve(op->children.size());
                    for (const auto &id : qAsConst(op->childrenIds)) {
                        auto child = model->itemFromIndex(id);
                        if (!child)
                            return nullptr;
                        children.append(child);
                    }
                } else {
                    children = op->children;

                    // Add children
                    for (const auto &child : qAsConst(op->children)) {
                        AceTreeModelPrivate::get(model)->propagate_model(child);
                    }

                    // Remove ownership
                    op->children.clear();
                }

                auto e =
                    new AceTreeRowsInsDelEvent(AceTreeEvent::RowsInsert, item, op->index, children);
                res = e;

                break;
            }
            case RowsMove: {
                auto op = static_cast<RowsMoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;
                auto e = new AceTreeRowsMoveEvent(AceTreeEvent::RowsMove, item, op->index,
                                                  op->count, op->dest);
                res = e;
                break;
            }
            case RowsRemove: {
                auto op = static_cast<RowsRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                // Search children
                QVector<AceTreeItem *> children;
                children.reserve(op->children.size());
                for (const auto &id : qAsConst(op->children)) {
                    auto child = model->itemFromIndex(id);
                    if (!child)
                        return nullptr;
                    children.append(child);
                }

                auto e =
                    new AceTreeRowsInsDelEvent(AceTreeEvent::RowsRemove, item, op->index, children);
                res = e;
                break;
            }
            case RecordAdd: {
                auto op = static_cast<RecordAddOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                AceTreeItem *child;
                if (brief) {
                    // Use brief
                    child = model->itemFromIndex(op->childId);
                    if (!child)
                        return nullptr;
                } else {
                    // Add child
                    AceTreeModelPrivate::get(model)->propagate_model(op->child);
                    child = op->child;

                    // Remove ownership
                    op->child = nullptr;
                }

                auto e = new AceTreeRecordEvent(AceTreeEvent::RecordAdd, item, op->seq, child);
                res = e;
                break;
            }
            case RecordRemove: {
                auto op = static_cast<RecordRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                // Search children
                auto child = model->itemFromIndex(op->child);
                if (!child)
                    return nullptr;

                auto e = new AceTreeRecordEvent(AceTreeEvent::RowsRemove, item, op->seq, child);
                res = e;
                break;
            }
            case ElementAdd: {
                auto op = static_cast<ElementAddOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                AceTreeItem *child;
                if (brief) {
                    // Use brief
                    child = model->itemFromIndex(op->childId);
                    if (!child)
                        return nullptr;
                } else {
                    // Add child
                    AceTreeModelPrivate::get(model)->propagate_model(op->child);
                    child = op->child;

                    // Remove ownership
                    op->child = nullptr;
                }

                auto e = new AceTreeElementEvent(AceTreeEvent::ElementAdd, item, op->key, child);
                res = e;

                break;
            }
            case ElementRemove: {
                auto op = static_cast<ElementRemoveOp *>(baseOp);
                auto item = model->itemFromIndex(op->parent);
                if (!item)
                    return nullptr;

                // Search children
                auto child = model->itemFromIndex(op->child);
                if (!child)
                    return nullptr;

                auto e = new AceTreeElementEvent(AceTreeEvent::ElementRemove, item, op->key, child);
                res = e;
                break;
            }
            case RootChange: {
                auto op = static_cast<RootChangeOp *>(baseOp);
                AceTreeItem *oldRoot = nullptr;
                if (op->oldRoot != 0) {
                    oldRoot = model->itemFromIndex(op->oldRoot);
                    if (!oldRoot)
                        return nullptr;
                }

                AceTreeItem *newRoot;
                if (brief) {
                    // Use brief
                    if (op->newRootId == 0) {
                        newRoot = nullptr;
                    } else {
                        newRoot = model->itemFromIndex(op->newRootId);
                        if (!newRoot)
                            return nullptr;
                    }
                } else {
                    // Add child
                    if (op->newRoot) {
                        AceTreeModelPrivate::get(model)->propagate_model(op->newRoot);
                    }
                    newRoot = op->newRoot;

                    // Remove ownership
                    op->newRoot = nullptr;
                }

                auto e = new AceTreeRootEvent(AceTreeEvent::RootChange, newRoot, oldRoot);
                res = e;
                break;
            }
        }
        return res;
    }

} // namespace Operations
