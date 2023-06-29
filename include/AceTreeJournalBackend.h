#ifndef ACETREEJOURNALBACKEND_H
#define ACETREEJOURNALBACKEND_H

#include "AceTreeMemBackend.h"

class AceTreeJournalBackendPrivate;

class ACETREE_EXPORT AceTreeJournalBackend : public AceTreeMemBackend {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeJournalBackend)
public:
    explicit AceTreeJournalBackend(QObject *parent = nullptr);
    ~AceTreeJournalBackend();

    void setMaxReservedSteps(int steps) override;

    int reservedCheckPoints() const;
    void setReservedCheckPoints(int n);

    bool start(const QString &dir);
    bool recover(const QString &dir);

    bool switchDir(const QString &dir);

public:
    void setup(AceTreeModel *model) override;

    int min() const override;
    int max() const override;
    QHash<QString, QString> attributes(int step) const override;

protected:
    AceTreeJournalBackend(AceTreeJournalBackendPrivate &d, QObject *parent = nullptr);
};

#endif // ACETREEJOURNALBACKEND_H
