#include "AceTreeEvent.h"

#include "AceTreeItem_p.h"
#include "AceTreeModel_p.h"

bool AceTreeValueEvent::execute(bool undo) {
    switch (t) {
        case PropertyChange:
            AceTreeItemPrivate::get(m_item)->setProperty_helper(k, undo ? oldv : v);
            break;

        default:
            return false;
            break;
    }
    return true;
}

bool AceTreeBytesEvent::execute(bool undo) {
    switch (t) {
        case BytesReplace:
            if (undo) {
                auto d = AceTreeItemPrivate::get(m_item);
                d->replaceBytes_helper(m_index, oldb);

                // Need truncate
                int delta = b.size() - oldb.size();
                if (delta > 0) {
                    d->removeBytes_helper(d->byteArray.size() - delta, delta);
                }
            } else {
                AceTreeItemPrivate::get(m_item)->replaceBytes_helper(m_index, b);
            }
            break;

        case BytesInsert:
        case BytesRemove:
            ((t == BytesRemove) ^ undo)
                ? AceTreeItemPrivate::get(m_item)->removeBytes_helper(m_index, b.size())
                : AceTreeItemPrivate::get(m_item)->insertBytes_helper(m_index, b);
            break;

        default:
            return false;
            break;
    }
    return true;
}

bool AceTreeRowsMoveEvent::execute(bool undo) {
    switch (t) {
        case RowsMove:
            if (undo) {
                int r_index;
                int r_dest;
                if (dest > m_index) {
                    r_index = dest - cnt;
                    r_dest = m_index;
                } else {
                    r_index = dest;
                    r_dest = m_index + cnt;
                }
                AceTreeItemPrivate::get(m_item)->moveRows_helper(r_index, cnt, r_dest);
            } else
                AceTreeItemPrivate::get(m_item)->moveRows_helper(m_index, cnt, dest);
            break;
        default:
            return false;
            break;
    }
    return true;
}

bool AceTreeRowsInsDelEvent::execute(bool undo) {
    switch (t) {
        case RowsInsert:
        case RowsRemove: {
            ((t == RowsRemove) ^ undo)
                ? AceTreeItemPrivate::get(m_item)->removeRows_helper(m_index, m_children.size())
                : AceTreeItemPrivate::get(m_item)->insertRows_helper(m_index, m_children);
            break;
        }
        default:
            return false;
            break;
    }
    return true;
}

void AceTreeRowsInsDelEvent::clean() {
    if (t != RowsInsert) {
        return;
    }

    for (const auto &child : qAsConst(m_children)) {
        if (child->isManaged())
            AceTreeItemPrivate::forceDeleteItem(child);
    }
}

bool AceTreeRecordEvent::execute(bool undo) {
    switch (t) {
        case RecordAdd:
        case RecordRemove: {
            ((t == RecordRemove) ^ undo)
                ? AceTreeItemPrivate::get(m_item)->removeRecord_helper(s)
                : AceTreeItemPrivate::get(m_item)->addRecord_helper(s, m_child);
            break;
        }
        default:
            return false;
            break;
    }
    return true;
}

void AceTreeRecordEvent::clean() {
    if (t != RecordAdd) {
        return;
    }

    if (m_child->isManaged())
        AceTreeItemPrivate::forceDeleteItem(m_child);
}

bool AceTreeElementEvent::execute(bool undo) {
    switch (t) {
        case ElementAdd:
        case ElementRemove: {
            ((t == ElementRemove) ^ undo)
                ? AceTreeItemPrivate::get(m_item)->removeElement_helper(k)
                : AceTreeItemPrivate::get(m_item)->addElement_helper(k, m_child);
            break;
        }
        default:
            return false;
            break;
    }
    return true;
}

void AceTreeElementEvent::clean() {
    if (t != ElementAdd) {
        return;
    }

    if (m_child->isManaged())
        AceTreeItemPrivate::forceDeleteItem(m_child);
}

bool AceTreeRootEvent::execute(bool undo) {
    switch (t) {
        case RootChange: {
            AceTreeModelPrivate::get(r ? r->model() : oldr->model())
                ->setRootItem_helper(undo ? oldr : r);
            break;
        }
        default:
            return false;
            break;
    }
    return true;
}

void AceTreeRootEvent::clean() {
    if (r->isManaged())
        AceTreeItemPrivate::forceDeleteItem(r);
}
