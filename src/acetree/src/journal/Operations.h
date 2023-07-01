#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <QDebug>
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

    /*
     * Read brief mode: for insertion operation, if read backward ones,
     * it's not necessary to deserialize the items, only need to read their
     * indexes, because we have already deserialized them when reading the
     * next checkpoint.
     *
     */

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
        ~RowsInsertOp();

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        bool readBrief(QDataStream &in);

        size_t parent;
        int index;
        QVector<size_t> childrenIds;
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
        RecordAddOp() : BaseOp(RecordAdd), parent(0), seq(-1), childId(0), child(nullptr) {
        }
        ~RecordAddOp();

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        bool readBrief(QDataStream &in);

        size_t parent;
        int seq;
        size_t childId;
        AceTreeItem *child;
    };

    struct RecordRemoveOp : public BaseOp {
        RecordRemoveOp() : BaseOp(RecordRemove), parent(0), seq(-1), child(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        int seq;
        size_t child;
    };

    struct ElementAddOp : public BaseOp {
        ElementAddOp() : BaseOp(ElementAdd), parent(0), childId(0), child(nullptr) {
        }
        ~ElementAddOp();

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        bool readBrief(QDataStream &in);

        size_t parent;
        QString key;
        size_t childId;
        AceTreeItem *child;
    };

    struct ElementRemoveOp : public BaseOp {
        ElementRemoveOp() : BaseOp(ElementRemove), parent(0), child(0) {
        }

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        size_t parent;
        QString key;
        size_t child;
    };

    struct RootChangeOp : public BaseOp {
        RootChangeOp() : BaseOp(RootChange), oldRoot(0), newRootId(0), newRoot(nullptr) {
        }
        ~RootChangeOp();

        bool read(QDataStream &in) override;
        bool write(QDataStream &out) const override;

        bool readBrief(QDataStream &in);

        size_t oldRoot;
        size_t newRootId;
        AceTreeItem *newRoot;
    };

    BaseOp *toOp(AceTreeEvent *e);

    // Will move all tree items from op to model, the op can be deleted later
    AceTreeEvent *fromOp(BaseOp *baseOp, AceTreeModel *model, bool brief);

    QDebug operator<<(QDebug debug, BaseOp *baseOp);
    QDebug operator<<(QDebug debug, PropertyChangeOp *op);
    QDebug operator<<(QDebug debug, BytesReplaceOp *op);
    QDebug operator<<(QDebug debug, BytesInsertRemoveOp *op);
    QDebug operator<<(QDebug debug, RowsInsertOp *op);
    QDebug operator<<(QDebug debug, RowsMoveOp *op);
    QDebug operator<<(QDebug debug, RowsRemoveOp *op);
    QDebug operator<<(QDebug debug, RecordAddOp *op);
    QDebug operator<<(QDebug debug, RecordRemoveOp *op);
    QDebug operator<<(QDebug debug, ElementAddOp *op);
    QDebug operator<<(QDebug debug, ElementRemoveOp *op);
    QDebug operator<<(QDebug debug, RootChangeOp *op);

} // namespace Operations

#endif // OPERATIONS_H
