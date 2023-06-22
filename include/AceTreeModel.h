#ifndef ACETREEMODEL_H
#define ACETREEMODEL_H

#include <QIODevice>
#include <QObject>
#include <QVariant>

#include "AceTreeRecoverable.h"

class AceTreeTransaction;

class AceTreeModel;

class AceTreeItemSubscriber;

class AceTreeItemPrivate;

class ACETREE_EXPORT AceTreeItem {
    Q_DECLARE_PRIVATE(AceTreeItem)
public:
    explicit AceTreeItem(const QString &name);
    virtual ~AceTreeItem();

    enum Status : qint8 {
        Root,        // Set as root or newly created
        Row,         // Added to vector
        Record,      // Added to record table
        Node,        // Added to set
        ManagedRoot, // Removed from an item on a model
    };

    inline bool isRoot() const;
    inline bool isRow() const;
    inline bool isRecord() const;
    inline bool isNode() const;
    inline bool isManagedRoot() const;

    QString name() const;
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
    int index() const;

    bool isFree() const;
    bool isObsolete() const;
    bool isWritable() const;

    // Properties
    QVariant property(const QString &key) const;
    bool setProperty(const QString &key, const QVariant &value);
    inline bool clearProperty(const QString &key);
    QStringList propertyKeys() const;
    QVariantHash propertyMap() const;
    inline QVariantHash properties() const;

    inline QVariant attribute(const QString &key) const;
    inline bool setAttribute(const QString &key, const QVariant &value);
    inline bool clearAttribute(const QString &key);
    inline QStringList attributeKeys() const;
    inline QVariantHash attributeMap() const;
    inline QVariantHash attributes() const;

    // ByteArray
    bool setBytes(int start, const QByteArray &bytes);
    inline bool prependBytes(const QByteArray &bytes);
    inline bool appendBytes(const QByteArray &bytes);
    bool insertBytes(int start, const QByteArray &bytes);
    bool removeBytes(int start, int size);
    inline bool truncateBytes(int size);
    inline bool clearBytes();
    QByteArray bytes() const;
    const char *bytesData() const;
    QByteArray midBytes(int start, int len) const;
    int bytesIndexOf(const QByteArray &bytes, int start) const;
    int bytesSize() const;

    // Vector - Rows
    inline bool prependRow(AceTreeItem *item);
    inline bool prependRows(const QVector<AceTreeItem *> &items);
    inline bool appendRow(AceTreeItem *item);
    inline bool appendRows(const QVector<AceTreeItem *> &items);
    inline bool insertRow(int index, AceTreeItem *item);
    inline bool removeRow(int index);
    bool insertRows(int index, const QVector<AceTreeItem *> &items);
    bool moveRows(int index, int count, int dest);         // `dest`: destination index before move
    inline bool moveRows2(int index, int count, int dest); // `dest`: destination index after move
    bool removeRows(int index, int count);
    bool removeRow(AceTreeItem *item);
    AceTreeItem *row(int index) const;
    QVector<AceTreeItem *> rows() const;
    int rowIndexOf(AceTreeItem *item) const;
    int rowCount() const;

    // Sheet - Records
    int addRecord(AceTreeItem *item);
    bool removeRecord(int seq);
    bool removeRecord(AceTreeItem *item);
    AceTreeItem *record(int seq);
    int recordIndexOf(AceTreeItem *item) const;
    QList<int> records() const;
    int recordCount() const;
    int maxRecordSeq() const;

    // Set - Nodes
    inline bool insertNode(AceTreeItem *item);
    bool addNode(AceTreeItem *item);
    bool removeNode(AceTreeItem *item);
    bool containsNode(AceTreeItem *item);
    QList<AceTreeItem *> nodes() const;
    int nodeCount() const;
    QStringList nodeNames() const;
    AceTreeItem *nameToNode(const QString &name) const;
    QList<AceTreeItem *> nameToNodes(const QString &name) const;

public:
    static AceTreeItem *read(QDataStream &in);
    void write(QDataStream &out) const;
    AceTreeItem *clone(const QString &newName = {}) const;

protected:
    void propagateModel(AceTreeModel *model);

    AceTreeItem(AceTreeItemPrivate &d, const QString &name);

    QScopedPointer<AceTreeItemPrivate> d_ptr;

    friend class AceTreeModel;
    friend class AceTreeModelPrivate;
    friend class AceTreeTransaction;
};

class AceTreeModelPrivate;

class ACETREE_EXPORT AceTreeModel : public AceTreeRecoverable {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeModel)
public:
    explicit AceTreeModel(QObject *parent = nullptr);
    ~AceTreeModel();

public:
    int steps() const;
    int currentStep() const;
    void setCurrentStep(int step);
    void truncateForwardSteps();
    inline void nextStep();
    inline void previousStep();

    bool isWritable() const;

    bool recover(const QByteArray &data) override;

    AceTreeItem *itemFromIndex(int index) const;

    AceTreeItem *rootItem() const;
    void setRootItem(AceTreeItem *item);

    AceTreeItem *reset();

signals:
    void dynamicDataChanged(AceTreeItem *item, const QString &key, const QVariant &newValue, const QVariant &oldValue);
    void propertyChanged(AceTreeItem *item, const QString &key, const QVariant &newValue, const QVariant &oldValue);

    void bytesSet(AceTreeItem *item, int start, const QByteArray &newBytes, const QByteArray &oldBytes);
    void bytesInserted(AceTreeItem *item, int start, const QByteArray &bytes);
    void bytesRemoved(AceTreeItem *item, int start, const QByteArray &bytes);

    void rowsInserted(AceTreeItem *parent, int index, const QVector<AceTreeItem *> &items);
    void rowsAboutToMove(AceTreeItem *parent, int index, int count, int dest);
    void rowsMoved(AceTreeItem *parent, int index, int count, int dest);
    void rowsAboutToRemove(AceTreeItem *parent, int index, const QVector<AceTreeItem *> &items);
    void rowsRemoved(AceTreeItem *parent, int index, const QVector<AceTreeItem *> &items);

    void recordAdded(AceTreeItem *parent, int seq, AceTreeItem *item);
    void recordAboutToRemove(AceTreeItem *parent, int seq, AceTreeItem *item);
    void recordRemoved(AceTreeItem *parent, int seq, AceTreeItem *item);

    void nodeAdded(AceTreeItem *parent, AceTreeItem *item);
    void nodeAboutToRemove(AceTreeItem *parent, AceTreeItem *item);
    void nodeRemoved(AceTreeItem *parent, AceTreeItem *item);

    void rootAboutToChange(AceTreeItem *newRoot, AceTreeItem *oldRoot);
    void rootChanged(AceTreeItem *newRoot, AceTreeItem *oldRoot);

    void stepChanged(int step);

protected:
    AceTreeModel(AceTreeModelPrivate &d, QObject *parent = nullptr);

    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
};

class AceTreeItemSubscriberPrivate;

class ACETREE_EXPORT AceTreeItemSubscriber {
    Q_DISABLE_COPY_MOVE(AceTreeItemSubscriber)
public:
    AceTreeItemSubscriber();
    virtual ~AceTreeItemSubscriber();

    AceTreeItem *treeItem() const;

    virtual void dynamicDataChanged(const QString &key, const QVariant &newValue, const QVariant &oldValue);
    virtual void propertyChanged(const QString &key, const QVariant &newValue, const QVariant &oldValue);

    virtual void bytesSet(int start, const QByteArray &newBytes, const QByteArray &oldBytes);
    virtual void bytesInserted(int start, const QByteArray &bytes);
    virtual void bytesRemoved(int start, const QByteArray &bytes);

    virtual void rowsInserted(int index, const QVector<AceTreeItem *> &items);
    virtual void rowsAboutToMove(int index, int count, int dest);
    virtual void rowsMoved(int index, int count, int dest);
    virtual void rowsAboutToRemove(int index, const QVector<AceTreeItem *> &items);
    virtual void rowsRemoved(int index, const QVector<AceTreeItem *> &items);

    virtual void recordAdded(int seq, AceTreeItem *item);
    virtual void recordAboutToRemove(int seq, AceTreeItem *item);
    virtual void recordRemoved(int seq, AceTreeItem *item);

    virtual void nodeAdded(AceTreeItem *item);
    virtual void nodeAboutToRemove(AceTreeItem *item);
    virtual void nodeRemoved(AceTreeItem *item);

private:
    AceTreeItemSubscriberPrivate *d;

    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
    friend class AceTreeModel;
    friend class AceTreeModelPrivate;
    friend class AceTreeItemSubscriberPrivate;
    friend class AceTreeTransaction;
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

inline bool AceTreeItem::isNode() const {
    return status() == Node;
}

inline bool AceTreeItem::isManagedRoot() const {
    return status() == ManagedRoot;
}

inline void AceTreeItem::clearDynamicData(const QString &key) {
    setDynamicData(key, {});
}

inline bool AceTreeItem::clearProperty(const QString &key) {
    return setProperty(key, {});
}

inline QVariantHash AceTreeItem::properties() const {
    return propertyMap();
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

inline QVariantHash AceTreeItem::attributes() const {
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

inline bool AceTreeItem::moveRows2(int index, int count, int dest) {
    return moveRows(index, count, (dest <= index) ? dest : (dest + count));
}

inline bool AceTreeItem::removeRow(int index) {
    return removeRows(index, 1);
}

inline bool AceTreeItem::insertNode(AceTreeItem *item) {
    return addNode(item);
}

inline void AceTreeModel::nextStep() {
    setCurrentStep(currentStep() + 1);
}

inline void AceTreeModel::previousStep() {
    setCurrentStep(currentStep() - 1);
}

#endif // ACETREEMODEL_H
