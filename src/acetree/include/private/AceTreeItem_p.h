#ifndef ACETREEITEM_P_H
#define ACETREEITEM_P_H

#include <set>

#include "AceTreeEvent.h"
#include "AceTreeItem.h"

class AceTreeEntity;

class AceTreeItemPrivate {
    Q_DECLARE_PUBLIC(AceTreeItem)
public:
    AceTreeItemPrivate();
    ~AceTreeItemPrivate();

    void init();

    AceTreeItem *q_ptr;
    bool is_clearing;

    AceTreeItem::Status status;
    bool m_managed;
    bool allowDelete;

    QHash<QString, QVariant> dynamicData;

    AceTreeItem *parent;
    AceTreeModel *model;

    size_t m_index;
    QHash<QString, QVariant> properties;
    QByteArray byteArray;
    QVector<AceTreeItem *> vector;
    QHash<int, AceTreeItem *> records;
    QHash<QString, AceTreeItem *> set;

    std::set<int> recordIds; // To preserve max seq
    QHash<AceTreeItem *, int> recordIndexes;
    QHash<AceTreeItem *, QString> setIndexes;

    // For AceTreeEntity cache
    AceTreeEntity *entity;

    bool testModifiable(const char *func) const;
    bool testInsertable(const char *func, const AceTreeItem *item) const;

    void sendEvent(AceTreeEvent *event);
    void changeManaged(bool managed);

    void setProperty_helper(const QString &key, const QVariant &value);
    void replaceBytes_helper(int index, const QByteArray &bytes);
    void insertBytes_helper(int index, const QByteArray &bytes);
    void removeBytes_helper(int index, int size);
    void insertRows_helper(int index, const QVector<AceTreeItem *> &items);
    void moveRows_helper(int index, int count, int dest);
    void removeRows_helper(int index, int count);
    void addRecord_helper(int seq, AceTreeItem *item);
    void removeRecord_helper(int seq);
    void addElement_helper(const QString &key, AceTreeItem *item);
    void removeElement_helper(const QString &key);

public:
    static AceTreeItem *read_helper(QDataStream &in, bool user);
    void write_helper(QDataStream &out, bool user) const;
    AceTreeItem *clone_helper(bool user) const;

    static inline AceTreeItemPrivate *get(AceTreeItem *item) {
        return item->d_func();
    }

    static inline const AceTreeItemPrivate *get(const AceTreeItem *item) {
        return item->d_func();
    }

    static inline size_t getId(const AceTreeItem *item) {
        return item ? item->index() : 0;
    }

    static void propagate(AceTreeItem *item, const std::function<void(AceTreeItem *)> &func);

    static void forceDeleteItem(AceTreeItem *item);

    static inline bool executeEvent(AceTreeEvent *event, bool undo) {
        return event->execute(undo);
    }
    
    static inline void cleanEvent(AceTreeEvent *event) {
        event->clean();
    }
};

namespace AceTreePrivate {

    QDataStream &operator>>(QDataStream &in, QString &s);
    QDataStream &operator<<(QDataStream &out, const QString &s);

    QDataStream &operator>>(QDataStream &stream, QVariantHash &s);
    QDataStream &operator<<(QDataStream &stream, const QVariantHash &s);

} // namespace AceTreePrivate

#endif // ACETREEITEM_P_H
