#ifndef CHORUSKIT_ACETREESTANDARDENTITY_P_H
#define CHORUSKIT_ACETREESTANDARDENTITY_P_H

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaMethod>

#include "AceTreeEntity_p.h"
#include "AceTreeStandardEntity.h"

class AceTreeStandardEntityPrivate : public AceTreeEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeStandardEntity)
public:
    explicit AceTreeStandardEntityPrivate(AceTreeEntityExtra *extra,
                                          AceTreeStandardEntity::Type type);
    ~AceTreeStandardEntityPrivate();

    AceTreeStandardEntity::Type type;
    bool m_external;

    QHash<QString, std::function<void(AceTreeEntity *)>> childPostAssignRefs;

    typedef QString (AceTreeStandardSchema::*GetTypeKey)() const;
    typedef AceTreeEntityBuilder (AceTreeStandardSchema::*GetDefaultBuilder)() const;
    typedef AceTreeEntityBuilder (AceTreeStandardSchema::*GetBuilder)(const QString &) const;

    bool testModifiable(const char *func) const;
    bool testInsertable(const char *func, const AceTreeEntity *entity) const;

    void event(AceTreeEvent *event) override;

    bool readVector_helper(const AceTreeStandardSchema &schema, GetTypeKey getTypeKey,
                           GetDefaultBuilder getDefaultBuilder, GetBuilder getBuilder,
                           const QJsonValue &value,
                           QVector<QPair<QString, AceTreeEntity *>> &childrenToAdd);
    bool readSet_helper(const AceTreeStandardSchema &schema, const QJsonValue &value);

    QJsonArray writeVector_helper(const AceTreeStandardSchema &schema,
                                  const QVector<AceTreeItem *> &childItems) const;
    QJsonObject writeSet_helper(const AceTreeStandardSchema &schema) const;

    void setupVector_helper(const AceTreeStandardSchema &schema, GetTypeKey getTypeKey,
                            GetDefaultBuilder getDefaultBuilder, GetBuilder getBuilder,
                            const QVector<AceTreeItem *> &items,
                            QVector<AceTreeEntity *> &childrenToAdd);

    void addElement_assign(AceTreeEntity *child);
    void removeElement_assigns(AceTreeEntity *child);

public:
    inline static AceTreeStandardEntityPrivate *get(AceTreeStandardEntity *entity) {
        return entity->d_func();
    }

    inline static const AceTreeStandardEntityPrivate *get(const AceTreeStandardEntity *entity) {
        return entity->d_func();
    }
};

class AceTreeStandardSchemaData : public QSharedData {
public:
    QHash<QString, QJsonValue> dynamicData;
    QHash<QString, QJsonValue> properties;
    QString rowTypeKey;
    QHash<QString, AceTreeEntityBuilder> rowBuilders;
    QHash<const QMetaObject *, QSet<QString>> rowBuilderIndexes;
    AceTreeEntityBuilder rowDefaultBuilder;

    QString recordTypeKey;
    QHash<QString, AceTreeEntityBuilder> recordBuilders;
    QHash<const QMetaObject *, QSet<QString>> recordBuilderIndexes;
    AceTreeEntityBuilder recordDefaultBuilder;

    QHash<QString, AceTreeEntityBuilder> elementBuilders;
    QHash<const QMetaObject *, QSet<QString>> elementBuilderIndexes;

    AceTreeStandardSchemaData() : rowDefaultBuilder(nullptr), recordDefaultBuilder(nullptr) {
    }
};

class ACETREE_EXPORT AceTreeEntityVectorPrivate : public AceTreeStandardEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeEntityVector)
public:
    explicit AceTreeEntityVectorPrivate(AceTreeEntityExtra *extra);
    ~AceTreeEntityVectorPrivate();

    void deferred_init() override;

    struct Signals {
        QMetaMethod inserted;
        QMetaMethod aboutToMove;
        QMetaMethod moved;
        QMetaMethod aboutToRemove;
        QMetaMethod removed;
    };

    Signals *_signals;

    void event(AceTreeEvent *event) override;

public:
    inline static AceTreeEntityVectorPrivate *get(AceTreeEntityVector *entity) {
        return entity->d_func();
    }

    inline static const AceTreeEntityVectorPrivate *get(const AceTreeEntityVector *entity) {
        return entity->d_func();
    }
};

class ACETREE_EXPORT AceTreeEntityRecordTablePrivate : public AceTreeStandardEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeEntityRecordTable)
public:
    explicit AceTreeEntityRecordTablePrivate(AceTreeEntityExtra *extra);
    ~AceTreeEntityRecordTablePrivate();

    void deferred_init() override;

    struct Signals {
        QMetaMethod inserted;
        QMetaMethod aboutToRemove;
        QMetaMethod removed;
    };

    Signals *_signals;

    void event(AceTreeEvent *event) override;

public:
    inline static AceTreeEntityRecordTablePrivate *get(AceTreeEntityRecordTable *entity) {
        return entity->d_func();
    }

    inline static const AceTreeEntityRecordTablePrivate *
        get(const AceTreeEntityRecordTable *entity) {
        return entity->d_func();
    }
};

class AceTreeEntityMappingPrivate : public AceTreeStandardEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeEntityMapping)
public:
    explicit AceTreeEntityMappingPrivate(AceTreeEntityExtra *extra);
    ~AceTreeEntityMappingPrivate();

public:
    // Nothing now

public:
    inline static AceTreeEntityMappingPrivate *get(AceTreeEntityMapping *entity) {
        return entity->d_func();
    }

    inline static const AceTreeEntityMappingPrivate *get(const AceTreeEntityMapping *entity) {
        return entity->d_func();
    }
};

class AceTreeEntityMappingExtraPrivate : public AceTreeEntityNotifyExtraPrivate {
public:
    // Nothing
};

#endif // CHORUSKIT_ACETREESTANDARDENTITY_P_H
