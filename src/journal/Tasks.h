#ifndef TASKS_H
#define TASKS_H

#include "Operations.h"

namespace Tasks {

    Q_NAMESPACE

    enum TaskType {
        Commit,
        ChangeStep,
        ReadCheckPoint,
        WriteCheckPoint,
        UpdateModelInfo,
    };
    Q_ENUM_NS(TaskType);

    struct OpsAndAttrs {
        QVector<Operations::BaseOp *> operations;
        QHash<QString, QString> attributes;
    };

    struct BaseTask {
        BaseTask(TaskType type) : t(type) {
        }
        virtual ~BaseTask();

        TaskType t;
    };

    struct CommitTask : public BaseTask {
        CommitTask() : BaseTask(Commit), fsStep(-1), fsMin(-1) {
        }
        ~CommitTask();

        OpsAndAttrs data;
        int fsStep;
        int fsMin;
    };

    struct ChangeStepTask : public BaseTask {
        ChangeStepTask() : BaseTask(ChangeStep), fsStep(0) {
        }
        ~ChangeStepTask();

        int fsStep;
    };

    struct WriteCkptTask : public BaseTask {
        WriteCkptTask() : BaseTask(WriteCheckPoint), num(0), root(nullptr) {
        }
        ~WriteCkptTask();

        int num;
        AceTreeItem *root;
        QVector<AceTreeItem *> removedItems;
    };

    struct ReadCkptTask : public BaseTask {
        ReadCkptTask() : BaseTask(ReadCheckPoint), num(0), buf(nullptr) {
        }
        ~ReadCkptTask();

        int num;
        void *buf;
    };

    struct UpdateModelInfoTask : public BaseTask {
        UpdateModelInfoTask() : BaseTask(UpdateModelInfo) {
        }
        ~UpdateModelInfoTask();

        QVariantHash info;
    };

} // namespace Tasks


#endif // TASKS_H
