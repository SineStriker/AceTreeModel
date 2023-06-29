#include "AceTreeJournalBackend.h"
#include "AceTreeJournalBackend_p.h"

#include "AceTreeItem_p.h"
#include "AceTreeModel_p.h"

#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#define myWarning(func)                                                                            \
    (qWarning().nospace() << "AceTreeJournalBackend::" << (func) << "():").space()

static bool truncateJournals(const QString &dir, int i) {
    bool b1 = QFile::remove(QString("%1/journal_%2.dat").arg(dir, QString::number(i)));
    bool b2 = QFile::remove(QString("%1/ckpt_%2.dat").arg(dir, QString::number(i)));

    if (b1) {
        qDebug().noquote().nospace()
            << "[Journal] Remove " << QString("%1/journal_%2.dat").arg(dir, QString::number(i));
    }
    if (b2) {
        qDebug().noquote().nospace()
            << "[Journal] Remove " << QString("%1/ckpt_%2.dat").arg(dir, QString::number(i));
    }

    return b1 || b2;
};

AceTreeJournalBackendPrivate::AceTreeJournalBackendPrivate() {
    maxCheckPoints = 1;
    worker = nullptr;
    fsMin = fsMax = 0;
    recoverData = nullptr;
    writeCkptTask = nullptr;
    backward_buf = forward_buf = nullptr;
    stepFile = infoFile = txFile = nullptr;
    txNum = -1;
    fsMin2 = fsMax2 = fsStep2 = 0;
}

AceTreeJournalBackendPrivate::~AceTreeJournalBackendPrivate() {
    delete recoverData;
    delete writeCkptTask;

    if (worker) {
        worker->join();
        delete worker;
    }

    delete stepFile;
    delete infoFile;
    delete txFile;
}

void AceTreeJournalBackendPrivate::init() {
}

void AceTreeJournalBackendPrivate::setup_helper() {
    if (!recoverData) {
        QFile file(QString("%1/model_step.dat").arg(dir));
        file.open(QIODevice::WriteOnly);
        QDataStream out(&file);

        // Write steps
        out << maxSteps << maxCheckPoints << fsMin2 << fsMax2 << fsStep2;
        return;
    }

    fsMin2 = fsMin = recoverData->fsMin;
    fsMax2 = fsMax = recoverData->fsMax;
    fsStep2 = recoverData->fsStep;

    if (recoverData->root) {
        AceTreeModelPrivate::get(model)->setRootItem_backend(recoverData->root);
        recoverData->root = nullptr; // Get ownership
    }

    current = 0;
    min = recoverData->currentNum * maxSteps;

    // Get backward transactions
    if (!recoverData->backwardData.isEmpty()) {
        extractBackwardJournal(recoverData->removedItems, recoverData->backwardData);
    }

    // Get forward transactions
    if (!recoverData->forwardData.isEmpty()) {
        extractForwardJournal(recoverData->forwardData);
    }

    int expected = fsStep2 - min;

    // Undo or redo
    while (current > expected) {
        const auto &tx = stack.at(current - 1);
        for (auto it = tx.events.rbegin(); it != tx.events.rend(); ++it) {
            AceTreeItemPrivate::executeEvent(*it, true);
        }
        current--;
    }

    while (current < expected) {
        const auto &tx = stack.at(current);
        for (auto it = tx.events.begin(); it != tx.events.end(); ++it) {
            AceTreeItemPrivate::executeEvent(*it, false);
        }
        current++;
    }

    delete recoverData;
    recoverData = nullptr;

    // Need to prepare a checkpoint to write as if a transaction has been commited
    if (stack.size() % maxSteps == 0) {
        writeCkptTask = genWriteCkptTask();
    }
}

QHash<QString, QString> AceTreeJournalBackendPrivate::fs_getAttributes(int step) const {
    if (!worker) {
        return fs_getAttributes_do(step);
    }

    auto task = new Tasks::ReadAttributesTask();
    task->step = step;

    AttributesTaskBuffer buf;
    task->buf = &buf;

    std::unique_lock<std::mutex> lock(buf.mtx);
    const_cast<AceTreeJournalBackendPrivate *>(this)->pushTask(task, true);
    while (!buf.finished) {
        buf.cv.wait(lock);
    }
    lock.unlock();

    return buf.res;
}

QHash<QString, QString> AceTreeJournalBackendPrivate::fs_getAttributes_do(int step) const {
    int num = (step - 1) / maxSteps;

    QScopedPointer<QFile> file;
    QDataStream in;
    if (num == txNum && txFile && txFile->isOpen()) {
        in.setDevice(txFile);
    } else {
        file.reset(new QFile(QString("%1/journal_%2.dat").arg(dir, QString::number(num))));
        if (!file->open(QIODevice::ReadOnly)) {
            return {};
        }
        in.setDevice(file.data());
    }

    qDebug().noquote().nospace() << "[Journal] Read attributes at " << step << " in "
                                 << QString("%1/journal_%2.dat").arg(dir, QString::number(num));

    auto &dev = *in.device();
    auto pos0 = dev.pos();

    int cur = (step - 1) % maxSteps + 1;
    if (cur == 1) {
        dev.seek((maxSteps + 2) * sizeof(qint64)); // Data section start
    } else {
        dev.seek((cur - 1) * sizeof(qint64));      // Previous transaction end
        qint64 pos;
        in >> pos;
        dev.seek(pos);
    }

    QHash<QString, QString> res;
    in >> res;
    if (in.status() != QDataStream::Ok) {
        res.clear();
    }

    // Restore pos
    dev.seek(pos0);
    return res;
}

bool AceTreeJournalBackendPrivate::readJournal(QFile &file, int maxSteps,
                                               QVector<Tasks::OpsAndAttrs> &res, bool brief) {
    QDataStream in(&file);
    QList<qint64> positions{
        static_cast<qint64>((maxSteps + 2) * sizeof(qint64)),
    };

    qint64 cnt;
    in >> cnt;

    positions.reserve(cnt);
    for (int i = 0; i < cnt - 1; ++i) {
        qint64 pos;
        in >> pos;
        positions.append(pos);
    }

    if (in.status() != QDataStream::Ok) {
        return false;
    }

    QVector<Tasks::OpsAndAttrs> data;
    data.reserve(cnt);
    for (const auto &pos : qAsConst(positions)) {
        file.seek(pos);

        // Read attributes
        QHash<QString, QString> attrs;
        in >> attrs;

        // Read operations
        int op_cnt;
        in >> op_cnt;
        QVector<Operations::BaseOp *> ops;
        ops.reserve(op_cnt);
        for (int i = 0; i < op_cnt; ++i) {
            bool success = false;
            Operations::Change c;
            in >> c;

            Operations::BaseOp *baseOp = nullptr;
            switch (c) {
                case Operations::PropertyChange: {
                    auto op = new Operations::PropertyChangeOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::BytesReplace: {
                    auto op = new Operations::BytesReplaceOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::BytesInsert: {
                    auto op = new Operations::BytesInsertRemoveOp(true);
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::BytesRemove: {
                    auto op = new Operations::BytesInsertRemoveOp(false);
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RowsInsert: {
                    auto op = new Operations::RowsInsertOp();
                    success = brief ? op->readBrief(in) : op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RowsMove: {
                    auto op = new Operations::RowsMoveOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RowsRemove: {
                    auto op = new Operations::RowsRemoveOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RecordAdd: {
                    auto op = new Operations::RecordAddOp();
                    success = brief ? op->readBrief(in) : op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RecordRemove: {
                    auto op = new Operations::RecordRemoveOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::ElementAdd: {
                    auto op = new Operations::ElementAddOp();
                    success = brief ? op->readBrief(in) : op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::ElementRemove: {
                    auto op = new Operations::ElementRemoveOp();
                    success = op->read(in);
                    baseOp = op;
                    break;
                }
                case Operations::RootChange: {
                    auto op = new Operations::RootChangeOp();
                    success = brief ? op->readBrief(in) : op->read(in);
                    baseOp = op;
                    break;
                }
                default:
                    break;
            }
            ops.append(baseOp);

            if (!success) {
                qDeleteAll(ops);
                goto failed;
            }
        }

        data.append({ops, attrs});
    }

    res = std::move(data);
    return true;

failed:
    for (const auto &item : qAsConst(data)) {
        qDeleteAll(item.operations);
    }
    return false;
}

bool AceTreeJournalBackendPrivate::readCheckPoint(QFile &file, AceTreeItem **rootRef,
                                                  QVector<AceTreeItem *> *removedItemsRef) {
    QDataStream in(&file);
    in.skipRawData(4);

    AceTreeItem *root = nullptr;
    QVector<AceTreeItem *> removedItems;

    if (!rootRef) {
        // Read removed items pos
        qint64 pos;
        in >> pos;
        file.seek(pos);
    } else {
        // Skip removed items pos
        in.skipRawData(sizeof(qint64));

        // Read root id
        size_t id;
        in >> id;
        if (id != 0) {
            // Read root
            root = AceTreeItemPrivate::read_helper(in, false);
            if (!root) {
                qDebug() << "read item failed";
                return false;
            }
        }
    }

    if (removedItemsRef) {
        // Read removed items size
        int sz;
        in >> sz;

        // Read removed items data
        removedItems.reserve(sz);
        for (int i = 0; i < sz; ++i) {
            auto item = AceTreeItemPrivate::read_helper(in, false);
            if (!item) {
                delete root;
                qDeleteAll(removedItems);
                return false;
            }
            removedItems.append(item);
        }
    }

    if (root)
        *rootRef = root;

    if (removedItemsRef)
        *removedItemsRef = std::move(removedItems);

    return true;
}

bool AceTreeJournalBackendPrivate::writeCheckPoint(QFile &file, AceTreeItem *root,
                                                   const QVector<AceTreeItem *> &removedItems) {
    file.write("CKPT", 4);
    QDataStream out(&file);
    out << qint64(0);
    if (root) {
        // Write index
        out << root->index();

        // Write root data
        AceTreeItemPrivate::get(root)->write_helper(out, false);
    } else {
        // Write 0
        out << size_t(0);
    }

    // Write removed items pos
    qint64 pos = file.pos();
    file.seek(4);
    out << pos;
    file.seek(pos);

    // Write removed items size
    out << (removedItems.size());

    // Write removed items data
    for (const auto &item : qAsConst(removedItems)) {
        AceTreeItemPrivate::get(item)->write_helper(out, false);
    }
    return true;
}

Tasks::WriteCkptTask *AceTreeJournalBackendPrivate::genWriteCkptTask() const {
    // Collect all removed items
    QVector<AceTreeItem *> removedItems;
    for (int i = stack.size() - maxSteps; i != stack.size(); ++i) {
        const auto &tx = stack.at(i);
        for (const auto &e : qAsConst(tx.events)) {
            switch (e->type()) {
                case AceTreeEvent::RowsRemove: {
                    auto event = static_cast<AceTreeRowsInsDelEvent *>(e);
                    const auto &children = event->children();
                    for (const auto &child : children) {
                        removedItems.append(AceTreeItemPrivate::get(child)->clone_helper(false));
                    }
                    break;
                }

                case AceTreeEvent::RecordRemove: {
                    auto event = static_cast<AceTreeRecordEvent *>(e);
                    removedItems.append(
                        AceTreeItemPrivate::get(event->child())->clone_helper(false));
                    break;
                }

                case AceTreeEvent::ElementRemove: {
                    auto event = static_cast<AceTreeElementEvent *>(e);
                    removedItems.append(
                        AceTreeItemPrivate::get(event->child())->clone_helper(false));
                    break;
                }

                case AceTreeEvent::RootChange: {
                    auto event = static_cast<AceTreeRootEvent *>(e);
                    if (event->oldRoot())
                        removedItems.append(
                            AceTreeItemPrivate::get(event->oldRoot())->clone_helper(false));
                    break;
                }

                default:
                    break;
            }
        }
    }

    auto task = new Tasks::WriteCkptTask();
    task->num = (min + stack.size()) / maxSteps;
    task->removedItems = std::move(removedItems);
    task->root = AceTreeItemPrivate::get(model->rootItem())->clone_helper(false);
    return task;
}

void AceTreeJournalBackendPrivate::updateStackSize() {
    // if (stack.size() > 3 * maxSteps) {
    if (current <= maxSteps / 2) {
        int size = stack.size() - 2 * maxSteps - 1;

        if (size > 0) {
            // Abort forward transactions reading task
            abortForwardReadTask();

            // Remove tail
            removeEvents(2 * maxSteps + 1, stack.size());

            qDebug().noquote().nospace()
                << "[Journal] Remove forward transactions, size=" << size << ", min=" << min
                << ", current=" << current << ", stack_size=" << stack.size();
        }

    } else if (current > maxSteps * 2.5) {
        // Abort backward transactions reading task
        abortBackwardReadTask();

        // Remove head
        removeEvents(0, maxSteps);
        min += maxSteps;
        current -= maxSteps;

        qDebug().noquote().nospace()
            << "[Journal] Remove backward transactions, size=" << maxSteps << ", min=" << min
            << ", current=" << current << ", stack_size=" << stack.size();
    }
    // }
}

void AceTreeJournalBackendPrivate::afterModelInfoSet() {
    auto task = new Tasks::UpdateModelInfoTask();
    task->info = modelInfo;
    pushTask(task);
}

void AceTreeJournalBackendPrivate::afterCommit(const QList<AceTreeEvent *> &events,
                                               const QHash<QString, QString> &attributes) {
    // Update fsMax
    fsMax = min + current;

    // Update fsMin
    int expectMin;
    if (maxCheckPoints >= 0 &&
        (expectMin = ((fsMax - 1) / maxSteps - (maxCheckPoints + 3)) * maxSteps) > fsMin) {
        fsMin = expectMin;
    }

    // Abort forward transactions reading task
    abortForwardReadTask();

    // Abort backward transactions reading task
    if (current > maxSteps * 1.5)
        abortBackwardReadTask();

    // Delete formal checkpoint task
    auto rem = stack.size() % maxSteps;
    if (rem == 0) {
        delete writeCkptTask;

        // Save checkpoint task
        writeCkptTask = genWriteCkptTask();
    } else if (rem == 1 && stack.size() > 1) {
        // Deferred push checkpoint task
        if (writeCkptTask) {
            pushTask(writeCkptTask);
            writeCkptTask = nullptr;
        }
    } else {
        if (writeCkptTask) {
            delete writeCkptTask;
            writeCkptTask = nullptr;
        }
    }

    // Add commit task (Must do it after writing checkpoint)
    {
        QVector<Operations::BaseOp *> ops;
        ops.reserve(events.size());
        for (const auto &event : events) {
            ops.append(Operations::toOp(event));
        }
        auto task = new Tasks::CommitTask();
        task->data = {std::move(ops), attributes};
        task->fsStep = fsMax;
        task->fsMin = fsMin;
        pushTask(task);
    }

    updateStackSize();
}

void AceTreeJournalBackendPrivate::afterReset() {
    AceTreeMemBackendPrivate::afterReset();

    fsMin = 0;
    fsMax = 0;

    if (writeCkptTask) {
        delete writeCkptTask;
        writeCkptTask = nullptr;
    }

    abortForwardReadTask();
    abortBackwardReadTask();

    auto task = new Tasks::BaseTask(Tasks::Reset);
    pushTask(task);
}

void AceTreeJournalBackendPrivate::abortBackwardReadTask() {
    if (!backward_buf) {
        return;
    }
    std::unique_lock<std::mutex> lock(backward_buf->mtx);
    if (backward_buf->finished) {
        delete backward_buf;
        backward_buf = nullptr;
    } else {
        backward_buf->obsolete = true;
        backward_buf = nullptr;
    }
}

void AceTreeJournalBackendPrivate::abortForwardReadTask() {
    if (!forward_buf) {
        return;
    }
    std::unique_lock<std::mutex> lock(forward_buf->mtx);
    if (forward_buf->finished) {
        delete forward_buf;
        forward_buf = nullptr;
    } else {
        forward_buf->obsolete = true;
        forward_buf = nullptr;
    }
}

void AceTreeJournalBackendPrivate::extractBackwardJournal(QVector<AceTreeItem *> &removedItems,
                                                          QVector<Tasks::OpsAndAttrs> &data) {
    int size = data.size();

    // Extrack transactions
    QVector<TransactionData> stack1;
    stack1.reserve(size);

    auto model_p = AceTreeModelPrivate::get(model);

    // Add removed items to model
    for (const auto &item : qAsConst(removedItems)) {
        model_p->addManagedItem_backend(item);
    }

    // Get ownership
    removedItems.clear();

    for (const auto &item : qAsConst(data)) {
        TransactionData tx;
        tx.events.reserve(item.operations.size());
        for (const auto &op : item.operations) {
            auto e = Operations::fromOp(op, model, true);
            tx.events.append(e);
        }
        tx.attrs = item.attributes;
        stack1.append(tx);
    }

    stack = stack1 + stack;
    min -= size;
    current += size;
}

void AceTreeJournalBackendPrivate::extractForwardJournal(QVector<Tasks::OpsAndAttrs> &data) {
    auto size = data.size();

    // Extrack transactions
    QVector<TransactionData> stack1;
    stack1.reserve(size);

    for (const auto &item : qAsConst(data)) {
        TransactionData tx;
        tx.events.reserve(item.operations.size());
        for (const auto &op : item.operations) {
            auto e = Operations::fromOp(op, model, false);
            tx.events.append(e);
        }
        tx.attrs = item.attributes;
        stack1.append(tx);
    }

    stack += stack1;
}

void AceTreeJournalBackendPrivate::afterCurrentChange() {
    Q_Q(AceTreeJournalBackend);

    // Push step updating task
    {
        auto task = new Tasks::ChangeStepTask();
        task->fsStep = q->current();
        pushTask(task);
    }

    // Check if backward transactions is enough to undo
    if (current <= maxSteps / 2) {

        if (fsMin < min) {
            // Abort forward transactions reading task
            abortForwardReadTask();

            // Need to read backward transactions from file system
            if (!backward_buf) {
                int num = min / maxSteps - 1;

                auto task = new Tasks::ReadCkptTask();
                task->num = num;

                // Allocate buffer
                backward_buf = new CheckPointTaskBuffer();
                backward_buf->brief = true;
                task->buf = backward_buf;

                pushTask(task);
            }

            // Need to wait until the reading task finished
            if (current == 0) {
                std::unique_lock<std::mutex> lock(backward_buf->mtx);
                while (!backward_buf->finished) {
                    backward_buf->cv.wait(lock);
                }
            }

            // Insert backward transactions
            if (backward_buf && backward_buf->finished) {
                qDebug().noquote().nospace() << "[Journal] Prepend backward transactions, size="
                                             << backward_buf->data.size();

                extractBackwardJournal(backward_buf->removedItems, backward_buf->data);
                delete backward_buf;
                backward_buf = nullptr;
            }
        }

    }

    // Check if forward transactions is enough to redo
    else if (current > stack.size() - maxSteps / 2) {

        if (fsMax > min + stack.size()) {
            // Abort backward transactions reading task
            abortBackwardReadTask();

            // Need to read backward transactions from file system
            if (!forward_buf) {
                int num = (min + stack.size()) / maxSteps;

                auto task = new Tasks::ReadCkptTask();
                task->num = num;

                // Allocate buffer
                forward_buf = new CheckPointTaskBuffer();
                task->buf = forward_buf;

                pushTask(task);
            }

            // Need to wait until the reading task finished
            if (current == stack.size() - 1) {
                std::unique_lock<std::mutex> lock(forward_buf->mtx);
                while (!forward_buf->finished) {
                    forward_buf->cv.wait(lock);
                }
            }

            // Insert forward transactions
            if (forward_buf && forward_buf->finished) {
                qDebug().noquote().nospace()
                    << "[Journal] Append forward transactions, size=" << forward_buf->data.size();

                // Extrack transactions
                extractForwardJournal(forward_buf->data);
                delete forward_buf;
                forward_buf = nullptr;
            }
        }
    }

    // Over
    updateStackSize();
}

/* Steps data (model_steps.dat)
 *
 * 0x0          maxSteps in a checkpoint
 * 0x4          maxCheckpoints
 * 0x8          min step in log
 * 0xC          max step in log
 * 0x10         current step
 *
 */

/* Checkpoint data (ckpt_XXX.dat)
 *
 * 0x0          CKPT
 * 0x4          removed items pos
 * 0xC          root id (0 if null)
 * 0x14         root data
 * 0xN          removed items size
 * 0xN+4        removed items data
 *
 */

/* Transaction data (journal_XXX.dat)
 *
 * 0x8              max entries
 * 0x10             entry 1
 * 0x18             entry 2
 * ...
 * 8*maxStep        entry maxStep
 * 8*maxStep+8      0xFFFFFFFFFFFFFFFF
 * 8*maxStep+16     transaction 0 (attributes + operations)
 * ...
 *
 */

void AceTreeJournalBackendPrivate::workerRoutine() {
    Q_Q(AceTreeMemBackend);

    if (!stepFile) {
        stepFile = new QFile(QString("%1/model_step.dat").arg(dir));
    }

    if (!infoFile) {
        infoFile = new QFile(QString("%1/model_info.dat").arg(dir));
    }

    if (!txFile) {
        txFile = new QFile();
    }

    QFile &stepFile = *this->stepFile;
    QFile &infoFile = *this->infoFile;
    QFile &txFile = *this->txFile;
    int &num = txNum;

    // Do initial work
    while (!task_queue.empty()) {
        std::unique_lock<std::mutex> lock(mtx);
        auto cur_task = task_queue.front();
        task_queue.pop_front();
        lock.unlock();

        // Execute task
        switch (cur_task->t) {
            case Tasks::Commit: {
                auto task = static_cast<Tasks::CommitTask *>(cur_task);
                int oldMin = fsMin2;
                int oldMax = fsMax2;

                fsMin2 = task->fsMin;
                fsMax2 = task->fsStep;
                fsStep2 = task->fsStep;

                // Write transaction
                {
                    auto &file = txFile;
                    int num1 = (fsStep2 - 1) / maxSteps;
                    if (!file.isOpen() || num != num1) {
                        num = num1;

                        // Reopen file
                        file.close();
                        file.setFileName(
                            QString("%1/journal_%2.dat").arg(dir, QString::number(num)));

                        auto exists = file.exists();
                        file.open(QIODevice::ReadWrite | QIODevice::Append);

                        // Write initial zeros
                        if (exists) {
                            QByteArray zero((maxSteps + 2) * sizeof(qint64), 0);
                            file.write(zero);
                            file.seek(0);
                        }
                    }

                    QDataStream out(&file);

                    // Get current transaction start pos
                    int cur = (fsStep2 - 1) % maxSteps + 1;
                    if (cur == 1) {
                        file.seek((maxSteps + 2) * sizeof(qint64)); // Data section start
                    } else {
                        file.seek((cur - 1) * sizeof(qint64));      // Previous transaction end
                        qint64 pos;
                        out >> pos;
                        file.seek(pos);
                    }

                    auto &data = task->data;

                    // Write attributes
                    out << data.attributes;

                    // Write operation count
                    out << data.operations.size();

                    // Write operations
                    for (const auto &op : qAsConst(data.operations)) {
                        out << op->c;
                        op->write(out);
                    }

                    // Update count and pos
                    qint64 pos = file.pos();
                    file.seek(0);
                    out << qint64(cur);
                    file.seek(cur * sizeof(qint64));
                    out << pos;
                    out << qint64(-1); // end of pos table

                    // Flush
                    if (file.size() > pos) {
                        file.resize(pos);
                    }
                    file.flush();
                }

                // Write steps (Must do it after writing transaction)
                {
                    auto &file = stepFile;
                    bool exists = file.exists();
                    if (!file.isOpen()) {
                        file.open(QIODevice::ReadWrite | QIODevice::Append);
                    }
                    QDataStream out(&file);
                    if (!exists) {
                        // Write initial values
                        out << maxSteps << maxCheckPoints;
                    } else {
                        file.seek(8);
                    }
                    out << fsMin2 << fsMax2 << fsStep2;
                    file.flush();
                }

                // Truncate
                {
                    // Remove backward logs
                    int oldMinNum = oldMin / maxSteps;
                    int minNum = fsMin2 / maxSteps;
                    for (int i = minNum - 1; i >= oldMinNum; --i) {
                        truncateJournals(dir, i);
                    }

                    // Remove forward logs
                    int oldMaxNum = (oldMax - 1) / maxSteps;
                    int maxNum = (fsMax2 - 1) / maxSteps;
                    for (int i = maxNum + 1; i <= oldMaxNum; ++i) {
                        truncateJournals(dir, i);
                    }
                }

                break;
            }

            case Tasks::ChangeStep: {
                auto task = static_cast<Tasks::ChangeStepTask *>(cur_task);
                fsStep2 = task->fsStep;

                // Write step
                {
                    auto &file = stepFile;
                    if (!file.isOpen())
                        file.open(QIODevice::ReadWrite | QIODevice::Append);

                    QDataStream out(&file);
                    file.seek(16);
                    out << fsStep2;
                    file.flush();
                }

                break;
            }

            case Tasks::ReadCheckPoint: {
                auto task = static_cast<Tasks::ReadCkptTask *>(cur_task);
                auto buf = reinterpret_cast<CheckPointTaskBuffer *>(task->buf);

                if (buf->obsolete) {
                    delete buf;
                    break;
                }

                // Read checkpoint
                if (buf->brief) {
                    QFile file(QString("%1/ckpt_%2.dat").arg(dir, QString::number(task->num + 1)));
                    file.open(QIODevice::ReadOnly);
                    readCheckPoint(file, nullptr, &buf->removedItems);
                }

                if (buf->obsolete) {
                    delete buf;
                    break;
                }

                // Read transactions
                {
                    QFile file(QString("%1/journal_%2.dat").arg(dir, QString::number(task->num)));
                    file.open(QIODevice::ReadOnly);
                    readJournal(file, maxSteps, buf->data, buf->brief);
                }

                std::unique_lock<std::mutex> lock(buf->mtx);
                if (buf->obsolete) {
                    lock.unlock();
                    delete buf;
                    break;
                }

                buf->finished = true;
                lock.unlock();
                buf->cv.notify_all();

                break;
            }

            case Tasks::WriteCheckPoint: {
                auto task = static_cast<Tasks::WriteCkptTask *>(cur_task);

                QFile file(QString("%1/ckpt_%2.dat").arg(dir, QString::number(task->num)));
                file.open(QIODevice::ReadWrite);
                writeCheckPoint(file, task->root, task->removedItems);
                break;
            }

            case Tasks::UpdateModelInfo: {
                auto task = static_cast<Tasks::UpdateModelInfoTask *>(cur_task);

                // Write info
                {
                    auto &file = infoFile;
                    if (!file.isOpen())
                        file.open(QIODevice::ReadWrite);
                    file.seek(0);
                    QDataStream out(&file);
                    out << task->info;

                    auto pos = file.pos();
                    if (file.size() > pos) {
                        file.resize(pos);
                    }
                    file.flush();
                }

                break;
            }

            case Tasks::ReadAttributes: {
                auto task = static_cast<Tasks::ReadAttributesTask *>(cur_task);
                auto buf = reinterpret_cast<AttributesTaskBuffer *>(task->buf);

                std::unique_lock<std::mutex> lock(buf->mtx);
                buf->res = fs_getAttributes_do(task->step);
                lock.unlock();
                buf->cv.notify_all();
            }

            case Tasks::Reset: {
                // Remove all files
                infoFile.remove();

                int oldMin = fsMin2;
                int oldMax = fsMax2;

                fsMin2 = 0;
                fsMax2 = 0;
                fsStep2 = 0;
                txNum = -1;

                // Write steps
                {
                    auto &file = stepFile;
                    bool exists = file.exists();
                    if (!file.isOpen()) {
                        file.open(QIODevice::ReadWrite | QIODevice::Append);
                    }
                    QDataStream out(&file);
                    if (!exists) {
                        // Write initial values
                        out << maxSteps << maxCheckPoints;
                    } else {
                        file.seek(8);
                    }
                    out << fsMin2 << fsMax2 << fsStep2;
                    file.flush();
                }

                // Truncate
                {
                    if (this->txFile) {
                        delete this->txFile;
                    }
                    int oldMinNum = oldMin / maxSteps;
                    int oldMaxNum = (oldMax - 1) / maxSteps;
                    for (int i = oldMaxNum; i >= oldMinNum; --i) {
                        truncateJournals(dir, i);
                    }
                }
            }

            default:
                break;
        }

        delete cur_task;
    }

    // This deferred task will be skipped if instance is deleted
    QTimer::singleShot(0, q, [this]() {
        worker->join();
        delete worker;
        worker = nullptr;
    });
}

void AceTreeJournalBackendPrivate::pushTask(Tasks::BaseTask *task, bool unshift) {
    std::unique_lock<std::mutex> lock(mtx);
    if (unshift)
        task_queue.push_front(task);
    else
        task_queue.push_back(task);
    lock.unlock();
    if (!worker) {
        worker = new std::thread(&AceTreeJournalBackendPrivate::workerRoutine, this);
    }

    qDebug() << "[Journal] Push task" << task->t;
}

AceTreeJournalBackend::AceTreeJournalBackend(QObject *parent)
    : AceTreeJournalBackend(*new AceTreeJournalBackendPrivate(), parent) {
}

AceTreeJournalBackend::~AceTreeJournalBackend() {
}

void AceTreeJournalBackend::setMaxReservedSteps(int steps) {
    Q_D(AceTreeJournalBackend);
    if (d->recoverData) {
        return;
    }
    AceTreeMemBackend::setMaxReservedSteps(steps);
}

int AceTreeJournalBackend::reservedCheckPoints() const {
    Q_D(const AceTreeJournalBackend);
    return d->maxCheckPoints;
}

void AceTreeJournalBackend::setReservedCheckPoints(int n) {
    Q_D(AceTreeJournalBackend);
    if (d->recoverData) {
        return;
    }

    if (d->model) {
        return; // Not allowed to change after setup
    }
    d->maxCheckPoints = n;
}

bool AceTreeJournalBackend::start(const QString &dir) {
    Q_D(AceTreeJournalBackend);
    QFileInfo info(dir);
    if (!info.isDir() || !info.isWritable()) {
        myWarning(__func__) << dir << "is not writable";
        return false;
    }

    if (d->recoverData) {
        delete d->recoverData;
        d->recoverData = nullptr;
    }
    d->dir = dir;

    return true;
}

bool AceTreeJournalBackend::recover(const QString &dir) {
    Q_D(AceTreeJournalBackend);

    {
        QFileInfo info(dir);
        if (!info.isDir() || !info.isWritable()) {
            myWarning(__func__) << dir << "is not writable";
            return false;
        }
    }

    // Read steps
    int maxSteps, maxCheckPoints, fsMin, fsMax, fsStep;
    auto readSteps = [&](QFile &file) {
        QDataStream in(&file);
        in >> maxSteps >> maxCheckPoints >> fsMin >> fsMax >> fsStep;
        if (in.status() != QDataStream::Ok) {
            return false;
        }
        return true;
    };
    {
        QFile file(QString("%1/model_step.dat").arg(dir));
        if (!file.open(QIODevice::ReadOnly) || !readSteps(file)) {
            myWarning(__func__) << "read model_step.dat failed";
            return false;
        }
    }

    // Unhandled inconsistency (Merely impossible)
    // 1. Crash during updating steps

    // Fix possible inconsistency:
    // 1. Crash during writing commited transaction or writing checkpoint,
    //    before updating steps
    {
        qint64 expected = fsMax % maxSteps;
        if (expected > 0) {
            int num = (fsMax - 1) / maxSteps;
            QFile file(QString("%1/journal_%2.dat").arg(dir, QString::number(num)));
            if (!file.open(QIODevice::ReadWrite)) {
                myWarning(__func__).noquote()
                    << QString("read journal_%1.dat failed").arg(QString::number(num));
                return false;
            }

            QDataStream in(&file);
            qint64 cur;
            in >> cur;

            if (cur != expected) {
                qDebug().noquote().nospace() << "[Journal] Journal step inconsistent, expected "
                                             << expected << ", actual " << cur;

                file.seek(0);
                in << expected;

                // Truncate
                file.seek(cur * sizeof(qint64)); // Transaction end
                qint64 pos;
                in >> pos;
                in << qint64(-1);
                file.resize(pos);
            }
        } else {
            // The first transaction may failed to be flushed
            truncateJournals(dir, fsMax / maxSteps);
        }
    }

    // 2. Crash during truncating logs
    {
        // Remove backward logs
        int minNum = fsMin / maxSteps;
        for (int i = minNum - 1; i >= 0; --i) {
            if (!truncateJournals(dir, i))
                break;
        }

        // Remove forward logs
        int maxNum = (fsMax - 1) / maxSteps;
        for (int i = maxNum + 1;; ++i) {
            if (!truncateJournals(dir, i))
                break;
        }
    }

    {
        QFile file(QString("%1/model_info.dat").arg(dir));
        if (file.open(QIODevice::ReadOnly)) {
            QDataStream in(&file);
            QVariantHash modelInfo;
            in >> modelInfo;
            if (in.status() == QDataStream::Ok) {
                d->modelInfo = modelInfo;
            }
        }
    }

    if (fsMax == 0) {
        d->dir = dir;
        return true;
    }

    // Get nearest checkpoint
    // Suppose maxSteps = 100
    // 1. 51 <= fsStep <= 150                         -> num = 1
    // 2. 151 <= fsStep <= 250                        -> num = 2
    // 3. fsStep >= 151, fsMin = 100, fsMax <= 200    -> num = 1

    int minNum = fsMin / maxSteps;
    int maxNum = qMax((fsMax - 1) / maxSteps, 0);
    int num = qMin((fsStep + maxSteps / 2 - 1) / maxSteps, maxNum);

    qDebug().noquote().nospace() << "[Journal] Restore, (min, max, cur)=(" << fsMin << ", " << fsMax
                                 << ", " << fsStep << "), select checkpoint " << num;

    AceTreeItem *root = nullptr;
    QVector<AceTreeItem *> removedItems;
    QVector<Tasks::OpsAndAttrs> backwardData;
    QVector<Tasks::OpsAndAttrs> forwardData;
    QVariantHash modelInfo;

    if (num > 0) {
        bool needBackward = num > minNum;

        // Read checkpoint
        {
            QFile file(QString("%1/ckpt_%2.dat").arg(dir, QString::number(num)));
            if (!file.open(QIODevice::ReadOnly) ||
                !d->readCheckPoint(file, &root, needBackward ? &removedItems : nullptr)) {
                myWarning(__func__).noquote()
                    << QString("read ckpt_%1.dat failed").arg(QString::number(num));
                return false;
            }
        }

        // Read backward transactions
        if (needBackward) {
            qDebug().noquote().nospace() << "[Journal] Restore backward transactions " << num - 1;

            QFile file(QString("%1/journal_%2.dat").arg(dir, QString::number(num - 1)));
            if (!file.open(QIODevice::ReadOnly) ||
                !d->readJournal(file, maxSteps, backwardData, true)) {
                myWarning(__func__).noquote()
                    << QString("read journal_%1.dat failed").arg(QString::number(num - 1));
                goto failed;
            }
        }
    }

    if (true) {
        qDebug().noquote().nospace() << "[Journal] Restore forward transactions " << num;

        // Read forward transactions
        QFile file(QString("%1/journal_%2.dat").arg(dir, QString::number(num)));
        if (!file.open(QIODevice::ReadOnly) ||
            !d->readJournal(file, maxSteps, forwardData, false)) {
            myWarning(__func__).noquote()
                << QString("read journal_%1.dat failed").arg(QString::number(num));
            goto failed;
        }
    }

    goto success;

failed:
    delete root;
    qDeleteAll(removedItems);
    return false;

success:
    auto rdata = new AceTreeJournalBackendPrivate::RecoverData();
    rdata->fsMin = fsMin;
    rdata->fsMax = fsMax;
    rdata->fsStep = fsStep;
    rdata->currentNum = num;
    rdata->root = root;
    rdata->removedItems = removedItems;
    rdata->backwardData = backwardData;
    rdata->forwardData = forwardData;
    d->maxSteps = maxSteps;
    d->maxCheckPoints = maxCheckPoints;
    d->recoverData = rdata;
    d->dir = dir;
    return true;
}

void AceTreeJournalBackend::setup(AceTreeModel *model) {
    Q_D(AceTreeJournalBackend);
    d->model = model;
    d->setup_helper();
}

int AceTreeJournalBackend::min() const {
    Q_D(const AceTreeJournalBackend);
    return d->fsMin;
}

int AceTreeJournalBackend::max() const {
    Q_D(const AceTreeJournalBackend);
    return d->fsMax;
}

QHash<QString, QString> AceTreeJournalBackend::attributes(int step) const {
    Q_D(const AceTreeJournalBackend);
    if (step <= d->fsMin || step > d->fsMax) {
        return {};
    }

    int step2 = step - (d->min + 1);
    if (step2 < 0 || step2 >= d->stack.size()) {
        return d->fs_getAttributes(step);
    }
    return d->stack.at(step2).attrs;
}

AceTreeJournalBackend::AceTreeJournalBackend(AceTreeJournalBackendPrivate &d, QObject *parent)
    : AceTreeMemBackend(d, parent) {
    d.init();
}

AceTreeJournalBackendPrivate::RecoverData::~RecoverData() {
    delete root;
    qDeleteAll(removedItems);
    for (const auto &item : qAsConst(backwardData)) {
        qDeleteAll(item.operations);
    }
    for (const auto &item : qAsConst(forwardData)) {
        qDeleteAll(item.operations);
    }
}

AceTreeJournalBackendPrivate::CheckPointTaskBuffer::~CheckPointTaskBuffer() {
    qDeleteAll(removedItems);
    for (const auto &item : qAsConst(data)) {
        qDeleteAll(item.operations);
    }
}
