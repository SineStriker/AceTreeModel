#include "AceTreeJournalBackend.h"
#include "AceTreeJournalBackend_p.h"

#include "AceTreeItem_p.h"
#include "AceTreeModel_p.h"

#include <QDataStream>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

#define myWarning(func)                                                                            \
    (qWarning().nospace() << "AceTreeJournalBackend::" << (func) << "(): ").maybeSpace()

AceTreeJournalBackendPrivate::AceTreeJournalBackendPrivate() {
    maxCheckPoints = 1;
    worker = nullptr;
    fsMin = fsMax = 0;
    backward_buf = forward_buf = nullptr;
    stepFile = infoFile = txFile = nullptr;
    txNum = -1;
    fsMin2 = fsMax2 = fsStep2 = 0;
}

AceTreeJournalBackendPrivate::~AceTreeJournalBackendPrivate() {
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

QHash<QString, QString> AceTreeJournalBackendPrivate::fs_getAttribute(int step) const {
    Q_UNUSED(step);
    return {};
}

void AceTreeJournalBackendPrivate::afterModelInfoSet() {
    auto task = new Tasks::UpdateModelInfoTask();
    task->info = modelInfo;
    pushTask(task);
}

void AceTreeJournalBackendPrivate::afterCommit(const QList<AceTreeEvent *> &events,
                                               const QHash<QString, QString> &attributes) {
    // Abort forward transactions reading task
    abortForwardReadTask();

    // Update fsMax
    fsMax = min + current;

    // Update fsMin
    int expectMin;
    if (maxCheckPoints >= 0 && (expectMin = fsMax - (maxCheckPoints + 2) * maxSteps) > fsMin) {
        fsMin = expectMin;
    }

    // Add commit task
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

    // Add checkpoint task
    if (stack.size() % maxSteps == 0) {

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
                            removedItems.append(
                                AceTreeItemPrivate::get(child)->clone_helper(false));
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
        pushTask(task);
    }

    updateStackSize();
}

void AceTreeJournalBackendPrivate::abortBackwardReadTask() {
    if (backward_buf) {
        std::unique_lock<std::mutex> lock(backward_buf->mtx);
        if (backward_buf->finished) {
            delete backward_buf;
            backward_buf = nullptr;
        } else {
            backward_buf->obsolete = true;
            backward_buf = nullptr;
        }
    }
}

void AceTreeJournalBackendPrivate::abortForwardReadTask() {
    if (forward_buf) {
        std::unique_lock<std::mutex> lock(forward_buf->mtx);
        if (forward_buf->finished) {
            delete forward_buf;
            forward_buf = nullptr;
        } else {
            forward_buf->obsolete = true;
            forward_buf = nullptr;
        }
    }
}

void AceTreeJournalBackendPrivate::updateStackSize() {
    if (current > 2 * maxSteps) {
        // Remove head
        removeEvents(0, maxSteps);
        min += maxSteps;
        current -= maxSteps;
    } else if (current <= maxSteps && stack.size() > 2 * maxSteps + 1) {
        // Remove tail
        removeEvents(2 * maxSteps + 1, stack.size());
    }
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
    if (fsMin < min && current <= maxSteps / 2) {

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
            int size = backward_buf->data.size();

            // Extrack transactions
            QVector<TransactionData> stack1;
            stack1.reserve(size);

            // Add removed items to model
            for (const auto &item : qAsConst(backward_buf->removedItems)) {
                AceTreeModelPrivate::get(model)->propagate_model(item);
            }

            // Get ownership
            backward_buf->removedItems.clear();

            for (const auto &item : qAsConst(backward_buf->data)) {
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
            delete backward_buf;
            backward_buf = nullptr;

            min -= size;
            current += size;
        }

    }

    // Check if forward transactions is enough to redo
    else if (fsMax > min + stack.size() && current >= maxSteps + maxSteps / 2) {

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
            auto size = forward_buf->data.size();

            // Extrack transactions
            QVector<TransactionData> stack1;
            stack1.reserve(size);

            for (const auto &item : qAsConst(forward_buf->data)) {
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
            delete forward_buf;
            forward_buf = nullptr;
        }
    }

    // Over
    updateStackSize();
}

/* Steps data
 *
 * 0x0          maxSteps in a checkpoint
 * 0x4          min step in log
 * 0x8          max step in log
 * 0xC          current step
 *
 */

/* Checkpoint data
 *
 * 0x0          CKPT
 * 0x4          removed items pos
 * 0xC          root id (0 if null)
 * 0x14         root data
 * 0xN          removed items size
 * 0xN+4        removed items data
 *
 */

/* Transaction data
 *
 * 0x8              max entries
 * 0x10             entry 1
 * 0x18             entry 2
 * ...
 * 8*maxStep        entry maxStep
 * 8*maxStep+8      0xFFFFFFFFFFFFFFFF
 * 8*maxStep+16     transaction 0
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

                    // Write operation count
                    auto &data = task->data;

                    // Write attributes
                    out << data.attributes;

                    // Write operations
                    out << data.operations.size();
                    for (const auto &op : qAsConst(data.operations)) {
                        out << op->c;
                        op->write(out);
                    }

                    qint64 pos = file.pos();

                    // Update count and pos
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

                // Write steps
                {
                    auto &file = stepFile;
                    bool exists = file.exists();
                    if (!file.isOpen()) {
                        file.open(QIODevice::ReadWrite | QIODevice::Append);
                    }
                    QDataStream out(&file);
                    if (!exists) {
                        // Write initial max steps
                        out << maxSteps;
                    } else {
                        file.seek(4);
                    }
                    out << fsMin2 << fsMax2 << fsStep2;
                    file.flush();
                }

                // Truncate
                {
                    auto rm = [this](int i) {
                        QFile::remove(QString("%1/journal_%2.dat").arg(dir, QString::number(i)));
                        QFile::remove(QString("%1/ckpt_%2.dat").arg(dir, QString::number(i)));
                    };

                    // Remove backward logs
                    int oldMinNum = oldMin / maxSteps;
                    int minNum = fsMin2 / maxSteps;
                    for (int i = oldMinNum; i < minNum; ++i) {
                        rm(i);
                    }

                    // Remove forward logs
                    int oldMaxNum = (oldMax - 1) / maxSteps;
                    int maxNum = (fsMax2 - 1) / maxSteps;
                    for (int i = oldMaxNum; i > maxNum; --i) {
                        rm(i);
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
                    file.seek(12);
                    out << fsStep2;
                    file.flush();
                }

                break;
            }

            case Tasks::ReadCheckPoint: {
                auto task = static_cast<Tasks::ReadCkptTask *>(cur_task);
                auto buf = reinterpret_cast<CheckPointTaskBuffer *>(task->buf);

                QVector<AceTreeItem *> removedItems;

                if (buf->obsolete) {
                    delete buf;
                    break;
                }

                // Read checkpoint
                if (buf->brief) {
                    QFile file(QString("%1/ckpt_%2.dat").arg(dir, QString::number(task->num + 1)));
                    file.open(QIODevice::ReadOnly);

                    QDataStream in(&file);
                    in.skipRawData(4);

                    // Read removed items pos
                    size_t offset;
                    in >> offset;
                    in.skipRawData(offset);

                    // Read removed items size
                    int sz;
                    in >> sz;

                    // Read removed items data
                    removedItems.reserve(sz);
                    for (int i = 0; i < sz; ++i) {
                        auto item = AceTreeItemPrivate::read_helper(in, false);
                        removedItems.append(item);
                    }

                    buf->removedItems = std::move(removedItems);
                }

                if (buf->obsolete) {
                    delete buf;
                    break;
                }

                // Read transactions
                {
                    QFile file(QString("%1/journal_%2.dat").arg(dir, QString::number(task->num)));
                    file.open(QIODevice::ReadOnly);

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

                    buf->data.reserve(cnt);
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
                            Operations::Change c;
                            in >> c;

                            Operations::BaseOp *baseOp = nullptr;
                            switch (c) {
                                case Operations::PropertyChange: {
                                    auto op = new Operations::PropertyChangeOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::BytesReplace: {
                                    auto op = new Operations::BytesReplaceOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::BytesInsert: {
                                    auto op = new Operations::BytesInsertRemoveOp(true);
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::BytesRemove: {
                                    auto op = new Operations::BytesInsertRemoveOp(false);
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RowsInsert: {
                                    auto op = new Operations::RowsInsertOp();
                                    buf->brief ? op->readBrief(in) : op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RowsMove: {
                                    auto op = new Operations::RowsMoveOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RowsRemove: {
                                    auto op = new Operations::RowsRemoveOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RecordAdd: {
                                    auto op = new Operations::RecordAddOp();
                                    buf->brief ? op->readBrief(in) : op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RecordRemove: {
                                    auto op = new Operations::RecordRemoveOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::ElementAdd: {
                                    auto op = new Operations::ElementAddOp();
                                    buf->brief ? op->readBrief(in) : op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::ElementRemove: {
                                    auto op = new Operations::ElementRemoveOp();
                                    op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                case Operations::RootChange: {
                                    auto op = new Operations::RootChangeOp();
                                    buf->brief ? op->readBrief(in) : op->read(in);
                                    baseOp = op;
                                    break;
                                }
                                default:
                                    break;
                            }
                            ops.append(baseOp);
                        }

                        buf->data.append({ops, attrs});
                    }
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
                file.write("CKPT", 4);

                QDataStream out(&file);
                out << qint64(0);
                if (task->root) {
                    // Write index
                    out << task->root->index();

                    // Write root data
                    task->root->write(out);
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
                out << (task->removedItems.size());

                // Write removed items data
                for (const auto &item : qAsConst(task->removedItems)) {
                    item->write(out);
                }

                break;
            }

            case Tasks::UpdateModelInfo: {
                auto task = static_cast<Tasks::UpdateModelInfoTask *>(cur_task);

                // Write info
                {
                    auto &file = infoFile;
                    if (!file.isOpen())
                        file.open(QIODevice::ReadWrite | QIODevice::Append);
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

void AceTreeJournalBackendPrivate::pushTask(Tasks::BaseTask *task) {
    std::unique_lock<std::mutex> lock(mtx);
    task_queue.push_back(task);
    lock.unlock();

    if (!worker) {
        worker = new std::thread(&AceTreeJournalBackendPrivate::workerRoutine, this);
    }
}

AceTreeJournalBackend::AceTreeJournalBackend(QObject *parent)
    : AceTreeJournalBackend(*new AceTreeJournalBackendPrivate(), parent) {
}

AceTreeJournalBackend::~AceTreeJournalBackend() {
}

int AceTreeJournalBackend::reservedCheckPoints() const {
    Q_D(const AceTreeJournalBackend);
    return d->maxCheckPoints;
}

void AceTreeJournalBackend::setReservedCheckPoints(int n) {
    Q_D(AceTreeJournalBackend);
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
    d->model = model;
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
    int step2 = step - d->min;
    if (step2 < 0 || step2 >= d->stack.size()) {
        return d->fs_getAttribute(step);
    }
    return d->stack.at(step2).attrs;
}

AceTreeJournalBackend::AceTreeJournalBackend(AceTreeJournalBackendPrivate &d, QObject *parent)
    : AceTreeMemBackend(d, parent) {
    d.init();
}


AceTreeJournalBackendPrivate::CheckPointTaskBuffer::~CheckPointTaskBuffer() {
    qDeleteAll(removedItems);
    for (const auto &item : qAsConst(data)) {
        qDeleteAll(item.operations);
    }
}
