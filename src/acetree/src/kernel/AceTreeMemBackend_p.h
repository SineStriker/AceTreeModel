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

    int maxSteps;

    AceTreeModel *model;
    QVariantHash modelInfo;

    struct TransactionData {
        QList<AceTreeEvent *> events;
        QHash<QString, QString> attrs;
    };
    QVector<TransactionData> stack;
    int min;
    int current;

    void removeEvents(int begin, int end);

    virtual bool acceptChangeMaxSteps(int steps) const;
    virtual void afterModelInfoSet();
    virtual void afterCurrentChange();
    virtual void afterCommit(const QList<AceTreeEvent *> &events,
                             const QHash<QString, QString> &attributes);
    virtual void afterReset();
};

#endif // ACETREEMEMBACKEND_P_H
