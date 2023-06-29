#ifndef ACETREEITEM_H
#define ACETREEITEM_H

#include <QVariant>

#include "AceTreeGlobal.h"

class AceTreeModel;

class AceTreeItemSubscriber;

class AceTreeItemPrivate;

class ACETREE_EXPORT AceTreeItem {
    Q_GADGET
    Q_DECLARE_PRIVATE(AceTreeItem)
public:
    AceTreeItem();
    ~AceTreeItem();

    enum Status {
        Root = 1,    // Set as root or newly created
        Row = 2,     // Added to vector
        Record = 4,  // Added to record table
        Element = 8, // Added to set
    };
    Q_ENUM(Status)

    inline bool isRoot() const;
    inline bool isRow() const;
    inline bool isRecord() const;
    inline bool isElement() const;

    Status status() const;

    bool addSubscriber(AceTreeItemSubscriber *sub);
    bool removeSubscriber(AceTreeItemSubscriber *sub);
    QList<AceTreeItemSubscriber *> subscribers() const;
    int subscriberCount() const;
    bool hasSubscriber(AceTreeItemSubscriber *sub) const;

    QVariant dynamicData(const QString &key) const;
    void setDynamicData(const QString &key, const QVariant &value);
    inline void clearDynamicData(const QString &key);
    QStringList dynamicDataKeys() const;
    QVariantHash dynamicDataMap() const;

public:
    AceTreeItem *parent() const;
    AceTreeModel *model() const;
    size_t index() const;

    bool isFree() const;
    inline bool isObsolete() const;
    bool isManaged() const;
    bool isWritable() const;

    // Properties
    QVariant property(const QString &key) const;
    bool setProperty(const QString &key, const QVariant &value);
    inline bool clearProperty(const QString &key);
    QStringList propertyKeys() const;
    QVariantHash propertyMap() const;

    inline QVariant attribute(const QString &key) const;
    inline bool setAttribute(const QString &key, const QVariant &value);
    inline bool clearAttribute(const QString &key);
    inline QStringList attributeKeys() const;
    inline QVariantHash attributeMap() const;

    // ByteArray
    inline bool prependBytes(const QByteArray &bytes);
    inline bool appendBytes(const QByteArray &bytes);
    bool insertBytes(int index, const QByteArray &bytes);
    bool removeBytes(int index, int size);
    bool replaceBytes(int index, const QByteArray &bytes);
    inline bool truncateBytes(int size);
    inline bool clearBytes();
    QByteArray bytes() const;
    const char *bytesData() const;
    QByteArray midBytes(int start, int len) const;
    int bytesIndexOf(const QByteArray &bytes, int index) const;
    int bytesSize() const;

    // Vector - Rows
    inline bool prependRow(AceTreeItem *item);
    inline bool prependRows(const QVector<AceTreeItem *> &items);
    inline bool appendRow(AceTreeItem *item);
    inline bool appendRows(const QVector<AceTreeItem *> &items);
    inline bool insertRow(int index, AceTreeItem *item);
    inline bool removeRow(int index);
    bool insertRows(int index, const QVector<AceTreeItem *> &items);
    bool moveRows(int index, int count, int dest);         // dest: destination index before move
    inline bool moveRows2(int index, int count, int dest); // dest: destination index after move
    bool removeRows(int index, int count);
    bool removeRow(AceTreeItem *item);
    AceTreeItem *row(int index) const;
    QVector<AceTreeItem *> rows() const;
    int rowIndexOf(AceTreeItem *item) const;
    int rowCount() const;

    // Record Table - Records
    inline int insertRecord(AceTreeItem *item);
    int addRecord(AceTreeItem *item);
    bool removeRecord(int seq);
    bool removeRecord(AceTreeItem *item);
    AceTreeItem *record(int seq);
    int recordSequenceOf(AceTreeItem *item) const;
    QList<int> records() const;
    QMap<int, AceTreeItem *> recordMap() const;
    int recordCount() const;
    int maxRecordSequence() const;

    // Set - Elements
    inline bool insertElement(const QString &key, AceTreeItem *item);
    bool addElement(const QString &key, AceTreeItem *item);
    bool removeElement(const QString &key);
    bool removeElement(AceTreeItem *item);
    AceTreeItem *element(const QString &key) const;
    QString elementKeyOf(AceTreeItem *item) const;
    bool containsElement(AceTreeItem *item) const;
    QStringList elementKeys() const;
    QList<AceTreeItem *> elements() const;
    QHash<QString, AceTreeItem *> elementHash() const;
    QMap<QString, AceTreeItem *> elementMap() const;
    int elementCount() const;

    static AceTreeItem *read(QDataStream &in);
    void write(QDataStream &out) const;
    AceTreeItem *clone() const;

protected:
    AceTreeItem(AceTreeItemPrivate &d);

    QScopedPointer<AceTreeItemPrivate> d_ptr;

    friend class AceTreeModel;
    friend class AceTreeModelPrivate;
};

inline bool AceTreeItem::isRoot() const {
    return status() == Root;
}

inline bool AceTreeItem::isRow() const {
    return status() == Row;
}

inline bool AceTreeItem::isRecord() const {
    return status() == Record;
}

inline bool AceTreeItem::isElement() const {
    return status() == Element;
}

inline void AceTreeItem::clearDynamicData(const QString &key) {
    setDynamicData(key, {});
}

inline bool AceTreeItem::isObsolete() const {
    return isManaged();
}

inline bool AceTreeItem::clearProperty(const QString &key) {
    return setProperty(key, {});
}

inline QVariant AceTreeItem::attribute(const QString &key) const {
    return property(key);
}

inline bool AceTreeItem::setAttribute(const QString &key, const QVariant &value) {
    return setProperty(key, value);
}

inline bool AceTreeItem::clearAttribute(const QString &key) {
    return clearProperty(key);
}

inline QStringList AceTreeItem::attributeKeys() const {
    return propertyKeys();
}

inline QVariantHash AceTreeItem::attributeMap() const {
    return propertyMap();
}

inline bool AceTreeItem::prependBytes(const QByteArray &bytes) {
    return insertBytes(0, bytes);
}

inline bool AceTreeItem::appendBytes(const QByteArray &bytes) {
    return insertBytes(bytesSize(), bytes);
}

inline bool AceTreeItem::truncateBytes(int size) {
    return removeBytes(size, bytesSize() - size);
}

inline bool AceTreeItem::clearBytes() {
    return removeBytes(0, bytesSize());
}

inline bool AceTreeItem::prependRow(AceTreeItem *item) {
    return insertRows(0, {item});
}

inline bool AceTreeItem::prependRows(const QVector<AceTreeItem *> &items) {
    return insertRows(0, items);
}

inline bool AceTreeItem::appendRow(AceTreeItem *item) {
    return insertRows(rowCount(), {item});
}

inline bool AceTreeItem::appendRows(const QVector<AceTreeItem *> &items) {
    return insertRows(rowCount(), items);
}

inline bool AceTreeItem::insertRow(int index, AceTreeItem *item) {
    return insertRows(index, {item});
}

inline bool AceTreeItem::removeRow(int index) {
    return removeRows(index, 1);
}

inline bool AceTreeItem::moveRows2(int index, int count, int dest) {
    return moveRows(index, count, (dest <= index) ? dest : (dest + count));
}

inline int AceTreeItem::insertRecord(AceTreeItem *item) {
    return addRecord(item);
}

inline bool AceTreeItem::insertElement(const QString &key, AceTreeItem *item) {
    return addElement(key, item);
}

class AceTreeEvent;

class AceTreeItemSubscriberPrivate;

class ACETREE_EXPORT AceTreeItemSubscriber {
    Q_DISABLE_COPY_MOVE(AceTreeItemSubscriber)
public:
    AceTreeItemSubscriber();
    virtual ~AceTreeItemSubscriber();

    AceTreeItem *treeItem() const;

protected:
    virtual void event(AceTreeEvent *event);

private:
    AceTreeItemSubscriberPrivate *d;

    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
};

#endif // ACETREEITEM_H
