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

    bool start(const QString &dir);
    bool recover(const QString &dir);

public:
    void setup(AceTreeModel *model) override;

protected:
    AceTreeJournalBackend(AceTreeJournalBackendPrivate &d, QObject *parent = nullptr);
};

#endif // ACETREEJOURNALBACKEND_H
