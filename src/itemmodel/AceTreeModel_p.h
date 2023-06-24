#ifndef ACETREEMODEL_P_H
#define ACETREEMODEL_P_H

#include <QDebug>
#include <QFileDevice>
#include <QSet>
#include <QStack>

#include <unordered_map>

#include "AceTreeItem_p.h"
#include "AceTreeModel.h"

class AceTreeModelPrivate {
    Q_DECLARE_PUBLIC(AceTreeModel)
public:
    AceTreeModelPrivate();
    ~AceTreeModelPrivate();

    void init();

    AceTreeModel *q_ptr;
    bool is_destruct;

    AceTreeBackend *backend;
    QList<AceTreeEvent *> tx_stack;

    AceTreeModel::State m_state;
    bool m_metaOperation;

    std::unordered_map<size_t, AceTreeItem *> indexes;
    size_t maxIndex;

    AceTreeItem *rootItem;

    void setRootItem_helper(AceTreeItem *item);

    int addIndex(AceTreeItem *item, size_t idx = 0);
    void removeIndex(size_t index);

    void event_helper(AceTreeEvent *e);
    void propagate_model(AceTreeItem *item);

    static AceTreeModelPrivate *get(AceTreeModel *model);

    struct InterruptGuard {
        inline InterruptGuard(AceTreeItem *item)
            : b(item->model() ? &item->model()->d_func()->m_metaOperation : shared_null) {
        }
        inline InterruptGuard(AceTreeItemPrivate *item_d)
            : b(item_d->model ? &item_d->model->d_func()->m_metaOperation : shared_null) {
        }
        inline InterruptGuard(AceTreeModel *model)
            : b(model ? &model->d_func()->m_metaOperation : shared_null) {
            *b = true;
        }
        inline InterruptGuard(AceTreeModelPrivate *model_p)
            : b(model_p ? &model_p->m_metaOperation : shared_null) {
            *b = true;
        }
        inline ~InterruptGuard() {
            *b = false;
        }
        bool *b;
        static bool *shared_null;
    };
};

#endif // ACETREEMODEL_P_H
