#ifndef ACETREEEVENT_H
#define ACETREEEVENT_H

#include <QObject>
#include <QVector>

#include "AceTreeItem.h"

class ACETREE_EXPORT AceTreeEvent {
    Q_GADGET
    Q_DISABLE_COPY_MOVE(AceTreeEvent)
public:
    enum Type {
        DynamicDataChange = 0x1,
        PropertyChange,
        BytesReplace,
        BytesInsert,
        BytesRemove,
        RowsInsert,
        RowsAboutToMove,
        RowsMove,
        RowsAboutToRemove,
        RowsRemove,
        RecordAdd,
        RecordAboutToRemove,
        RecordRemove,
        ElementAdd,
        ElementAboutToRemove,
        ElementRemove,
        RootAboutToChange,
        RootChange,
    };
    Q_ENUM(Type)

    static bool isChangeType(Type type);

public:
    explicit AceTreeEvent(Type type);
    virtual ~AceTreeEvent();

    inline Type type() const;

    virtual AceTreeEvent *clone() const = 0;

protected:
    Type t;

    virtual bool execute(bool undo) = 0;
    virtual void clean();

    friend class AceTreeModel;
    friend class AceTreeModelPrivate;
    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
};

inline AceTreeEvent::Type AceTreeEvent::type() const {
    return t;
}

class ACETREE_EXPORT AceTreeItemEvent : public AceTreeEvent {
public:
    AceTreeItemEvent(Type type, AceTreeItem *item);
    ~AceTreeItemEvent();

    inline AceTreeItem *parent() const;

protected:
    AceTreeItem *m_item;
};

inline AceTreeItem *AceTreeItemEvent::parent() const {
    return m_item;
}

class ACETREE_EXPORT AceTreeValueEvent : public AceTreeItemEvent {
public:
    AceTreeValueEvent(Type type, AceTreeItem *item, const QString &key, const QVariant &value,
                      const QVariant &oldValue);
    ~AceTreeValueEvent();

    AceTreeEvent *clone() const override;

public:
    inline QString key() const;
    inline QVariant value() const;
    inline QVariant oldValue() const;

protected:
    QString k;
    QVariant v, oldv;

    bool execute(bool undo) override;
};

inline QString AceTreeValueEvent::key() const {
    return k;
}

inline QVariant AceTreeValueEvent::value() const {
    return v;
}

inline QVariant AceTreeValueEvent::oldValue() const {
    return oldv;
}

class ACETREE_EXPORT AceTreeBytesEvent : public AceTreeItemEvent {
public:
    AceTreeBytesEvent(Type type, AceTreeItem *item, int index, const QByteArray &bytes,
                      const QByteArray &oldBytes = {});
    ~AceTreeBytesEvent();

    AceTreeEvent *clone() const override;

public:
    inline int index() const;
    inline QByteArray bytes() const;
    inline QByteArray oldBytes() const;

protected:
    int m_index;
    QByteArray b, oldb;

    bool execute(bool undo) override;
};

inline int AceTreeBytesEvent::index() const {
    return m_index;
}

inline QByteArray AceTreeBytesEvent::bytes() const {
    return b;
}

inline QByteArray AceTreeBytesEvent::oldBytes() const {
    return oldb;
}

class ACETREE_EXPORT AceTreeRowsEvent : public AceTreeItemEvent {
public:
    AceTreeRowsEvent(Type type, AceTreeItem *item, int index);
    ~AceTreeRowsEvent();

public:
    inline int index() const;

public:
    int m_index;
};

inline int AceTreeRowsEvent::index() const {
    return m_index;
}

class ACETREE_EXPORT AceTreeRowsMoveEvent : public AceTreeRowsEvent {
public:
    AceTreeRowsMoveEvent(Type type, AceTreeItem *item, int index, int count, int dest);
    ~AceTreeRowsMoveEvent();

    AceTreeEvent *clone() const override;

public:
    inline int count() const;
    inline int destination() const;

protected:
    int cnt, dest;

    bool execute(bool undo) override;
};

inline int AceTreeRowsMoveEvent::count() const {
    return cnt;
}

inline int AceTreeRowsMoveEvent::destination() const {
    return dest;
}

class ACETREE_EXPORT AceTreeRowsInsDelEvent : public AceTreeRowsEvent {
public:
    AceTreeRowsInsDelEvent(Type type, AceTreeItem *item, int index,
                           const QVector<AceTreeItem *> &children);
    ~AceTreeRowsInsDelEvent();

    AceTreeEvent *clone() const override;

public:
    inline QVector<AceTreeItem *> children() const;

protected:
    QVector<AceTreeItem *> m_children;

    bool execute(bool undo) override;
    void clean() override;
};

inline QVector<AceTreeItem *> AceTreeRowsInsDelEvent::children() const {
    return m_children;
}

class ACETREE_EXPORT AceTreeRecordEvent : public AceTreeItemEvent {
public:
    AceTreeRecordEvent(Type type, AceTreeItem *item, int seq, AceTreeItem *child);
    ~AceTreeRecordEvent();

    AceTreeEvent *clone() const override;

public:
    inline int sequence() const;
    inline AceTreeItem *child() const;

protected:
    int s;
    AceTreeItem *m_child;

    bool execute(bool undo) override;
    void clean() override;
};

inline int AceTreeRecordEvent::sequence() const {
    return s;
}

inline AceTreeItem *AceTreeRecordEvent::child() const {
    return m_child;
}

class ACETREE_EXPORT AceTreeElementEvent : public AceTreeItemEvent {
public:
    AceTreeElementEvent(Type type, AceTreeItem *item, const QString &key, AceTreeItem *child);
    ~AceTreeElementEvent();

    AceTreeEvent *clone() const override;

public:
    inline QString key() const;
    inline AceTreeItem *child() const;

protected:
    QString k;
    AceTreeItem *m_child;

    bool execute(bool undo) override;
    void clean() override;
};

inline QString AceTreeElementEvent::key() const {
    return k;
}

inline AceTreeItem *AceTreeElementEvent::child() const {
    return m_child;
}

class ACETREE_EXPORT AceTreeRootEvent : public AceTreeEvent {
public:
    AceTreeRootEvent(Type type, AceTreeItem *root, AceTreeItem *oldRoot);
    ~AceTreeRootEvent();

    AceTreeEvent *clone() const override;

public:
    inline AceTreeItem *root() const;
    inline AceTreeItem *oldRoot() const;

protected:
    AceTreeItem *r, *oldr;

    bool execute(bool undo) override;
    void clean() override;
};

inline AceTreeItem *AceTreeRootEvent::root() const {
    return r;
}

inline AceTreeItem *AceTreeRootEvent::oldRoot() const {
    return oldr;
}

#endif // ACETREEEVENT_H
