#ifndef ACETREEJOURNALBACKEND_P_H
#define ACETREEJOURNALBACKEND_P_H

#include <QFile>

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

    void setup_helper();

    int maxCheckPoints;
    QString dir;

    int fsMin;
    int fsMax;

    struct RecoverData {
        int fsMin;
        int fsMax;
        int fsStep;
        int currentNum;
        AceTreeItem *root;
        QVector<AceTreeItem *> removedItems;
        QVector<Tasks::OpsAndAttrs> backwardData;
        QVector<Tasks::OpsAndAttrs> forwardData;

        ~RecoverData();
    };
    RecoverData *recoverData;

    QHash<QString, QString> fs_getAttributes(int step) const;
    QHash<QString, QString> fs_getAttributes_do(int step) const;

    static bool readJournal(QFile &file, int maxSteps, QVector<Tasks::OpsAndAttrs> &res,
                            bool brief);
    static bool readCheckPoint(QFile &file, AceTreeItem **rootRef,
                               QVector<AceTreeItem *> &removedItemsRef);

    void updateStackSize();

    void afterModelInfoSet() override;
    void afterCurrentChange() override;
    void afterCommit(const QList<AceTreeEvent *> &events,
                     const QHash<QString, QString> &attributes) override;

    void abortBackwardReadTask();
    void abortForwardReadTask();

    void extractBackwardJournal(QVector<AceTreeItem *> &removedItems,
                                QVector<Tasks::OpsAndAttrs> &data);
    void extractForwardJournal(QVector<Tasks::OpsAndAttrs> &data);

    // Worker routine
    void workerRoutine();
    void pushTask(Tasks::BaseTask *task, bool unshift = false);

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

    struct AttributesTaskBuffer {
        std::mutex mtx;
        std::condition_variable cv;
        QHash<QString, QString> res;
        volatile bool finished;
        AttributesTaskBuffer() : finished(false) {
        }
    };

    QFile *stepFile;
    QFile *infoFile;
    QFile *txFile;
    int txNum;

    int fsMin2;
    int fsMax2;
    int fsStep2;
    std::list<Tasks::BaseTask *> task_queue;
};

#endif // ACETREEJOURNALBACKEND_P_H
