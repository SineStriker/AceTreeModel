#include "AceTreeJournalBackend.h"
#include "AceTreeJournalBackend_p.h"

#include <QDebug>
#include <QFileInfo>

#define myWarning(func)                                                                            \
    (qWarning().nospace() << "AceTreeJournalBackend::" << (func) << "(): ").maybeSpace()

AceTreeJournalBackendPrivate::AceTreeJournalBackendPrivate() {
    worker = nullptr;
}

AceTreeJournalBackendPrivate::~AceTreeJournalBackendPrivate() {
    if (worker) {
        pushTask(new Tasks::BaseTask(Tasks::Exit));
        worker->join();
        delete worker;
    }
}

void AceTreeJournalBackendPrivate::init() {
}

void AceTreeJournalBackendPrivate::modelInfoSet() {
    auto task = new Tasks::UpdateModelInfoTask();
    task->info = modelInfo;
    pushTask(task);
}

void AceTreeJournalBackendPrivate::afterChangeStep(int step) {
    Q_Q(AceTreeJournalBackend);

    Q_UNUSED(step);

    auto task = new Tasks::ChangeStepTask();
    task->current = q->current();
    task->total = q->max();
    pushTask(task);
}

void AceTreeJournalBackendPrivate::afterCommit(const QList<AceTreeEvent *> &events,
                                               const QHash<QString, QString> &attributes) {

    QVector<Operations::BaseOp *> ops;
    ops.reserve(events.size());
    for (const auto &event : events) {
        ops.append(Operations::toOp(event));
    }

    auto task = new Tasks::CommitTask();
    task->operations = std::move(ops);
    task->attributes = attributes;
    pushTask(task);
}

void AceTreeJournalBackendPrivate::workerRoutine() {
    bool over = false;

    // Do initial work

    while (!over) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {
            return !task_queue.empty(); //
        });

        auto cur_task = task_queue.front();
        task_queue.pop_front();
        lock.unlock();

        // Execute task
        switch (cur_task->t) {
            case Tasks::Commit: {
                auto task = static_cast<Tasks::CommitTask *>(cur_task);
                break;
            }
            case Tasks::ChangeStep: {
                auto task = static_cast<Tasks::ChangeStepTask *>(cur_task);
                break;
            }
            case Tasks::WriteCheckPoint: {
                auto task = static_cast<Tasks::WriteCheckPointTask *>(cur_task);
                break;
            }
            case Tasks::UpdateModelInfo: {
                auto task = static_cast<Tasks::UpdateModelInfoTask *>(cur_task);
                break;
            }
            case Tasks::Exit: {
                over = true;
                break;
            }
            default:
                break;
        }
    }

    // Do final work
}

void AceTreeJournalBackendPrivate::pushTask(Tasks::BaseTask *task) {
    std::unique_lock<std::mutex> lock(mtx);
    task_queue.push_back(task);
    lock.unlock();
    cv.notify_all();
}

AceTreeJournalBackend::AceTreeJournalBackend(QObject *parent)
    : AceTreeJournalBackend(*new AceTreeJournalBackendPrivate(), parent) {
}

AceTreeJournalBackend::~AceTreeJournalBackend() {
}

bool AceTreeJournalBackend::start(const QString &dir) {
    Q_D(AceTreeJournalBackend);
    QFileInfo info(dir);
    if (!info.isDir() || !info.isWritable()) {
        myWarning(__func__) << dir << "is not available";
        return false;
    }

    d->dir = dir;
    return true;
}

bool AceTreeJournalBackend::recover(const QString &dir) {
    Q_D(AceTreeJournalBackend);
    d->dir = dir;
    return true;
}

void AceTreeJournalBackend::setup(AceTreeModel *model) {
    Q_D(AceTreeJournalBackend);

    if (!d->dir.isEmpty()) {
        d->worker = new std::thread(&AceTreeJournalBackendPrivate::workerRoutine, d);
    }

    AceTreeMemBackend::setup(model);
}

AceTreeJournalBackend::AceTreeJournalBackend(AceTreeJournalBackendPrivate &d, QObject *parent)
    : AceTreeMemBackend(d, parent) {
    d.init();
}
