#ifndef ACETREEBACKEND_H
#define ACETREEBACKEND_H

#include "AceTreeModel.h"

class AceTreeBackendPrivate;

class ACETREE_EXPORT AceTreeBackend : public QObject {
    Q_OBJECT
    Q_DECLARE_PRIVATE(AceTreeBackend)
public:
    explicit AceTreeBackend(QObject *parent = nullptr);
    ~AceTreeBackend();

public:
    virtual void setup(AceTreeModel *model) = 0;

    virtual QVariantHash modelInfo() const;
    virtual void setModelInfo(const QVariantHash &info);

    virtual int min() const = 0;
    virtual int max() const = 0;
    virtual int current() const = 0;
    virtual QHash<QString, QString> attributes(int step) const = 0;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual void commit(const QList<AceTreeEvent *> &events,
                        const QHash<QString, QString> &attrs) = 0;

    virtual void reset() = 0;

    inline bool canUndo() const;
    inline bool canRedo() const;
};

inline bool AceTreeBackend::canUndo() const {
    return current() > 0;
}

inline bool AceTreeBackend::canRedo() const {
    return current() < max();
}

#endif // ACETREEBACKEND_H
