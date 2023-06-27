#ifndef ACETREEJOURNALBACKEND_P_H
#define ACETREEJOURNALBACKEND_P_H

#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "AceTreeJournalBackend.h"
#include "AceTreeMemBackend_p.h"

#include "journal/Tasks.h"

class AceTreeJournalBackendPrivate : AceTreeMemBackendPrivate {
    Q_DECLARE_PUBLIC(AceTreeJournalBackend)
public:
    AceTreeJournalBackendPrivate();
    virtual ~AceTreeJournalBackendPrivate();

    void init();

    int maxCheckPoints;
    QString dir;

    int fsMin;
    int fsMax;

    QHash<QString, QString> fs_getAttribute(int step) const;

    void afterModelInfoSet() override;
    void afterCurrentChange() override;
    void afterCommit(const QList<AceTreeEvent *> &events,
                     const QHash<QString, QString> &attributes) override;

    void updateStackSize();

    // Worker routine
    void workerRoutine();
    void pushTask(Tasks::BaseTask *task);

    std::thread *worker;
    std::mutex mtx;

    struct CheckPointTaskBuffer {
        bool brief; // Read only id of insert operation
        QVector<AceTreeItem *> removedItems;
        QVector<Tasks::OpsAndAttrs> data;
        std::mutex mtx;
        std::condition_variable cv;
        volatile bool obsolete;
        volatile bool finished;
        CheckPointTaskBuffer() : brief(false), obsolete(false), finished(false) {
        }
        ~CheckPointTaskBuffer();
    };
    CheckPointTaskBuffer *backward_buf;

    CheckPointTaskBuffer *forward_buf;

    int fsMin2;
    int fsMax2;
    int fsStep2;
    std::list<Tasks::BaseTask *> task_queue;
};

#endif // ACETREEJOURNALBACKEND_P_H
