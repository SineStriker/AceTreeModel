#include "AceTreeMemBackend.h"
#include "AceTreeMemBackend_p.h"

#include "AceTreeItem_p.h"

template <class T>
static void removeEvents(T &stack, typename T::iterator begin, typename T::iterator end) {
    for (auto it = begin; it != end; ++it) {
        AceTreeMemBackendPrivate::TransactionData &tx = *it;
        for (const auto &e : qAsConst(tx.events)) {
            AceTreeItemPrivate::cleanEvent(e);
            delete e;
        }
    }
    stack.erase(begin, end);
}

AceTreeMemBackendPrivate::AceTreeMemBackendPrivate() {
    maxReserved = 100;
    model = nullptr;
    step = 0;
}

AceTreeMemBackendPrivate::~AceTreeMemBackendPrivate() {
}

void AceTreeMemBackendPrivate::init() {
}

void AceTreeMemBackendPrivate::modelInfoSet() {
}

void AceTreeMemBackendPrivate::afterChangeStep(int step) {
    Q_UNUSED(step)
}

void AceTreeMemBackendPrivate::afterCommit(const QList<AceTreeEvent *> &events,
                                           const QHash<QString, QString> &attributes) {
    Q_UNUSED(events);
    Q_UNUSED(attributes);
}

void AceTreeMemBackendPrivate::removeForwardSteps() {
    removeEvents(stack, stack.begin() + step, stack.end());
}

void AceTreeMemBackendPrivate::removeEarlySteps() {
    // Remove all managed items
    removeEvents(stack, stack.begin(), stack.begin() + maxReserved);
    minStep += maxReserved;
    step -= maxReserved;
}

AceTreeMemBackend::AceTreeMemBackend(QObject *parent)
    : AceTreeMemBackend(*new AceTreeMemBackendPrivate(), parent) {
}

AceTreeMemBackend::~AceTreeMemBackend() {
}

int AceTreeMemBackend::maxReservedSteps() const {
    Q_D(const AceTreeMemBackend);
    return d->maxReserved;
}

void AceTreeMemBackend::setMaxReservedSteps(int steps) {
    Q_D(AceTreeMemBackend);
    if (d->model) {
        return; // Not allowed to change after setup
    }
    d->maxReserved = steps;
}

void AceTreeMemBackend::setup(AceTreeModel *model) {
    Q_D(AceTreeMemBackend);
    d->model = model;
}

QVariantHash AceTreeMemBackend::modelInfo() const {
    Q_D(const AceTreeMemBackend);
    return d->modelInfo;
}

void AceTreeMemBackend::setModelInfo(const QVariantHash &info) {
    Q_D(AceTreeMemBackend);
    d->modelInfo = info;
    d->modelInfoSet();
}

int AceTreeMemBackend::min() const {
    Q_D(const AceTreeMemBackend);
    return d->minStep;
}

int AceTreeMemBackend::max() const {
    Q_D(const AceTreeMemBackend);
    return d->minStep + d->stack.size();
}

int AceTreeMemBackend::current() const {
    Q_D(const AceTreeMemBackend);
    return d->minStep + d->step;
}

QHash<QString, QString> AceTreeMemBackend::attributes(int step) const {
    Q_D(const AceTreeMemBackend);
    step -= d->minStep;
    if (step < 0 || step >= d->stack.size())
        return {};
    return d->stack.at(step).attrs;
}

void AceTreeMemBackend::undo() {
    Q_D(AceTreeMemBackend);

    if (d->step == 0)
        return;

    // Step backward
    const auto &tx = d->stack.at(d->step - 1);
    for (auto it = tx.events.rbegin(); it != tx.events.rend(); ++it) {
        AceTreeItemPrivate::executeEvent(*it, true);
    }
    d->step--;

    d->afterChangeStep(d->step);
}

void AceTreeMemBackend::redo() {
    Q_D(AceTreeMemBackend);
    if (d->step == d->stack.size())
        return;

    // Step forward
    const auto &tx = d->stack.at(d->step);
    for (auto it = tx.events.begin(); it != tx.events.end(); ++it) {
        AceTreeItemPrivate::executeEvent(*it, false);
    }
    d->step++;

    d->afterChangeStep(d->step);
}

void AceTreeMemBackend::commit(const QList<AceTreeEvent *> &events,
                               const QHash<QString, QString> &attrs) {
    Q_D(AceTreeMemBackend);

    // Truncate tail
    if (d->step < d->stack.size()) {
        d->removeForwardSteps();
    }

    // Commit
    d->stack.append({events, attrs});
    d->step++;

    // Remove head
    if (d->stack.size() >= 2 * d->maxReserved) {
        d->removeEarlySteps();
    }

    d->afterCommit(events, attrs);
}

AceTreeMemBackend::AceTreeMemBackend(AceTreeMemBackendPrivate &d, QObject *parent)
    : AceTreeBackend(parent), d_ptr(&d) {
    d.q_ptr = this;

    d.init();
}
