#ifndef ACETREEENTITYPRIVATE_H
#define ACETREEENTITYPRIVATE_H

#include "AceTreeEntity.h"

class AceTreeEntityPrivate {
    Q_DECLARE_PUBLIC(AceTreeEntity)
public:
    AceTreeEntityPrivate(AceTreeEntityExtra *extra);
    virtual ~AceTreeEntityPrivate();

    void init();

    AceTreeEntityExtra *extra;
    AceTreeEntity *q_ptr;

    AceTreeItem *m_treeItem;

    QSet<AceTreeEntity *> children;
    AceTreeEntity *parent;

    virtual void event(AceTreeEvent *event);

    virtual void deferred_init();

    inline static AceTreeItem *getItem(AceTreeEntity *entity) {
        return entity->d_func()->m_treeItem;
    }

    inline static const AceTreeItem *getItem(const AceTreeEntity *entity) {
        return entity->d_func()->m_treeItem;
    }

    inline static AceTreeEntityPrivate *get(AceTreeEntity *entity) {
        return entity->d_func();
    }

    inline static const AceTreeEntityPrivate *get(const AceTreeEntity *entity) {
        return entity->d_func();
    }
};

class AceTreeEntityExtraPrivate {
public:
    AceTreeEntityExtraPrivate();
    virtual ~AceTreeEntityExtraPrivate();

    void init();

    AceTreeEntity *entity;
};

class AceTreeEntityNotifyExtraPrivate : public AceTreeEntityExtraPrivate {
public:
    QHash<QString, AceTreeEntityNotifyExtra::Notifier> dynamicPropertyNotifiers;
    QHash<QString, AceTreeEntityNotifyExtra::Notifier> propertyNotifiers;
};

#endif // ACETREEENTITYPRIVATE_H
