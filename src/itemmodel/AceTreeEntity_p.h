#ifndef ACETREEENTITYPRIVATE_H
#define ACETREEENTITYPRIVATE_H

#include "AceTreeEntity.h"

#include "QMChronSet.h"

class AceTreeEntityPrivate : public AceTreeItemSubscriber {
    Q_DECLARE_PUBLIC(AceTreeEntity)
public:
    AceTreeEntityPrivate();
    virtual ~AceTreeEntityPrivate();

    void init();

    AceTreeEntity *q_ptr;

    AceTreeItem *m_treeItem;

    QSet<AceTreeEntity *> children;
    AceTreeEntity *parent;

    AceTreeEntityExtra *extra;

    void event(AceTreeEvent *event) override;

    virtual void init_deferred();

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

    AceTreeEntityExtra *q_ptr;

    AceTreeEntity *entity;
};

#endif // ACETREEENTITYPRIVATE_H
