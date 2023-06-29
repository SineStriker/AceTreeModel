#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStack>
#include <QTextStream>

#include <thread>

#include <AceTreeJournalBackend.h>
#include <AceTreeModel.h>

static AceTreeModel *model;

static const char *help_display[] = {
    "Control Commands:",
    "    help                               Show this help",
    "    exit/quit                          Exit",
    "    cls/clear                          Clear screen",
    "",
    "Model Commands:",
    "    new                                Create new item (Return negative temp id)",
    "    del    <id>                        Remove item (Negative id only)",
    "    show   <id>                        Show item",
    "    rid                                Show root id",
    "    temp                               Show all temp items' id",
    "    steps                              Show step information",
    "    attr                               Show step attributes",
    "    undo                               Undo",
    "    redo                               Redo",
    "    reset                              Reset",
    "",
    "    set    <id> <key> <value>          Set property",
    "    get    <id> <key>                  Get property",
    "",
    "    repb   <id> <index> <bytes...>     Replace bytes",
    "    insb   <id> <index> <bytes...>     Insert bytes",
    "    rmb    <id> <index> <count>        Remove bytes",
    "    bcnt   <id>                        Get bytes count",
    "    bts    <id>                        Get bytes",
    "",
    "    insr   <id> <index> <id...>        Insert rows",
    "    ppdr   <id> <id...>                Prepend rows",
    "    apdr   <id> <id...>                Append rows",
    "    rmr    <id> <index> <count>        Remove rows",
    "    rcnt   <id>                        Get row count",
    "    rs     <id>                        Get row ids",
    "    rf     <id> <id>                   Find row",
    "",
    "    insa   <id> <id>                   Add record (Return seq)",
    "    rma    <id> <seq>                  Remove record",
    "    acnt   <id>                        Get record count",
    "    as     <id>                        Get record seqs",
    "    af     <id> <id>                   Find seq",
    "",
    "    inse   <id> <key> <id>             Add element",
    "    rme    <id> <key>                  Remove element",
    "    ecnt   <id>                        Get element count",
    "    es     <id>                        Get element keys",
    "    ef     <id> <id>                   Find element",
    "",
    "    setr   <id>                        Set root item",
};

static QStringList parseCommand(const QString &command) {
    QStringList result;

    QString currentToken;
    bool inQuotes = false;

    for (int i = 0; i < command.length(); ++i) {
        QChar currentChar = command.at(i);

        if (currentChar == '\"') {
            inQuotes = !inQuotes;
        } else if (currentChar == ' ' && !inQuotes) {
            if (!currentToken.isEmpty()) {
                result.append(currentToken);
                currentToken.clear();
            }
        } else {
            currentToken.append(currentChar);
        }
    }

    if (!currentToken.isEmpty()) {
        result.append(currentToken);
    }

    return result;
}


static void showItem(AceTreeItem *item, int indent, const char *prefix = nullptr) {
    std::string indent_str(indent, ' ');
    if (prefix) {
        qDebug().noquote().nospace() << indent_str.c_str() << prefix;
    }
    qDebug().noquote().nospace() << indent_str.c_str() << "id: " << item->index();
    qDebug().noquote().nospace() << indent_str.c_str() << "properties: ";
    auto map = item->propertyMap();
    for (auto it = map.begin(); it != map.end(); ++it) {
        qDebug().nospace().noquote()
            << indent_str.c_str() << "  " << it.key() << ": " << it.value();
    }

    indent += 2;
    qDebug().noquote().nospace() << indent_str.c_str() << "rows: ";
    auto rows = item->rows();
    int i = 0;
    for (const auto &child : qAsConst(rows)) {
        showItem(child, indent,
                 QString("index: %1").arg(QString::number(i++)).toStdString().c_str());
        qDebug().noquote().nospace() << indent_str.c_str() << "  --------";
    }
    qDebug().noquote().nospace() << indent_str.c_str() << "records: ";
    auto recs = item->recordMap();
    for (auto it = recs.begin(); it != recs.end(); ++it) {
        showItem(it.value(), indent,
                 QString("seq: %1").arg(QString::number(it.key())).toStdString().c_str());
        qDebug().noquote().nospace() << indent_str.c_str() << "  --------";
    }
    qDebug().noquote().nospace() << indent_str.c_str() << "elements: ";
    auto eles = item->elementMap();
    for (auto it = eles.begin(); it != eles.end(); ++it) {
        showItem(it.value(), indent, QString("key: %1").arg(it.key()).toStdString().c_str());
        qDebug().noquote().nospace() << indent_str.c_str() << "  --------";
    }
}


static void cli() {
    QTextStream inputStream(stdin);
    QString line;

    QMap<int, AceTreeItem *> tempItems;
    int maxTempId = -1;

    auto getItem = [&](int id) {
        AceTreeItem *item;
        if (id < 0) {
            item = tempItems.value(id);
        } else {
            item = model->itemFromIndex(id);
        }
        return item;
    };

    auto filterItems = [&tempItems]() {
        for (auto it = tempItems.begin(); it != tempItems.end();) {
            if (it.value()->index() > 0)
                it = tempItems.erase(it);
            else
                ++it;
        }
    };

    do {
        printf("> ");

        line = inputStream.readLine();

        auto list = parseCommand(line);
        if (list.isEmpty()) {
            continue;
        }

        auto cmd = list.front();

#define ENSURE_SIZE(count)                                                                         \
    if (list.size() != count) {                                                                    \
        qDebug() << "Invalid use of command" << cmd;                                               \
        continue;                                                                                  \
    }

#define GET_ITEM(id)                                                                               \
    auto item = getItem(id);                                                                       \
    if (!item) {                                                                                   \
        qDebug() << "Item" << id << "not found";                                                   \
        continue;                                                                                  \
    }

        // Help
        if (cmd == "help") {
            for (const auto &s : help_display) {
                qDebug() << s;
            }
            continue;
        }

        // Exit
        if (cmd == "quit" || cmd == "exit") {
            qApp->quit();
            break;
        }

        // Clear
        if (cmd == "clear" || cmd == "cls") {
#ifdef Q_OS_WINDOWS
            system("cls");
#else
            system("clear");
#endif
            continue;
        }

        // New
        if (cmd == "new") {
            auto item = new AceTreeItem();
            auto id = maxTempId--;
            tempItems.insert(id, item);
            item->setDynamicData("_temp_id", id);
            qDebug() << "Create new item" << id;
            continue;
        }

        // Del
        if (cmd == "del") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            auto it = tempItems.find(id);
            if (it == tempItems.end()) {
                qDebug() << "Item" << id << "not found";
                continue;
            }
            delete it.value();
            tempItems.erase(it);
            qDebug() << "OK";
            continue;
        }

        // Show
        if (cmd == "show") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            showItem(item, 0);
            continue;
        }

        // rid
        if (cmd == "rid") {
            qDebug() << (model->rootItem() ? model->rootItem()->index() : 0);
            continue;
        }

        // Temp
        if (cmd == "temp") {
            QStringList ids;
            ids.reserve(tempItems.size());
            for (auto it = tempItems.end() - 1; it != tempItems.begin() - 1; --it) {
                ids.append(QString::number(it.key()));
            }
            qDebug().noquote() << ids.join(", ");
            continue;
        }

        // steps
        if (cmd == "steps") {
            qDebug() << "Max:" << model->maxStep();
            qDebug() << "Min:" << model->minStep();
            qDebug() << "Current:" << model->currentStep();
            continue;
        }

        // attr
        if (cmd == "attr") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            qDebug() << model->stepAttributes(id);
            continue;
        }

        // Undo
        if (cmd == "undo") {
            model->previousStep();
            continue;
        }

        // Redo
        if (cmd == "redo") {
            model->nextStep();
            continue;
        }

        // Redo
        if (cmd == "reset") {
            model->reset();
            continue;
        }

        auto tx = [](bool tx, const std::function<bool()> &func) {
            if (tx)
                model->beginTransaction();

            bool res = func();
            if (res) {
                qDebug() << "OK";
            } else {
                qDebug() << "Failed";
            }
            if (tx)
                model->commitTransaction({{"num", QString::number(model->maxStep())}});

            return res;
        };

        // Set
        if (cmd == "set") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto key = list.at(2);
            auto val = list.at(3);

            tx(item->index() > 0, [&]() { return item->setProperty(key, val); });
            continue;
        }

        // Get
        if (cmd == "get") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto key = list.at(2);
            qDebug() << item->property(key);
            continue;
        }

        // repb
        if (cmd == "repb") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto index = list.at(2).toInt();
            auto bytes = list.at(3).toUtf8();
            tx(item->index() > 0, [&]() { return item->replaceBytes(index, bytes); });
            continue;
        }

        // insb
        if (cmd == "insb") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto index = list.at(2).toInt();
            auto bytes = list.at(3).toUtf8();
            tx(item->index() > 0, [&]() { return item->insertBytes(index, bytes); });
            continue;
        }

        // rmb
        if (cmd == "rmb") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto index = list.at(2).toInt();
            auto count = list.at(3).toInt();
            tx(item->index() > 0, [&]() { return item->removeBytes(index, count); });
            continue;
        }

        // bcnt
        if (cmd == "bcnt") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug() << item->bytesSize();
            continue;
        }

        // bts
        if (cmd == "bts") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug() << item->bytes();
            continue;
        }

        auto getChildren = [getItem](const QStringList &list) -> QVector<AceTreeItem *> {
            QVector<AceTreeItem *> children;
            children.reserve(list.size());
            for (int i = 0; i < list.size(); ++i) {
                auto id = list.at(i).toInt();
                auto child = getItem(id);
                if (!child) {
                    qDebug() << "Item" << id << "not found";
                    return {};
                }
                children.append(child);
            }
            return children;
        };

        // insr
        if (cmd == "insr") {
            ENSURE_SIZE(4);

            auto id = list.at(1).toInt();
            GET_ITEM(id);
            auto index = list.at(2).toInt();
            auto children = getChildren(list.mid(3));
            if (children.isEmpty())
                continue;

            if (tx(item->index() > 0, [&]() { return item->insertRows(index, children); })) {
                filterItems();
            }
            continue;
        }

        // apdr
        if (cmd == "apdr") {
            ENSURE_SIZE(3);

            auto id = list.at(1).toInt();
            GET_ITEM(id);
            auto children = getChildren(list.mid(2));
            if (children.isEmpty())
                continue;

            if (tx(item->index() > 0, [&]() { return item->appendRows(children); })) {
                filterItems();
            }
            continue;
        }

        // ppdr
        if (cmd == "ppdr") {
            ENSURE_SIZE(3);

            auto id = list.at(1).toInt();
            GET_ITEM(id);
            auto children = getChildren(list.mid(2));
            if (children.isEmpty())
                continue;

            if (tx(item->index() > 0, [&]() { return item->prependRows(children); })) {
                filterItems();
            }
            continue;
        }

        // rmr
        if (cmd == "rmr") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto index = list.at(2).toInt();
            auto count = list.at(3).toInt();
            tx(item->index() > 0, [&]() { return item->removeRows(index, count); });
            continue;
        }

        // rcnt
        if (cmd == "rcnt") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug() << item->rowCount();
            continue;
        }

        // rs
        if (cmd == "rs") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            QStringList ids;
            ids.reserve(item->rowCount());
            const auto &rows = item->rows();
            for (const auto &i : rows) {
                auto idx = i->index();
                if (idx == 0) {
                    idx = item->dynamicData("_temp_id").toInt();
                }
                ids.append(QString::number(idx));
            }
            qDebug().noquote() << ids.join(", ");
            continue;
        }

        // rf
        if (cmd == "rf") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto id2 = list.at(2).toInt();
            auto child = getItem(id2);
            if (!child) {
                qDebug() << "Item" << id2 << "not found";
                continue;
            }

            qDebug() << item->rowIndexOf(child);
            continue;
        }

        // insa
        if (cmd == "insa") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto id2 = list.at(2).toInt();
            auto child = getItem(id2);
            if (!child) {
                qDebug() << "Item" << id2 << "not found";
                continue;
            }

            if (item->index() > 0)
                model->beginTransaction();

            auto seq = item->addRecord(child);
            if (seq > 0) {
                filterItems();
            }
            qDebug() << seq;

            if (item->index() > 0)
                model->commitTransaction();
            continue;
        }

        // rma
        if (cmd == "rma") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto seq = list.at(2).toInt();
            tx(item->index() > 0, [&]() { return item->removeRecord(seq); });
            continue;
        }

        // acnt
        if (cmd == "acnt") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug() << item->recordCount();
            continue;
        }

        // as
        if (cmd == "as") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            QStringList ids;
            ids.reserve(item->rowCount());
            const auto &seqs = item->records();
            for (const auto &seq : seqs) {
                ids.append(QString::number(seq));
            }
            qDebug().noquote() << ids.join(", ");
            continue;
        }

        // af
        if (cmd == "af") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto id2 = list.at(2).toInt();
            auto child = getItem(id2);
            if (!child) {
                qDebug() << "Item" << id2 << "not found";
                continue;
            }

            qDebug() << item->recordSequenceOf(child);
            continue;
        }

        // inse
        if (cmd == "inse") {
            ENSURE_SIZE(4);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto key = list.at(2);

            auto id2 = list.at(3).toInt();
            auto child = getItem(id2);
            if (!child) {
                qDebug() << "Item" << id2 << "not found";
                continue;
            }

            if (tx(item->index() > 0, [&]() { return item->addElement(key, child); })) {
                filterItems();
            }
            continue;
        }

        // rme
        if (cmd == "rme") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto key = list.at(2);
            tx(item->index() > 0, [&]() { return item->removeElement(key); });
            continue;
        }

        // ecnt
        if (cmd == "ecnt") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug() << item->elementCount();
            continue;
        }

        // es
        if (cmd == "es") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            qDebug().noquote() << item->elementKeys().join(", ");
            continue;
        }

        // ef
        if (cmd == "ef") {
            ENSURE_SIZE(3);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            auto id2 = list.at(2).toInt();
            auto child = getItem(id2);
            if (!child) {
                qDebug() << "Item" << id2 << "not found";
                continue;
            }

            qDebug() << item->elementKeyOf(child);
            continue;
        }

        // setr
        if (cmd == "setr") {
            ENSURE_SIZE(2);
            auto id = list.at(1).toInt();
            GET_ITEM(id);

            if (tx(true, [&]() { return model->setRootItem(item); })) {
                filterItems();
            }
            continue;
        }

        qDebug() << "Unknown command:" << cmd;
    } while (!inputStream.atEnd());
}

int main(int argc, char *argv[]) {
    QCoreApplication a(argc, argv);

    auto backend = new AceTreeJournalBackend();
    auto dir = QDir(QCoreApplication::applicationDirPath() + "/model");
    if (dir.exists()) {
        if (!backend->recover(dir.absolutePath())) {
            backend->start(dir.absolutePath());
        }
    } else {
        dir.mkpath(dir.absolutePath());
        backend->start(dir.absolutePath());
    }

    model = new AceTreeModel(backend);

    std::thread t(cli);
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &a, [&t]() {
        t.join(); // Wait thread to finish
    });

    return a.exec();
}
