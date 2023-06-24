#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <QObject>

#include "AceTreeEvent.h"

namespace Operations {

    Q_NAMESPACE

    enum Change {
        PropertyChange = 0xC1,
        BytesReplace,
        BytesInsert,
        BytesRemove,
        RowsInsert,
        RowsMove,
        RowsRemove,
        RecordAdd,
        RecordRemove,
        ElementAdd,
        ElementRemove,
        RootChange,
    };
    Q_ENUM_NS(Change)

    struct BaseOp {
        explicit BaseOp(Change c) : c(c) {
        }
        virtual ~BaseOp() {
        }

        virtual bool read(QDataStream &in) = 0;
        virtual bool write(QDataStream &out) const = 0;

        Change c;
    };

    struct PropertyChangeOp : public BaseOp {
        PropertyChangeOp() : BaseOp(PropertyChange), parent(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        QString key;
        QVariant oldValue;
        QVariant newValue;
    };

    struct BytesReplaceOp : public BaseOp {
        BytesReplaceOp() : BaseOp(BytesReplace), parent(0), index(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int index;
        QByteArray oldBytes;
        QByteArray newBytes;
    };

    struct BytesInsertRemoveOp : public BaseOp {
        BytesInsertRemoveOp(bool isInsert = false)
            : BaseOp(isInsert ? BytesInsert : BytesRemove), parent(0), index(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int index;
        QByteArray bytes;
    };

    struct RowsInsertOp : public BaseOp {
        RowsInsertOp() : BaseOp(RowsInsert), parent(0), index(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int index;
        QVector<AceTreeItem *> children;
    };

    struct RowsRemoveOp : public BaseOp {
        RowsRemoveOp() : BaseOp(RowsRemove), parent(0), index(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int index;
        QVector<size_t> children;
    };

    struct RowsMoveOp : public BaseOp {
        RowsMoveOp() : BaseOp(RowsMove), parent(0), index(0), count(0), dest(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int index;
        int count;
        int dest;
    };

    struct RecordAddOp : public BaseOp {
        RecordAddOp() : BaseOp(RecordAdd), parent(0), seq(-1), child(nullptr) {
        }
        ~RecordAddOp() {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int seq;
        AceTreeItem *child;
    };

    struct RecordRemoveOp : public BaseOp {
        RecordRemoveOp() : BaseOp(RecordRemove), parent(0), seq(-1), child(0) {
        }
        ~RecordRemoveOp() {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int seq;
        size_t child;
    };

    struct ElementAddOp : public BaseOp {
        ElementAddOp() : BaseOp(ElementAdd), parent(0), child(nullptr) {
        }
        ~ElementAddOp() {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        QString key;
        AceTreeItem *child;
    };

    struct ElementRemoveOp : public BaseOp {
        ElementRemoveOp() : BaseOp(ElementRemove), parent(0), child(0) {
        }
        ~ElementRemoveOp() {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        QString key;
        size_t child;
    };

    struct RootChangeOp : public BaseOp {
        RootChangeOp() : BaseOp(RootChange), oldRoot(0), newRoot(nullptr) {
        }
        ~RootChangeOp() {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t oldRoot;
        AceTreeItem *newRoot;
    };

    BaseOp *toOp(AceTreeEvent *e);
    AceTreeEvent *fromOp(BaseOp *baseOp, QHash<size_t, AceTreeItem *> &hash);

} // namespace Operations

#endif // OPERATIONS_H
