#ifndef ACETREEMEMBACKEND_H
#define ACETREEMEMBACKEND_H

#include "AceTreeBackend.h"

class AceTreeMemBackendPrivate;

class ACETREE_EXPORT AceTreeMemBackend : public AceTreeBackend {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeMemBackend)
public:
    explicit AceTreeMemBackend(QObject *parent = nullptr);
    ~AceTreeMemBackend();

    int maxReservedSteps() const;
    virtual void setMaxReservedSteps(int steps);

public:
    void setup(AceTreeModel *model) override;

    QVariantHash modelInfo() const override;
    void setModelInfo(const QVariantHash &info) override;

    int min() const override;
    int max() const override;
    int current() const override;
    QHash<QString, QString> attributes(int step) const override;

    void undo() override;
    void redo() override;
    void commit(const QList<AceTreeEvent *> &events, const QHash<QString, QString> &attrs) override;

    void reset() override;

protected:
    AceTreeMemBackend(AceTreeMemBackendPrivate &d, QObject *parent = nullptr);

    QScopedPointer<AceTreeMemBackendPrivate> d_ptr;
};

#endif // ACETREEMEMBACKEND_H
