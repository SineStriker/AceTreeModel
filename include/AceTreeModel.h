#ifndef ACETREEMODEL_H
#define ACETREEMODEL_H

#include <QIODevice>
#include <QVariant>

#include "AceTreeEvent.h"

class AceTreeBackend;

class AceTreeModelPrivate;

class ACETREE_EXPORT AceTreeModel : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeModel)
public:
    explicit AceTreeModel(QObject *parent = nullptr);
    AceTreeModel(AceTreeBackend *backend, QObject *parent = nullptr);
    ~AceTreeModel();

    QVariantHash modelInfo() const;
    void setModelInfo(const QVariantHash &info);

public:
    bool isWritable() const;
    AceTreeItem *itemFromIndex(size_t index) const;

    AceTreeItem *rootItem() const;
    bool setRootItem(AceTreeItem *item);

    void reset();

public:
    enum StateFlag {
        TransactionFlag = 1,
        UndoRedoFlag = 2,
        UndoFlag = 4,
        RedoFlag = 8,
    };

    enum State {
        Idle = 0,
        Transaction = TransactionFlag,
        Undo = UndoFlag | UndoRedoFlag,
        Redo = RedoFlag | UndoRedoFlag,
    };
    Q_ENUM(State)

    State state() const;
    inline bool inTransaction() const;
    inline bool stepChanging() const;

    void beginTransaction();
    void abortTransaction();
    void commitTransaction(const QHash<QString, QString> &attributes = {});

    int minStep() const;
    int maxStep() const;
    int currentStep() const;
    QHash<QString, QString> stepAttributes(int step) const;

    void nextStep();
    void previousStep();

signals:
    void modelChanged(AceTreeEvent *e);
    void stepChanged(int step);

protected:
    AceTreeModel(AceTreeModelPrivate &d, QObject *parent = nullptr);

    QScopedPointer<AceTreeModelPrivate> d_ptr;

    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
};

inline bool AceTreeModel::inTransaction() const {
    return state() == Transaction;
}

inline bool AceTreeModel::stepChanging() const {
    return state() & UndoRedoFlag;
}

#endif // ACETREEMODEL_H
