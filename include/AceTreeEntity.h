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

protected:
    virtual void doInitialize() = 0;
    virtual void doSetup() = 0;

    virtual void childAdded(AceTreeEntity *child);
    virtual void childAboutToRemove(AceTreeEntity *child);

    virtual AceTreeEntityExtra *createExtra() const;

    AceTreeEntity(AceTreeEntityPrivate &d, QObject *parent = nullptr);
    QScopedPointer<AceTreeEntityPrivate> d_ptr;
};

class AceTreeEntityExtraPrivate;

class ACETREE_EXPORT AceTreeEntityExtra {
    Q_DECLARE_PRIVATE(AceTreeEntityExtra)
public:
    explicit AceTreeEntityExtra();
    virtual ~AceTreeEntityExtra();

    virtual void setup(AceTreeEntity *entity);
    virtual void event(AceTreeEvent *event);

protected:
    AceTreeEntityExtra(AceTreeEntityExtraPrivate &d);

    QScopedPointer<AceTreeEntityExtraPrivate> d_ptr;

    friend class AceTreeEntity;
    friend class AceTreeEntityPrivate;
};

#endif // ACETREEENTITY_H
