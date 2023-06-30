#include <QCoreApplication>
#include <QTest>

#include <AceTreeModel.h>

static AceTreeItem *createItem(const QString &name) {
    auto item = new AceTreeItem();
    item->setProperty("name", name);
    return item;
}

class tst_Basic : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void init();

private slots:
    void basic();
};

void tst_Basic::init() {
    // Initialize
}

void tst_Basic::basic() {
    AceTreeModel model;

    auto rootItem = createItem("1");
    rootItem->setProperty("key1", "value1");

    // Step 1 - Test Change Root
    model.beginTransaction();
    model.setRootItem(rootItem);
    model.commitTransaction();

    model.previousStep();
    QVERIFY(!model.rootItem());
    model.nextStep();
    QCOMPARE(model.rootItem(), rootItem);

    // Step 2 - Test Modify Item
    model.beginTransaction();
    rootItem->setProperty("key1", "a");
    rootItem->setProperty("key2", "b");
    rootItem->replaceBytes(0, "12345678");
    model.commitTransaction();

    model.beginTransaction();
    rootItem->insertBytes(4, "0000");
    rootItem->replaceBytes(8, "ABCDEF");
    rootItem->removeBytes(0, 2);
    model.commitTransaction();

    model.previousStep();
    QCOMPARE(rootItem->bytes(), "12345678");
    model.nextStep();
    QCOMPARE(rootItem->bytes(), "340000ABCDEF");
}

QTEST_APPLESS_MAIN(tst_Basic)
#include "tst_Basic.moc"
