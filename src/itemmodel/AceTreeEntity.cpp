#include "AceTreeEntity.h"
#include "AceTreeEntity_p.h"

#include "AceTreeItem_p.h"

AceTreeEntityPrivate::AceTreeEntityPrivate() {
    m_treeItem = nullptr;
    parent = nullptr;
}

AceTreeEntityPrivate::~AceTreeEntityPrivate() {
}

void AceTreeEntityPrivate::init() {
}

void AceTreeEntityPrivate::event(AceTreeEvent *event) {
    if (extra) {
        extra->event(event);
    }
}

void AceTreeEntityPrivate::init_deferred() {
    Q_Q(AceTreeEntity);
    AceTreeItemPrivate::get(m_treeItem)->entity = q;
    m_treeItem->addSubscriber(this);

    extra = q->createExtra();
    if (extra) {
        extra->d_func()->entity = q;
        extra->setup(q);
    }
}

AceTreeEntity::AceTreeEntity(QObject *parent) : AceTreeEntity(*new AceTreeEntityPrivate(), parent) {
}

AceTreeEntity::~AceTreeEntity() {
    Q_D(AceTreeEntity);
    if (d->parent)
        d->parent->removeChild(this);

    auto &treeItem = d->m_treeItem;
    if (treeItem) {
        AceTreeItemPrivate::get(treeItem)->entity = nullptr;
        if (treeItem->isFree()) {
            delete treeItem;
        }
    }

    for (const auto &child : qAsConst(d->children)) {
        if (!child->parent())
            delete child;
    }
}

void AceTreeEntity::initialize() {
    Q_D(AceTreeEntity);
    auto &treeItem = d->m_treeItem;
    if (treeItem) {
        qWarning() << "AceTreeEntity::setup(): entity has been initialized" << this;
        return;
    }

    treeItem = new AceTreeItem();
    d->init_deferred();
    doInitialize();
}

void AceTreeEntity::setup(AceTreeItem *item) {
    Q_D(AceTreeEntity);
    auto &treeItem = d->m_treeItem;
    if (treeItem) {
        qWarning() << "AceTreeEntity::setup(): entity has been setup" << this;
        return;
    }

    treeItem = item;
    d->init_deferred();
    doSetup();
}

QList<AceTreeEntity *> AceTreeEntity::childEntities() const {
    Q_D(const AceTreeEntity);
    return d->children.values();
}

bool AceTreeEntity::hasChildEntity(AceTreeEntity *child) const {
    Q_D(const AceTreeEntity);
    return d->children.contains(child);
}

AceTreeEntity *AceTreeEntity::parentEntity() const {
    Q_D(const AceTreeEntity);
    return d->parent;
}

bool AceTreeEntity::isFree() const {
    Q_D(const AceTreeEntity);
    return !d->parent && (!d->m_treeItem || d->m_treeItem->isFree());
}

bool AceTreeEntity::isWritable() const {
    Q_D(const AceTreeEntity);
    return d->m_treeItem && d->m_treeItem->isWritable();
}

bool AceTreeEntity::addChild(AceTreeEntity *child) {
    Q_D(AceTreeEntity);
    auto &parent = child->d_func()->parent;
    if (parent) {
        qWarning() << "AceTreeEntity::addChild(): entity" << child << "has parent" << parent;
        return false;
    }
    d->children.insert(child);
    parent = child;
    childAdded(child);
    return true;
}

bool AceTreeEntity::removeChild(AceTreeEntity *child) {
    Q_D(AceTreeEntity);
    auto it = d->children.find(child);
    if (it == d->children.end()) {
        qWarning() << "AceTreeEntity::removeChild(): entity" << child << "doesn't belong to"
                   << this;
        return false;
    }
    childAboutToRemove(child);
    child->d_func()->parent = nullptr;
    d->children.erase(it);
    return true;
}

AceTreeEntity *AceTreeEntity::itemToEntity(AceTreeItem *item) {
    if (!item)
        return nullptr;
    return AceTreeItemPrivate::get(item)->entity;
}

void AceTreeEntity::childAdded(AceTreeEntity *child) {
    Q_UNUSED(child) //
}

void AceTreeEntity::childAboutToRemove(AceTreeEntity *child) {
    Q_UNUSED(child) //
}

AceTreeEntityExtra *AceTreeEntity::createExtra() const {
    return nullptr;
}

AceTreeEntity::AceTreeEntity(AceTreeEntityPrivate &d, QObject *parent)
    : QObject(parent), d_ptr(&d) {
    d.q_ptr = this;

    d.init();
}

AceTreeEntityExtraPrivate::AceTreeEntityExtraPrivate() : entity(nullptr) {
}

AceTreeEntityExtraPrivate::~AceTreeEntityExtraPrivate() {
}

void AceTreeEntityExtraPrivate::init() {
}

AceTreeEntityExtra::AceTreeEntityExtra() {
}

AceTreeEntityExtra::~AceTreeEntityExtra() {
}

void AceTreeEntityExtra::setup(AceTreeEntity *entity) {
    Q_UNUSED(entity)
}

void AceTreeEntityExtra::event(AceTreeEvent *event) {
    Q_UNUSED(event)
}

AceTreeEntityExtra::AceTreeEntityExtra(AceTreeEntityExtraPrivate &d) : d_ptr(&d) {
    d.q_ptr = this;
    d.init();
}
