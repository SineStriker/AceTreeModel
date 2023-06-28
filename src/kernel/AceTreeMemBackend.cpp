#include "AceTreeMemBackend.h"
#include "AceTreeMemBackend_p.h"

#include "AceTreeItem_p.h"

AceTreeMemBackendPrivate::AceTreeMemBackendPrivate() {
    maxSteps = 4;
    model = nullptr;
    min = 0;
    current = 0;
}

AceTreeMemBackendPrivate::~AceTreeMemBackendPrivate() {
    removeEvents(0, stack.size());
}

void AceTreeMemBackendPrivate::init() {
}

void AceTreeMemBackendPrivate::removeEvents(int b, int e) {
    if (b >= e){
        return;
    }

    auto begin = stack.begin() + b;
    auto end = stack.begin() + e;
    for (auto it = begin; it != end; ++it) {
        AceTreeMemBackendPrivate::TransactionData &tx = *it;
        for (const auto &e : qAsConst(tx.events)) {
            AceTreeItemPrivate::cleanEvent(e);
            delete e;
        }
    }
    stack.erase(begin, end);
}

void AceTreeMemBackendPrivate::afterModelInfoSet() {
}

void AceTreeMemBackendPrivate::afterCurrentChange() {
}

void AceTreeMemBackendPrivate::afterCommit(const QList<AceTreeEvent *> &events,
                                           const QHash<QString, QString> &attributes) {
    Q_UNUSED(events);
    Q_UNUSED(attributes);

    if (current > 2 * maxSteps) {
        // Remove head
        removeEvents(0, maxSteps);
        min += maxSteps;
        current -= maxSteps;
    }
}

AceTreeMemBackend::AceTreeMemBackend(QObject *parent)
    : AceTreeMemBackend(*new AceTreeMemBackendPrivate(), parent) {
}

AceTreeMemBackend::~AceTreeMemBackend() {
}

int AceTreeMemBackend::maxReservedSteps() const {
    Q_D(const AceTreeMemBackend);
    return d->maxSteps;
}

void AceTreeMemBackend::setMaxReservedSteps(int steps) {
    Q_D(AceTreeMemBackend);
    if (d->model) {
        return; // Not allowed to change after setup
    }

    if (steps < 100) {
        return; // To small
    }

    d->maxSteps = steps;
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
    d->afterModelInfoSet();
}

int AceTreeMemBackend::min() const {
    Q_D(const AceTreeMemBackend);
    return d->min;
}

int AceTreeMemBackend::max() const {
    Q_D(const AceTreeMemBackend);
    return d->min + d->stack.size();
}

int AceTreeMemBackend::current() const {
    Q_D(const AceTreeMemBackend);
    return d->min + d->current;
}

QHash<QString, QString> AceTreeMemBackend::attributes(int step) const {
    Q_D(const AceTreeMemBackend);
    step -= d->min;
    if (step < 0 || step >= d->stack.size())
        return {};
    return d->stack.at(step).attrs;
}

void AceTreeMemBackend::undo() {
    Q_D(AceTreeMemBackend);

    if (d->current == 0)
        return;

    // Step backward
    const auto &tx = d->stack.at(d->current - 1);
    for (auto it = tx.events.rbegin(); it != tx.events.rend(); ++it) {
        AceTreeItemPrivate::executeEvent(*it, true);
    }
    d->current--;

    d->afterCurrentChange();
}

void AceTreeMemBackend::redo() {
    Q_D(AceTreeMemBackend);
    if (d->current == d->stack.size())
        return;

    // Step forward
    const auto &tx = d->stack.at(d->current);
    for (auto it = tx.events.begin(); it != tx.events.end(); ++it) {
        AceTreeItemPrivate::executeEvent(*it, false);
    }
    d->current++;

    d->afterCurrentChange();
}

void AceTreeMemBackend::commit(const QList<AceTreeEvent *> &events,
                               const QHash<QString, QString> &attrs) {
    Q_D(AceTreeMemBackend);

    // Truncate tail
    if (d->current < d->stack.size()) {
        d->removeEvents(d->current, d->stack.size());
    }

    // Commit
    d->stack.append({events, attrs});
    d->current++;

    d->afterCommit(events, attrs);
}

AceTreeMemBackend::AceTreeMemBackend(AceTreeMemBackendPrivate &d, QObject *parent)
    : AceTreeBackend(parent), d_ptr(&d) {
    d.q_ptr = this;

    d.init();
}
