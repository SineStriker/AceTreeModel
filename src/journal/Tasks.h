#ifndef TASKS_H
#define TASKS_H

#include "Operations.h"

namespace Tasks {

    Q_NAMESPACE

    enum TaskType {
        Commit,
        ChangeStep,
        WriteCheckPoint,
        UpdateModelInfo,
        Exit,
    };
    Q_ENUM_NS(TaskType);

    struct BaseTask {
        BaseTask(TaskType type) : t(type) {
        }
        virtual ~BaseTask();

        TaskType t;
    };

    struct CommitTask : public BaseTask {
        CommitTask() : BaseTask(Commit) {
        }
        ~CommitTask();

        QVector<Operations::BaseOp *> operations;
        QHash<QString, QString> attributes;
    };

    struct ChangeStepTask : public BaseTask {
        ChangeStepTask() : BaseTask(ChangeStep), current(0), total(0) {
        }
        ~ChangeStepTask();

        int current;
        int total;
    };

    struct WriteCheckPointTask : public BaseTask {
        WriteCheckPointTask() : BaseTask(WriteCheckPoint), root(nullptr) {
        }
        ~WriteCheckPointTask();

        AceTreeItem *root;
    };

    struct UpdateModelInfoTask : public BaseTask {
        UpdateModelInfoTask() : BaseTask(UpdateModelInfo) {
        }
        ~UpdateModelInfoTask();

        QVariantHash info;
    };

} // namespace Tasks


#endif // TASKS_H
