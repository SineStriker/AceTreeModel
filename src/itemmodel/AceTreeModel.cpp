#include "AceTreeModel.h"
#include "AceTreeModel_p.h"

#include "AceTreeItem_p.h"
#include "AceTreeMemBackend.h"

#include <QDataStream>
#include <QDebug>
#include <QStack>

#include <iostream>

// #define ENABLE_DEBUG_COUNT

#define myWarning(func) (qWarning().nospace() << "AceTreeModel::" << (func) << "(): ").maybeSpace()

static bool dummy;
bool *AceTreeModelPrivate::InterruptGuard::shared_null = &dummy;

// Model
AceTreeModelPrivate::AceTreeModelPrivate() {
    is_destruct = false;
    backend = nullptr;
    m_state = AceTreeModel::Idle;
    m_metaOperation = false;
    maxIndex = 1;
    rootItem = nullptr;
}

AceTreeModelPrivate::~AceTreeModelPrivate() {
    AceTreeItemPrivate::forceDeleteItem(rootItem);
}

void AceTreeModelPrivate::init() {
}

void AceTreeModelPrivate::setRootItem_helper(AceTreeItem *item) {
    InterruptGuard _guard(this);

    auto org = rootItem;

    // Pre-Propagate
    AceTreeRootEvent e1(AceTreeEvent::RootAboutToChange, item, org);
    event_helper(&e1);

    // Do change
    if (org) {
        org->d_func()->changeManaged(true);
    }
    if (item && item->d_func()->m_managed) {
        item->d_func()->changeManaged(false);
    }
    rootItem = item;

    // Propagate signal
    AceTreeRootEvent e2(AceTreeEvent::RootChange, item, org);
    event_helper(&e2);
}

int AceTreeModelPrivate::addIndex(AceTreeItem *item, size_t idx) {
    int index = idx > 0 ? (maxIndex = qMax(maxIndex, idx), idx) : maxIndex++;
    indexes.insert(std::make_pair(index, item));
    return index;
}

void AceTreeModelPrivate::removeIndex(size_t index) {
    auto it = indexes.find(index);
    if (it == indexes.end())
        return;
    indexes.erase(it);
}

void AceTreeModelPrivate::event_helper(AceTreeEvent *e) {
    Q_Q(AceTreeModel);

    if (m_state == AceTreeModel::Transaction && AceTreeEvent::isChangeType(e->type())) {
        tx_stack << e->clone(); // Save operation to temp stack
    }
    emit q->modelChanged(e);
}

void AceTreeModelPrivate::propagate_model(AceTreeItem *item) {
    Q_Q(AceTreeModel);
    AceTreeItemPrivate::propagate(item, [this, q](AceTreeItem *item) {
        auto d = item->d_func();
        d->m_index = addIndex(item, d->m_index);
        d->model = q;
    });
}

AceTreeModelPrivate *AceTreeModelPrivate::get(AceTreeModel *model) {
    return model->d_func();
}

AceTreeModel::AceTreeModel(QObject *parent) : AceTreeModel(*new AceTreeModelPrivate(), parent) {
    Q_D(AceTreeModel);
    d->backend = new AceTreeMemBackend(this);
    d->backend->setup(this);
}

AceTreeModel::AceTreeModel(AceTreeBackend *backend, QObject *parent)
    : AceTreeModel(*new AceTreeModelPrivate(), parent) {
    Q_D(AceTreeModel);
    backend->setParent(this);
    d->backend = backend;
    d->backend->setup(this);
}

AceTreeModel::~AceTreeModel() {
    Q_D(AceTreeModel);
    if (d->m_state == Transaction) {
        qFatal("AceTreeModel::~AceTreeModel(): transaction is not commited!!!");
        abortTransaction();
    }
    d->is_destruct = true;
}

QVariantHash AceTreeModel::modelInfo() const {
    Q_D(const AceTreeModel);
    return d->backend->modelInfo();
}

void AceTreeModel::setModelInfo(const QVariantHash &info) {
    Q_D(AceTreeModel);
    d->backend->setModelInfo(info);
}

bool AceTreeModel::isWritable() const {
    Q_D(const AceTreeModel);
    return d->m_state == Transaction && !d->m_metaOperation;
}

AceTreeItem *AceTreeModel::itemFromIndex(size_t index) const {
    Q_D(const AceTreeModel);
    if (index == 0)
        return nullptr;
    auto it = d->indexes.find(index);
    if (it == d->indexes.end())
        return nullptr;
    return it->second;
}

AceTreeItem *AceTreeModel::rootItem() const {
    Q_D(const AceTreeModel);
    return d->rootItem;
}

bool AceTreeModel::setRootItem(AceTreeItem *item) {
    Q_D(AceTreeModel);
    if (item == d->rootItem)
        return false;

    if (!isWritable()) {
        myWarning(__func__) << "the model is now not writable" << this;
        return false;
    }

    if (item && !item->isFree()) {
        myWarning(__func__) << "item is not free" << item;
        return false;
    }

    if (item)
        d->propagate_model(item);

    d->setRootItem_helper(item);
    return true;
}

void AceTreeModel::reset() {
}

AceTreeModel::State AceTreeModel::state() const {
    Q_D(const AceTreeModel);
    return d->m_state;
}

void AceTreeModel::beginTransaction() {
    Q_D(AceTreeModel);
    if (d->m_state != Idle) {
        myWarning(__func__) << "A transaction is executing";
        return;
    }

    d->m_state = Transaction;
}

void AceTreeModel::abortTransaction() {
    Q_D(AceTreeModel);
    if (d->m_state != Transaction) {
        myWarning(__func__) << "No executing transaction";
        return;
    }

    auto &stack = d->tx_stack;
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        const auto &e = *it;
        e->execute(true);
        e->clean(); // Need to remove newly created items
        delete e;
    }
    stack.clear();

    d->m_state = Idle;
}

void AceTreeModel::commitTransaction(const QHash<QString, QString> &attributes) {
    Q_D(AceTreeModel);
    if (d->m_state != Transaction) {
        myWarning(__func__) << "No executing transaction";
        return;
    }

    if (d->tx_stack.isEmpty()) {
        d->m_state = Idle;
        return;
    }

    d->backend->commit(d->tx_stack, attributes);
    d->tx_stack.clear();

    d->m_state = Idle;

    emit stepChanged(currentStep());
}

int AceTreeModel::minStep() const {
    Q_D(const AceTreeModel);
    return d->backend->min();
}

int AceTreeModel::maxStep() const {
    Q_D(const AceTreeModel);
    return d->backend->max();
}

int AceTreeModel::currentStep() const {
    Q_D(const AceTreeModel);
    return d->backend->current();
}

QHash<QString, QString> AceTreeModel::stepAttributes(int step) const {
    Q_D(const AceTreeModel);
    return d->backend->attributes(step);
}

void AceTreeModel::nextStep() {
    Q_D(AceTreeModel);

    if (d->m_state != Idle) {
        myWarning(__func__) << "Not available to change step";
        return;
    }
    d->m_state = Redo;
    d->backend->redo();
    d->m_state = Idle;

    emit stepChanged(currentStep());
}

void AceTreeModel::previousStep() {
    Q_D(AceTreeModel);

    if (d->m_state != Idle) {
        myWarning(__func__) << "Not available to change step";
        return;
    }
    d->m_state = Undo;
    d->backend->undo();
    d->m_state = Idle;

    emit stepChanged(currentStep());
}

AceTreeModel::AceTreeModel(AceTreeModelPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) {
    d.q_ptr = this;
    d.init();
}
