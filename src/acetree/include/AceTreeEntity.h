#ifndef ACETREEENTITY_H
#define ACETREEENTITY_H

#include <QJsonValue>
#include <QObject>
#include <QSharedData>

#include "AceTreeModel.h"

class AceTreeEntityExtra;

class AceTreeEntityPrivate;

class ACETREE_EXPORT AceTreeEntity : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeEntity)
public:
    explicit AceTreeEntity(QObject *parent = nullptr);
    explicit AceTreeEntity(AceTreeEntityExtra *extra, QObject *parent = nullptr);
    ~AceTreeEntity();

public:
    void initialize();
    void setup(AceTreeItem *item);

    virtual bool read(const QJsonValue &value) = 0;
    virtual QJsonValue write() const = 0;

    QList<AceTreeEntity *> childEntities() const;
    bool hasChildEntity(AceTreeEntity *child) const;
    AceTreeEntity *parentEntity() const;

    bool isFree() const;
    bool isWritable() const;

    bool addChild(AceTreeEntity *child);
    bool removeChild(AceTreeEntity *child);

    static AceTreeEntity *itemToEntity(AceTreeItem *item);

    AceTreeItem *treeItem() const;

protected:
    virtual void doInitialize() = 0;
    virtual void doSetup() = 0;

    virtual void childAdded(AceTreeEntity *child);
    virtual void childAboutToRemove(AceTreeEntity *child);

    AceTreeEntity(AceTreeEntityPrivate &d, QObject *parent = nullptr);
    QScopedPointer<AceTreeEntityPrivate> d_ptr;

protected:
    AceTreeEntityExtra *extra() const;

    virtual void itemEvent(AceTreeEvent *event);

    friend class AceTreeItem;
    friend class AceTreeItemPrivate;
};

class AceTreeEntityExtraPrivate;

class ACETREE_EXPORT AceTreeEntityExtra {
    Q_DECLARE_PRIVATE(AceTreeEntityExtra)
public:
    AceTreeEntityExtra();
    virtual ~AceTreeEntityExtra();

    AceTreeEntity *entity() const;

protected:
    virtual void setup(AceTreeEntity *entity);
    virtual void event(AceTreeEvent *event);

protected:
    QScopedPointer<AceTreeEntityExtraPrivate> d_ptr;

    AceTreeEntityExtra(AceTreeEntityExtraPrivate &d);

    friend class AceTreeEntity;
    friend class AceTreeEntityPrivate;
};

class AceTreeEntityNotifyExtraPrivate;

class ACETREE_EXPORT AceTreeEntityNotifyExtra : public AceTreeEntityExtra {
    Q_DECLARE_PRIVATE(AceTreeEntityNotifyExtra)
public:
    AceTreeEntityNotifyExtra();
    ~AceTreeEntityNotifyExtra();

public:
    using Notifier = std::function<void(const QVariant & /* value */, const QVariant & /* oldValue */)>;

    void addDynamicDataNotifier(const QString &key, const Notifier &notifier);
    void removeDynamicDataNotifier(const QString &key);

    void addPropertyNotifier(const QString &key, const Notifier &notifier);
    void removePropertyNotifier(const QString &key);

protected:
    void event(AceTreeEvent *event) override;

    AceTreeEntityNotifyExtra(AceTreeEntityNotifyExtraPrivate &d);
};

#endif // ACETREEENTITY_H
