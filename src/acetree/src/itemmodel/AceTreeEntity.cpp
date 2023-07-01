#include "AceTreeEntity.h"
#include "AceTreeEntity_p.h"

#include "AceTreeItem_p.h"

AceTreeEntityPrivate::AceTreeEntityPrivate(AceTreeEntityExtra *extra) : extra(extra) {
    m_treeItem = nullptr;
    parent = nullptr;
    is_clearing = false;
}

AceTreeEntityPrivate::~AceTreeEntityPrivate() {
    delete extra;
}

void AceTreeEntityPrivate::init() {
    if (extra)
        extra->d_func()->entity = q_ptr;
}

void AceTreeEntityPrivate::event(AceTreeEvent *event) {
    if (extra) {
        extra->event(event);
    }
}

void AceTreeEntityPrivate::deferred_init() {
}

AceTreeEntity::AceTreeEntity(QObject *parent) : AceTreeEntity(nullptr, parent) {
}

AceTreeEntity::AceTreeEntity(AceTreeEntityExtra *extra, QObject *parent)
    : AceTreeEntity(*new AceTreeEntityPrivate(extra), parent) {
}

AceTreeEntity::~AceTreeEntity() {
    Q_D(AceTreeEntity);
    d->is_clearing = true;

    if (d->parent && !d->parent->d_func()->is_clearing)
        d->parent->removeChild(this);

    auto &treeItem = d->m_treeItem;
    if (treeItem) {
        AceTreeItemPrivate::get(treeItem)->entity = nullptr;
        if (treeItem->isFree()) {
            delete treeItem;
        }
    }

    for (const auto &child : qAsConst(d->children)) {
        child->d_func()->parent = nullptr;
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
    AceTreeItemPrivate::get(treeItem)->entity = this;

    d->deferred_init();
    if (d->extra)
        d->extra->setup(this);

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
    AceTreeItemPrivate::get(treeItem)->entity = this;

    d->deferred_init();
    if (d->extra)
        d->extra->setup(this);

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
        qWarning() << "AceTreeEntity::removeChild(): entity" << child << "doesn't belong to" << this;
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

AceTreeItem *AceTreeEntity::treeItem() const {
    Q_D(const AceTreeEntity);
    return d->m_treeItem;
}

void AceTreeEntity::childAdded(AceTreeEntity *child) {
    Q_UNUSED(child) //
}

void AceTreeEntity::childAboutToRemove(AceTreeEntity *child){
    Q_UNUSED(child) //
}

AceTreeEntity::AceTreeEntity(AceTreeEntityPrivate &d, QObject *parent)
    : QObject(parent), d_ptr(&d) {
    d.q_ptr = this;

    d.init();
}

AceTreeEntityExtra *AceTreeEntity::extra() const {
    Q_D(const AceTreeEntity);
    return d->extra;
}

void AceTreeEntity::itemEvent(AceTreeEvent *event) {
    Q_D(AceTreeEntity);
    d->event(event);
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

AceTreeEntity *AceTreeEntityExtra::entity() const {
    Q_D(const AceTreeEntityExtra);
    return d->entity;
}

void AceTreeEntityExtra::setup(AceTreeEntity *entity) {
    Q_UNUSED(entity)
}

void AceTreeEntityExtra::event(AceTreeEvent *event){Q_UNUSED(event)}

AceTreeEntityExtra::AceTreeEntityExtra(AceTreeEntityExtraPrivate &d)
    : d_ptr(&d) {
    d.init();
}

AceTreeEntityNotifyExtra::AceTreeEntityNotifyExtra()
    : AceTreeEntityNotifyExtra(*new AceTreeEntityNotifyExtraPrivate()) {
}

AceTreeEntityNotifyExtra::~AceTreeEntityNotifyExtra() {
}

void AceTreeEntityNotifyExtra::addDynamicDataNotifier(const QString &key,
                                                      const AceTreeEntityNotifyExtra::Notifier &notifier) {
    Q_D(AceTreeEntityNotifyExtra);
    d->dynamicPropertyNotifiers.insert(key, notifier);
}

void AceTreeEntityNotifyExtra::removeDynamicDataNotifier(const QString &key) {
    Q_D(AceTreeEntityNotifyExtra);
    d->dynamicPropertyNotifiers.remove(key);
}

void AceTreeEntityNotifyExtra::addPropertyNotifier(const QString &key,
                                                   const AceTreeEntityNotifyExtra::Notifier &notifier) {
    Q_D(AceTreeEntityNotifyExtra);
    d->propertyNotifiers.insert(key, notifier);
}

void AceTreeEntityNotifyExtra::removePropertyNotifier(const QString &key) {
    Q_D(AceTreeEntityNotifyExtra);
    d->propertyNotifiers.remove(key);
}

void AceTreeEntityNotifyExtra::event(AceTreeEvent *event) {
    Q_D(AceTreeEntityNotifyExtra);
    switch (event->type()) {
        case AceTreeEvent::DynamicDataChange: {
            auto e = static_cast<AceTreeValueEvent *>(event);
            auto it = d->dynamicPropertyNotifiers.find(e->key());
            if (it == d->dynamicPropertyNotifiers.end())
                return;
            it.value()(e->value(), e->oldValue());
            break;
        }
        case AceTreeEvent::PropertyChange: {
            auto e = static_cast<AceTreeValueEvent *>(event);
            auto it = d->propertyNotifiers.find(e->key());
            if (it == d->propertyNotifiers.end())
                return;
            it.value()(e->value(), e->oldValue());
            break;
        }
        default:
            break;
    }
}

AceTreeEntityNotifyExtra::AceTreeEntityNotifyExtra(AceTreeEntityNotifyExtraPrivate &d) : AceTreeEntityExtra(d) {
}
