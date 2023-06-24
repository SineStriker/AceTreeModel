#ifndef ACETREEJOURNALBACKEND_P_H
#define ACETREEJOURNALBACKEND_P_H

#include <condition_variable>
#include <mutex>
#include <thread>

#include "AceTreeJournalBackend.h"
#include "AceTreeMemBackend_p.h"

#include "journal/Tasks.h"

class AceTreeJournalBackendPrivate : AceTreeMemBackendPrivate {
    Q_DECLARE_PUBLIC(AceTreeJournalBackend)
public:
    AceTreeJournalBackendPrivate();
    virtual ~AceTreeJournalBackendPrivate();

    void init();

    void modelInfoSet() override;
    void afterChangeStep(int step) override;
    void afterCommit(const QList<AceTreeEvent *> &events,
                     const QHash<QString, QString> &attributes) override;

    QString dir;

    // Worker routine
    void workerRoutine();
    void pushTask(Tasks::BaseTask *task);

    std::thread *worker;
    std::mutex mtx;
    std::condition_variable cv;

    std::list<Tasks::BaseTask *> task_queue;
};

#endif // ACETREEJOURNALBACKEND_P_H
