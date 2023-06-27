#include "Tasks.h"

#include "AceTreeItem_p.h"

namespace Tasks {

    BaseTask::~BaseTask() {
    }

    CommitTask::~CommitTask() {
    }

    ChangeStepTask::~ChangeStepTask() {
    }

    UpdateModelInfoTask::~UpdateModelInfoTask() {
    }

    WriteCkptTask::~WriteCkptTask() {
        delete root;
        qDeleteAll(removedItems);
    }

    ReadCkptTask::~ReadCkptTask() {
    }

} // namespace Tasks
