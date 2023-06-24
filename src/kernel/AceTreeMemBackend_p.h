#ifndef ACETREEMEMBACKEND_P_H
#define ACETREEMEMBACKEND_P_H

#include "AceTreeMemBackend.h"

class AceTreeMemBackendPrivate {
    Q_DECLARE_PUBLIC(AceTreeMemBackend)
public:
    AceTreeMemBackendPrivate();
    virtual ~AceTreeMemBackendPrivate();

    void init();

    AceTreeMemBackend *q_ptr;

    int maxReserved;

    AceTreeModel *model;
    QVariantHash modelInfo;

    struct TransactionData {
        QList<AceTreeEvent *> events;
        QHash<QString, QString> attrs;
    };
    QList<TransactionData> stack;
    int minStep;
    int step;

    virtual void modelInfoSet();

    virtual void afterChangeStep(int step);
    virtual void afterCommit(const QList<AceTreeEvent *> &events,
                             const QHash<QString, QString> &attributes);

    void removeForwardSteps();
    void removeEarlySteps();
};

#endif // ACETREEMEMBACKEND_P_H
