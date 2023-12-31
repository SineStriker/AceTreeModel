#include "AceTreeStandardEntity.h"
#include "AceTreeStandardEntity_p.h"

#include "AceTreeItem_p.h"

static const char KEY_NAME_CHILD_TYPE[] = "ace_tree_child_type";

static int indent = 0;

using GlobalSpecMap = QHash<const QMetaObject *, AceTreeStandardSchema>;
Q_GLOBAL_STATIC(GlobalSpecMap, globalSpecMap)
Q_GLOBAL_STATIC(GlobalSpecMap, globalSpecMapComplete)

static inline void setTypeValueToItem(AceTreeItem *item, const QString &typeValue) {
    item->setProperty(KEY_NAME_CHILD_TYPE, typeValue);
}

static inline QString getTypeValueFromItem(const AceTreeItem *item) {
    return item->property(KEY_NAME_CHILD_TYPE).toString();
}

static inline AceTreeEntity *getEntityFromElements(AceTreeItem *item, const QString &key) {
    AceTreeItem *tmp;
    return (tmp = item->element(key)) ? AceTreeEntity::itemToEntity(tmp) : nullptr;
}

#define myWarning(func) (qWarning().nospace() << "AceTreeStandardEntity::" << (func) << "():").space()

AceTreeStandardEntityPrivate::AceTreeStandardEntityPrivate(AceTreeEntityExtra *extra, AceTreeStandardEntity::Type type)
    : AceTreeEntityPrivate(extra), type(type) {
    m_external = false;
}

AceTreeStandardEntityPrivate::~AceTreeStandardEntityPrivate() {
}

bool AceTreeStandardEntityPrivate::testModifiable(const char *func) const {
    Q_Q(const AceTreeStandardEntity);
    if (!m_treeItem->isWritable()) {
        myWarning(func) << "the entity isn't writable now" << q;
        return false;
    }
    return true;
}

bool AceTreeStandardEntityPrivate::testInsertable(const char *func, const AceTreeEntity *entity) const {
    Q_Q(const AceTreeStandardEntity);
    if (!entity) {
        myWarning(func) << "entity is null";
        return false;
    }

    if (entity == q) {
        myWarning(func) << "entity cannot be its child itself";
        return false;
    }

    // Validate
    if (!entity->isFree()) {
        myWarning(func) << "entity is not free" << entity;
        return false;
    }

    return true;
}

void AceTreeStandardEntityPrivate::event(AceTreeEvent *event) {
    Q_Q(AceTreeStandardEntity);

    switch (event->type()) {
        case AceTreeEvent::RowsInsert: {
            auto e = static_cast<AceTreeRowsInsDelEvent *>(event);
            const auto &items = e->children();

            QVector<AceTreeEntity *> entities;
            entities.reserve(items.size());
            if (m_external) {
                // Triggered by user
                for (const auto &item : items) {
                    entities.append(AceTreeEntity::itemToEntity(item));
                }
            } else {
                // Triggered by undo/redo
                setupVector_helper(q->schema(), &AceTreeStandardSchema::rowTypeKey,
                                   &AceTreeStandardSchema::rowDefaultBuilder, &AceTreeStandardSchema::rowBuilder, items,
                                   entities);
            }

            // Add children
            for (const auto &entity : qAsConst(entities)) {
                q->addChild(entity);
            }
            break;
        }
        case AceTreeEvent::RowsRemove: {
            auto e = static_cast<AceTreeRowsInsDelEvent *>(event);
            const auto &items = e->children();

            Q_UNUSED(m_external);

            for (const auto &item : items) {
                auto entity = AceTreeEntity::itemToEntity(item);
                q->removeChild(entity);
                if (!entity->isFree())
                    delete entity;
            }
            break;
        }
        case AceTreeEvent::RecordAdd: {
            auto e = static_cast<AceTreeRecordEvent *>(event);
            const auto &item = e->child();

            QVector<AceTreeEntity *> entities;
            entities.reserve(1);
            if (m_external) {
                // Triggered by user
                entities.append(AceTreeEntity::itemToEntity(item));
            } else {
                // Triggered by undo/redo
                setupVector_helper(q->schema(), &AceTreeStandardSchema::recordTypeKey,
                                   &AceTreeStandardSchema::recordDefaultBuilder, &AceTreeStandardSchema::recordBuilder,
                                   {item}, entities);
            }

            // Add children
            for (const auto &entity : qAsConst(entities)) {
                q->addChild(entity);
            }
            break;
        }
        case AceTreeEvent::RecordRemove: {
            auto e = static_cast<AceTreeRecordEvent *>(event);
            const auto &item = e->child();

            auto entity = AceTreeEntity::itemToEntity(item);
            q->removeChild(entity);
            if (!entity->isFree())
                delete entity;
            break;
        }
        case AceTreeEvent::ElementAdd: {
            auto e = static_cast<AceTreeElementEvent *>(event);
            const auto &item = e->child();

            AceTreeEntity *entity;
            const auto &typeValue = getTypeValueFromItem(item);
            if (m_external) {
                // Triggered by user
                entity = AceTreeEntity::itemToEntity(item);
            } else {
                // Triggered by undo/redo
                // Find type value to determine the derived entity class builder
                auto builder = q->schema().elemenetBuilder(typeValue);
                if (!builder.isValid())
                    return; // No builder, skip

                auto child = builder();
                if (!child)
                    return;

                child->setup(item);
                entity = child;
            }

            q->addChild(entity);
            addElement_assign(entity);

            break;
        }
        case AceTreeEvent::ElementRemove: {
            auto e = static_cast<AceTreeElementEvent *>(event);
            const auto &item = e->child();

            auto entity = AceTreeEntity::itemToEntity(item);
            // const auto &typeValue = getTypeValueFromItem(item);

            removeElement_assigns(entity);
            q->removeChild(entity);
            if (!entity->isFree())
                delete entity;

            break;
        }
        default:
            break;
    }

    AceTreeEntityPrivate::event(event);
}

bool AceTreeStandardEntityPrivate::readVector_helper(const AceTreeStandardSchema &schema,
                                                     AceTreeStandardEntityPrivate::GetTypeKey getTypeKey,
                                                     AceTreeStandardEntityPrivate::GetDefaultBuilder getDefaultBuilder,
                                                     AceTreeStandardEntityPrivate::GetBuilder getBuilder,
                                                     const QJsonValue &value,
                                                     QVector<QPair<QString, AceTreeEntity *>> &childrenToAdd) {

    if (!value.isArray()) {
        return false;
    }

    const auto &typeKey = (schema.*getTypeKey)();
    if (typeKey.isEmpty()) {
        // If there's no type key
        // Consider it's an array storing non-polymorphic class, use default builder
        auto builder = (schema.*getDefaultBuilder)();
        if (!builder.isValid())
            return true; // No default builder, skip

        auto arr = value.toArray();
        for (const auto &item : qAsConst(arr)) {
            auto child = builder();
            if (!child)
                continue;

            child->initialize();

            qDebug().noquote() << std::string(indent, ' ').c_str() << "[read child]" << child;
            if (!child->read(item)) {
                // Ignore error
                delete child;
                continue;
            }
            qDebug() << std::string(indent, ' ').c_str() << "[success]";

            childrenToAdd.append({QString(), child});
        }
    } else {
        auto arr = value.toArray();
        for (const auto &item : qAsConst(arr)) {
            const auto &typeValue = item[typeKey].toString();

            // Find type value to determine the derived entity class builder
            auto builder = (schema.*getBuilder)(typeValue);
            if (!builder.isValid())
                continue; // No builder, skip

            auto child = builder();
            if (!child)
                continue;

            child->initialize();
            qDebug().noquote() << std::string(indent, ' ').c_str() << "[read child]" << typeValue << child;
            if (!child->read(item)) {
                // Ignore error
                delete child;
                continue;
            }
            qDebug() << std::string(indent, ' ').c_str() << "[success]";

            childrenToAdd.append({typeValue, child});
        }
    }
    return true;
}

bool AceTreeStandardEntityPrivate::readSet_helper(const AceTreeStandardSchema &schema, const QJsonValue &value) {
    if (!value.isObject()) {
        return false;
    }

    auto obj = value.toObject();

    // Read dynamic data
    auto hash = schema.dynamicDataSpecHash();
    QVector<QPair<QString, QVariant>> dynamicDataToSet;
    if (!hash.isEmpty())
        qDebug().noquote() << std::string(indent, ' ').c_str() << "[collect dynamic-data]" << hash.keys();
    for (auto it = hash.begin(); it != hash.end(); ++it) {
        auto val = obj.value(it.key());
        if (val.isUndefined())
            continue;

        // Type checking (Important)
        if (val.type() != it->type())
            return false;

        dynamicDataToSet.append({it.key(), val.toVariant()});
    }
    for (const auto &pair : qAsConst(dynamicDataToSet)) {
        qDebug().noquote() << std::string(indent, ' ').c_str() << "[read dynamic-data]" << pair.first << pair.second;
        m_treeItem->setDynamicData(pair.first, pair.second);
    }

    // Read properties
    hash = schema.propertySpecHash();
    QVector<QPair<QString, QVariant>> propertiesToSet;
    if (!hash.isEmpty())
        qDebug().noquote() << std::string(indent, ' ').c_str() << "[collect properties]" << hash.keys();
    for (auto it = hash.begin(); it != hash.end(); ++it) {
        auto val = obj.value(it.key());
        if (val.isUndefined())
            continue;

        // Type checking (Important)
        if (val.type() != it->type())
            return false;

        propertiesToSet.append({it.key(), val.toVariant()});
    }
    for (const auto &pair : qAsConst(propertiesToSet)) {
        qDebug().noquote() << std::string(indent, ' ').c_str() << "[read property]" << pair.first << pair.second;
        m_treeItem->setProperty(pair.first, pair.second);
    }

    // Read element children
    const auto &elementKeys = schema.elementKeys();
    for (const auto &key : elementKeys) {
        auto val = obj.value(key);
        if (val.isUndefined())
            continue;

        auto child = getEntityFromElements(m_treeItem, key);
        if (!child)
            continue;

        qDebug().noquote() << std::string(indent, ' ').c_str() << "[read child]" << key << child;
        if (!child->read(val))
            // Ignore
            continue;
        qDebug() << std::string(indent, ' ').c_str() << "[success]";
    }
    return true;
}

QJsonArray AceTreeStandardEntityPrivate::writeVector_helper(const AceTreeStandardSchema &schema,
                                                            const QVector<AceTreeItem *> &childItems) const {
    Q_Q(const AceTreeStandardEntity);
    Q_UNUSED(q);

    QJsonArray arr;
    auto typeKey = schema.rowTypeKey();
    if (typeKey.isEmpty()) {
        for (const auto &item : childItems) {
            auto child = AceTreeEntity::itemToEntity(item);
            if (!child)
                continue;

            QJsonValue value = child->write();
            arr.append(value);
        }
    } else {
        for (const auto &item : childItems) {
            auto child = AceTreeEntity::itemToEntity(item);
            if (!child)
                continue;

            QJsonValue value = child->write();
            auto childItem = AceTreeEntityPrivate::getItem(child);

            // Insert type value
            auto typeValue = getTypeValueFromItem(childItem);
            if (!typeValue.isEmpty() && value.isObject()) {
                // Add type value to json object
                auto obj = value.toObject();
                obj.insert(typeKey, typeValue);
                value = obj;
            }

            arr.append(value);
        }
    }
    return arr;
}

QJsonObject AceTreeStandardEntityPrivate::writeSet_helper(const AceTreeStandardSchema &schema) const {
    Q_UNUSED(schema);

    QJsonObject obj;

    // Write dynamic data
    auto hash = schema.dynamicDataSpecHash();
    for (auto it = hash.begin(); it != hash.end(); ++it) {
        auto val = m_treeItem->dynamicData(it.key());
        if (val.isValid())
            obj.insert(it.key(), QJsonValue::fromVariant(val));
        else
            obj.insert(it.key(), it.value());
    }

    // Write properties
    hash = schema.propertySpecHash();
    for (auto it = hash.begin(); it != hash.end(); ++it) {
        auto val = m_treeItem->property(it.key());
        if (val.isValid())
            obj.insert(it.key(), QJsonValue::fromVariant(val));
        else
            obj.insert(it.key(), it.value());
    }

    // Write element children
    const auto &childElementMap = m_treeItem->elementHash();
    for (auto it = childElementMap.begin(); it != childElementMap.end(); ++it) {
        obj.insert(it.key(), AceTreeEntity::itemToEntity(it.value())->write());
    }

    return obj;
}

void AceTreeStandardEntityPrivate::setupVector_helper(const AceTreeStandardSchema &schema, GetTypeKey getTypeKey,
                                                      GetDefaultBuilder getDefaultBuilder, GetBuilder getBuilder,
                                                      const QVector<AceTreeItem *> &items,
                                                      QVector<AceTreeEntity *> &childrenToAdd) {
    const auto &typeKey = (schema.*getTypeKey)();
    if (typeKey.isEmpty()) {
        // If there's no type key
        // Consider it's an array storing non-polymorphic class, use default builder
        auto builder = (schema.*getDefaultBuilder)();
        if (!builder.isValid())
            return; // No default builder, skip

        for (const auto &item : qAsConst(items)) {
            auto child = builder();
            if (!child)
                continue;

            child->setup(item);
            childrenToAdd.append(child);
        }
    } else {
        for (const auto &item : qAsConst(items)) {
            const auto &typeValue = getTypeValueFromItem(item);

            // Find type value to determine the derived entity class builder
            auto builder = (schema.*getBuilder)(typeValue);
            if (!builder.isValid())
                continue; // No builder, skip

            auto child = builder();
            if (!child)
                continue;

            child->setup(item);
            childrenToAdd.append(child);
        }
    }
}

void AceTreeStandardEntityPrivate::addElement_assign(AceTreeEntity *child) {
    auto it = childPostAssignRefs.find(getTypeValueFromItem(AceTreeEntityPrivate::getItem(child)));
    if (it == childPostAssignRefs.end())
        return;
    it.value()(child);
}

void AceTreeStandardEntityPrivate::removeElement_assigns(AceTreeEntity *child) {
    auto it = childPostAssignRefs.find(getTypeValueFromItem(AceTreeEntityPrivate::getItem(child)));
    if (it == childPostAssignRefs.end())
        return;
    it.value()(child);
}

//===========================================================================
// AceTreeStandardEntity

AceTreeStandardEntity::AceTreeStandardEntity(Type type, QObject *parent)
    : AceTreeStandardEntity(type, nullptr, parent) {
}

AceTreeStandardEntity::AceTreeStandardEntity(AceTreeStandardEntity::Type type, AceTreeEntityExtra *extra,
                                             QObject *parent)
    : AceTreeStandardEntity(*new AceTreeStandardEntityPrivate(extra, type), parent) {
}

AceTreeStandardEntity::~AceTreeStandardEntity() {
}

AceTreeStandardEntity::Type AceTreeStandardEntity::type() const {
    Q_D(const AceTreeStandardEntity);
    return d->type;
}

bool AceTreeStandardEntity::read(const QJsonValue &value) {
    Q_D(AceTreeStandardEntity);

    class ABC {
    public:
        explicit ABC(int &i) : i(i) {
            i += 2;
        }
        ~ABC() {
            i -= 2;
        }

        int &i;
    };

    ABC abc(indent);

    auto schema = this->schema();
    switch (d->type) {
        case Vector: {
            QVector<QPair<QString, AceTreeEntity *>> childrenToAdd;
            if (!d->readVector_helper(schema, &AceTreeStandardSchema::rowTypeKey,
                                      &AceTreeStandardSchema::rowDefaultBuilder, &AceTreeStandardSchema::rowBuilder,
                                      value, childrenToAdd)) {
                return false;
            }
            insertRows(rowCount(), childrenToAdd);
            break;
        }

        case RecordTable: {
            QVector<QPair<QString, AceTreeEntity *>> childrenToAdd;
            if (!d->readVector_helper(schema, &AceTreeStandardSchema::recordTypeKey,
                                      &AceTreeStandardSchema::recordDefaultBuilder,
                                      &AceTreeStandardSchema::recordBuilder, value, childrenToAdd)) {
                return false;
            }
            for (const auto &pair : qAsConst(childrenToAdd)) {
                addRecord(pair.first, pair.second);
            }
            break;
        }

        case Mapping: {
            if (!d->readSet_helper(schema, value)) {
                return false;
            }
            break;
        }
        default:
            break;
    }
    return true;
}

QJsonValue AceTreeStandardEntity::write() const {
    Q_D(const AceTreeStandardEntity);

    QJsonValue res;
    switch (d->type) {
        case Vector: {
            auto schema = this->schema();
            res = d->writeVector_helper(schema, d->m_treeItem->rows());
            break;
        }

        case RecordTable: {
            auto schema = this->schema();

            // Get entities and sort
            const auto &recordMap = d->m_treeItem->recordMap();

            QVector<AceTreeEntity *> entities;
            entities.reserve(recordMap.size());
            for (auto it = recordMap.begin(); it != recordMap.end(); ++it) {
                entities.append(AceTreeEntity::itemToEntity(it.value()));
            }

            // Get items in the right order
            QVector<AceTreeItem *> childItems;
            for (const auto &entity : qAsConst(entities)) {
                childItems.append(AceTreeEntityPrivate::getItem(entity));
            }
            res = d->writeVector_helper(schema, childItems);
            break;
        }

        case Mapping: {
            auto schema = this->schema();
            res = d->writeSet_helper(schema);
            break;
        }

        default:
            break;
    }

    return res;
}

void AceTreeStandardEntity::sortRecords(QVector<AceTreeEntity *> &records) const {
    Q_UNUSED(records);

    // Do nothing
}

template <class K, template <class> class List, class T>
static void unionHashCollections(QHash<K, List<T>> &to, const QHash<K, List<T>> &from) {
    for (auto it = from.begin(); it != from.end(); ++it) {
        auto it2 = to.find(it.key());
        if (it2 == to.end()) {
            it2 = to.insert(it.key(), {});
        }
        it2->reserve(it2->capacity() + it->size());
        for (const auto &item : qAsConst(it.value())) {
            it2->insert(item);
        }
    }
}

template <class K, class T>
static void unionHash(QHash<K, T> &to, const QHash<K, T> &from) {
    for (auto it = from.begin(); it != from.end(); ++it) {
        if (to.contains(it.key()))
            continue;
        to.insert(it.key(), it.value());
    }
}

AceTreeStandardSchema AceTreeStandardEntity::schema() const {
    auto mo = metaObject();
    {
        // Search in cache
        auto it = globalSpecMapComplete->find(mo);
        if (it != globalSpecMapComplete->end()) {
            return it.value();
        }
    }

    auto mo0 = mo;

    // Get and cache schema
    auto thisSchema = AceTreeStandardSchema::globalSchema(mo);
    auto d1 = thisSchema.d.data();
    mo = mo->superClass();

    while (mo != &AceTreeStandardEntity::staticMetaObject) {
        bool ok;
        auto schema = AceTreeStandardSchema::globalSchema(mo, &ok);
        if (!ok) {
            mo = mo->superClass();
            continue;
        }

        // Add super class data
        auto d2 = schema.d.constData();
        unionHash(d1->dynamicData, d2->dynamicData);
        unionHash(d1->properties, d2->properties);

        unionHash(d1->rowBuilders, d2->rowBuilders);
        unionHashCollections(d1->rowBuilderIndexes, d2->rowBuilderIndexes);
        if (!d1->rowDefaultBuilder.isValid()) {
            d1->rowDefaultBuilder = d2->rowDefaultBuilder;
        }

        unionHash(d1->recordBuilders, d2->recordBuilders);
        unionHashCollections(d1->recordBuilderIndexes, d2->recordBuilderIndexes);
        if (!d1->recordDefaultBuilder.isValid()) {
            d1->recordDefaultBuilder = d2->recordDefaultBuilder;
        }

        unionHash(d1->elementBuilders, d2->elementBuilders);
        unionHashCollections(d1->elementBuilderIndexes, d2->elementBuilderIndexes);

        mo = mo->superClass();
    }

    globalSpecMapComplete->insert(mo0, thisSchema);

    return thisSchema;
}

QString AceTreeStandardEntity::searchChildType(const QMetaObject *metaObject) const {
    Q_D(const AceTreeStandardEntity);

    auto schema = this->schema();
    typedef QString (AceTreeStandardSchema::*SearchType)(const QMetaObject *) const;
    SearchType func = nullptr;
    switch (d->type) {
        case Vector:
            if (schema.rowTypeKey().isEmpty()) {
                return {};
            }
            func = &AceTreeStandardSchema::rowType;
            break;
        case RecordTable:
            if (schema.recordTypeKey().isEmpty()) {
                return {};
            }
            func = &AceTreeStandardSchema::recordType;
            break;
        case Mapping:
            func = &AceTreeStandardSchema::elementKey;
            break;
        default:
            return {};
            break;
    }

    QString res;
    while (metaObject != &AceTreeStandardEntity::staticMetaObject) {
        res = (schema.*func)(metaObject);
        if (!res.isEmpty()) {
            break;
        }
        metaObject = metaObject->superClass();
    }
    return res;
}

AceTreeEntity *AceTreeStandardEntity::childElement(const QString &key) const {
    Q_D(const AceTreeStandardEntity);
    return getEntityFromElements(d->m_treeItem, key);
}

QStringList AceTreeStandardEntity::childElementKeys() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->elementKeys();
}

int AceTreeStandardEntity::childElementCount() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->elementCount();
}

void AceTreeStandardEntity::doInitialize() {
    Q_D(AceTreeStandardEntity);

    // Add subscriber
    auto &treeItem = d->m_treeItem;

    switch (d->type) {
        case Mapping: {
            auto schema = this->schema();

            // Set default dynamic data
            auto hash = schema.dynamicDataSpecHash();
            for (auto it = hash.begin(); it != hash.end(); ++it) {
                treeItem->setDynamicData(it.key(), it->toVariant());
            }

            // Set default properties
            hash = schema.propertySpecHash();
            for (auto it = hash.begin(); it != hash.end(); ++it) {
                treeItem->setProperty(it.key(), it->toVariant());
            }

            // Set default element children
            const auto &keys = schema.elementKeys();
            for (const auto &key : keys) {
                auto builder = schema.elemenetBuilder(key);
                auto child = builder();
                if (!child)
                    continue;

                child->initialize();
                addElement(key, child);
            }
            break;
        }

        default:
            break;
    }
}

void AceTreeStandardEntity::doSetup() {
    Q_D(AceTreeStandardEntity);

    // Add subscriber
    auto &treeItem = d->m_treeItem;

    switch (d->type) {
        case Vector: {
            AceTreeRowsInsDelEvent e(AceTreeEvent::RowsInsert, treeItem, 0, treeItem->rows());
            d->event(&e);
            break;
        }
        case RecordTable: {
            const auto &recordMap = d->m_treeItem->recordMap();
            for (auto it = recordMap.begin(); it != recordMap.end(); ++it) {
                AceTreeRecordEvent e(AceTreeEvent::RecordAdd, treeItem, it.key(), it.value());
                d->event(&e);
            }
            break;
        }
        case Mapping: {
            const auto &elementHash = d->m_treeItem->elementHash();
            for (auto it = elementHash.begin(); it != elementHash.end(); ++it) {
                AceTreeElementEvent e(AceTreeEvent::ElementAdd, treeItem, it.key(), it.value());
                d->event(&e);
            }
            break;
        }
        default:
            break;
    }
}

AceTreeStandardEntity::AceTreeStandardEntity(AceTreeStandardEntityPrivate &d, QObject *parent)
    : AceTreeEntity(d, parent) {
}

QVariant AceTreeStandardEntity::dynamicData(const QString &key) const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->dynamicData(key);
}

void AceTreeStandardEntity::setDynamicData(const QString &key, const QVariant &value) {
    Q_D(AceTreeStandardEntity);
    d->m_treeItem->setDynamicData(key, value);
}

QVariantHash AceTreeStandardEntity::dynamicDataMap() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->dynamicDataMap();
}

QVariant AceTreeStandardEntity::property(const QString &key) const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->property(key);
}

bool AceTreeStandardEntity::setProperty(const QString &key, const QVariant &value) {
    Q_D(AceTreeStandardEntity);
    return d->m_treeItem->setProperty(key, value);
}

QVariantHash AceTreeStandardEntity::properties() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->propertyMap();
}

bool AceTreeStandardEntity::insertRows(int index, const QVector<QPair<QString, AceTreeEntity *>> &entities) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    for (const auto &pair : entities) {
        auto &child = pair.second;
        if (!d->testInsertable(__func__, child)) {
            return false;
        }
    }

    QVector<AceTreeItem *> childItems;
    childItems.reserve(entities.size());
    for (const auto &pair : entities) {
        auto &child = pair.second;
        auto childItem = AceTreeEntityPrivate::getItem(child);
        setTypeValueToItem(childItem, pair.first); // Set type value
        childItems.append(childItem);
    }

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->insertRows(index, childItems);
    d->m_external = false;
    return res;
}

bool AceTreeStandardEntity::moveRows(int index, int count, int dest) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->moveRows(index, count, dest);
    d->m_external = false;
    return res;
}

bool AceTreeStandardEntity::removeRows(int index, int count) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->removeRows(index, count);
    d->m_external = false;
    return res;
}

AceTreeEntity *AceTreeStandardEntity::row(int row) const {
    Q_D(const AceTreeStandardEntity);
    return AceTreeEntity::itemToEntity(d->m_treeItem->row(row));
}

QVector<AceTreeEntity *> AceTreeStandardEntity::rows() const {
    Q_D(const AceTreeStandardEntity);
    const auto &rows = d->m_treeItem->rows();

    QVector<AceTreeEntity *> res;
    res.reserve(rows.size());
    for (const auto &item : rows) {
        auto child = AceTreeEntity::itemToEntity(item);
        if (!child)
            continue;
        res.append(child);
    }
    return res;
}

int AceTreeStandardEntity::rowIndexOf(AceTreeEntity *entity) const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->rowIndexOf(AceTreeEntityPrivate::getItem(entity));
}

int AceTreeStandardEntity::rowCount() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->rowCount();
}

int AceTreeStandardEntity::addRecord(const QString &key, AceTreeEntity *entity) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!d->testInsertable(__func__, entity)) {
        return false;
    }

    auto childItem = AceTreeEntityPrivate::getItem(entity);
    setTypeValueToItem(childItem, key); // Set type value

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->addRecord(childItem);
    d->m_external = false;
    return res;
}

bool AceTreeStandardEntity::removeRecord(int seq) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->removeRecord(seq);
    d->m_external = false;
    return res;
}

bool AceTreeStandardEntity::removeRecord(AceTreeEntity *entity) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    if (!entity) {
        myWarning(__func__) << "trying to remove a null record from" << this;
        return false;
    }

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->removeRecord(AceTreeEntityPrivate::getItem(entity));
    d->m_external = false;
    return res;
}

AceTreeEntity *AceTreeStandardEntity::record(int seq) const {
    Q_D(const AceTreeStandardEntity);
    return AceTreeEntity::itemToEntity(d->m_treeItem->record(seq));
}

int AceTreeStandardEntity::recordSequenceOf(AceTreeEntity *entity) const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->recordSequenceOf(AceTreeEntityPrivate::getItem(entity));
}

QList<int> AceTreeStandardEntity::records() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->records();
}

int AceTreeStandardEntity::recordCount() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->recordCount();
}

bool AceTreeStandardEntity::containsElement(AceTreeEntity *entity) const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->containsElement(AceTreeEntityPrivate::getItem(entity));
}

QList<AceTreeEntity *> AceTreeStandardEntity::elements() const {
    Q_D(const AceTreeStandardEntity);
    const auto &hash = d->m_treeItem->elementHash();
    QList<AceTreeEntity *> res;
    res.reserve(hash.size());
    for (const auto &item : hash) {
        res.append(AceTreeEntity::itemToEntity(item));
    }
    return res;
}

int AceTreeStandardEntity::elementCount() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->elementCount();
}

QStringList AceTreeStandardEntity::elementKeys() const {
    Q_D(const AceTreeStandardEntity);
    return d->m_treeItem->elementKeys();
}

AceTreeEntity *AceTreeStandardEntity::element(const QString &key) const {
    Q_D(const AceTreeStandardEntity);
    return AceTreeEntity::itemToEntity(d->m_treeItem->element(key));
}

bool AceTreeStandardEntity::addElement(const QString &key, AceTreeEntity *entity) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    // Validate
    if (!d->testInsertable(__func__, entity)) {
        return false;
    }

    auto childItem = AceTreeEntityPrivate::getItem(entity);
    setTypeValueToItem(childItem, key); // Set type value

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->addElement(key, childItem);
    d->m_external = false;
    return res;
}

bool AceTreeStandardEntity::removeElement(AceTreeEntity *entity) {
    Q_D(AceTreeStandardEntity);

    if (!d->testModifiable(__func__))
        return false;

    if (!entity) {
        myWarning(__func__) << "trying to remove a null element from" << this;
        return false;
    }

    // Commit change to model
    d->m_external = true;
    auto res = d->m_treeItem->removeElement(AceTreeEntityPrivate::getItem(entity));
    d->m_external = false;
    return res;
}

//===========================================================================


//===========================================================================
// AceTreeStandardSchema

AceTreeStandardSchema::AceTreeStandardSchema() : d(new AceTreeStandardSchemaData()) {
}

AceTreeStandardSchema::~AceTreeStandardSchema() {
}

AceTreeStandardSchema::AceTreeStandardSchema(const AceTreeStandardSchema &other) : d(other.d) {
}

AceTreeStandardSchema::AceTreeStandardSchema(AceTreeStandardSchema &&other) noexcept {
    qSwap(d, other.d);
}

AceTreeStandardSchema &AceTreeStandardSchema::operator=(const AceTreeStandardSchema &other) = default;

AceTreeStandardSchema &AceTreeStandardSchema::operator=(AceTreeStandardSchema &&other) noexcept {
    qSwap(d, other.d);
    return *this;
}

QJsonValue AceTreeStandardSchema::propertySpec(const QString &key) const {
    return d->properties.value(key);
}

void AceTreeStandardSchema::setPropertySpec(const QString &key, const QJsonValue &defaultValue) {
    if (defaultValue.isUndefined()) {
        d->properties.remove(key);
    } else {
        d->properties.insert(key, defaultValue);
    }
}

QStringList AceTreeStandardSchema::propertySpecKeys() const {
    return d->properties.keys();
}

QHash<QString, QJsonValue> AceTreeStandardSchema::propertySpecHash() const {
    return d->properties;
}

QJsonValue AceTreeStandardSchema::dynamicDataSpec(const QString &key) const {
    return d->dynamicData.value(key);
}

void AceTreeStandardSchema::setDynamicDataSpec(const QString &key, const QJsonValue &defaultValue) {
    if (defaultValue.isUndefined()) {
        d->dynamicData.remove(key);
    } else {
        d->dynamicData.insert(key, defaultValue);
    }
}

QStringList AceTreeStandardSchema::dynamicDataSpecKeys() const {
    return d->dynamicData.keys();
}

QHash<QString, QJsonValue> AceTreeStandardSchema::dynamicDataSpecHash() const {
    return d->dynamicData;
}

QString AceTreeStandardSchema::rowTypeKey() const {
    return d->rowTypeKey;
}

void AceTreeStandardSchema::setRowTypeKey(const QString &key) {
    d->rowTypeKey = key;
}

AceTreeEntityBuilder AceTreeStandardSchema::rowBuilder(const QString &type) const {
    return d->rowBuilders.value(type, nullptr);
}

static void setBuilder_helper(QHash<QString, AceTreeEntityBuilder> &builders,
                              QHash<const QMetaObject *, QSet<QString>> &indexes, const QString &type,
                              const AceTreeEntityBuilder &builder) {
    // Remove old
    auto it = builders.find(type);
    if (it != builders.end()) {
        auto it2 = indexes.find(it->metaObject());
        it2->remove(type);
        if (it2->isEmpty()) {
            indexes.erase(it2);
        }
    }

    if (!builder.isValid())
        return;

    // Insert new
    builders.insert(type, builder);
    indexes[builder.metaObject()].insert(type);
}

void AceTreeStandardSchema::setRowBuilder(const QString &type, const AceTreeEntityBuilder &builder) {
    setBuilder_helper(d->rowBuilders, d->rowBuilderIndexes, type, builder);
}

QStringList AceTreeStandardSchema::rowTypes() const {
    return d->rowBuilders.keys();
}

QString AceTreeStandardSchema::rowType(const QMetaObject *metaObject) const {
    auto it = d->rowBuilderIndexes.find(metaObject);
    if (it == d->rowBuilderIndexes.end())
        return {};
    return *it->begin();
}

QStringList AceTreeStandardSchema::rowTypes(const QMetaObject *metaObject) const {
    return d->rowBuilderIndexes.value(metaObject).values();
}

int AceTreeStandardSchema::rowTypeCount() const {
    return d->rowBuilders.size();
}

AceTreeEntityBuilder AceTreeStandardSchema::rowDefaultBuilder() const {
    return d->rowDefaultBuilder;
}

void AceTreeStandardSchema::setRowDefaultBuilder(const AceTreeEntityBuilder &builder) {
    d->rowDefaultBuilder = builder;
}

QString AceTreeStandardSchema::recordTypeKey() const {
    return d->rowTypeKey;
}

void AceTreeStandardSchema::setRecordTypeKey(const QString &key) {
    d->rowTypeKey = key;
}

AceTreeEntityBuilder AceTreeStandardSchema::recordBuilder(const QString &type) const {
    return d->recordBuilders.value(type, nullptr);
}

void AceTreeStandardSchema::setRecordBuilder(const QString &type, const AceTreeEntityBuilder &builder) {
    setBuilder_helper(d->recordBuilders, d->recordBuilderIndexes, type, builder);
}

QStringList AceTreeStandardSchema::recordTypes() const {
    return d->recordBuilders.keys();
}

QString AceTreeStandardSchema::recordType(const QMetaObject *metaObject) const {
    auto it = d->recordBuilderIndexes.find(metaObject);
    if (it == d->recordBuilderIndexes.end())
        return {};
    return *it->begin();
}

QStringList AceTreeStandardSchema::recordTypes(const QMetaObject *metaObject) const {
    return d->recordBuilderIndexes.value(metaObject).values();
}

int AceTreeStandardSchema::recordTypeCount() const {
    return d->recordBuilders.size();
}

AceTreeEntityBuilder AceTreeStandardSchema::recordDefaultBuilder() const {
    return d->recordDefaultBuilder;
}

void AceTreeStandardSchema::setRecordDefaultBuilder(const AceTreeEntityBuilder &builder) {
    d->recordDefaultBuilder = builder;
}

AceTreeEntityBuilder AceTreeStandardSchema::elemenetBuilder(const QString &key) const {
    return d->elementBuilders.value(key, nullptr);
}

QStringList AceTreeStandardSchema::elementKeys() const {
    return d->elementBuilders.keys();
}

QString AceTreeStandardSchema::elementKey(const QMetaObject *metaObject) const {
    auto it = d->elementBuilderIndexes.find(metaObject);
    if (it == d->elementBuilderIndexes.end())
        return {};
    return *it->begin();
}

QStringList AceTreeStandardSchema::elementKeys(const QMetaObject *metaObject) const {
    return d->elementBuilderIndexes.value(metaObject).values();
}

int AceTreeStandardSchema::elementBuilderCount() const {
    return d->elementBuilders.size();
}

void AceTreeStandardSchema::setElementBuilder(const QString &key, const AceTreeEntityBuilder &builder) {
    setBuilder_helper(d->elementBuilders, d->elementBuilderIndexes, key, builder);
}

AceTreeStandardSchema &AceTreeStandardSchema::globalSchemaRef(const QMetaObject *metaObject) {
    return (*globalSpecMap)[metaObject];
}

AceTreeStandardSchema AceTreeStandardSchema::globalSchema(const QMetaObject *metaObject, bool *ok) {
    auto it = globalSpecMap->find(metaObject);
    if (it == globalSpecMap->end()) {
        if (ok)
            *ok = false;
        return {};
    }
    if (ok)
        *ok = true;
    return it.value();
}

void AceTreeStandardSchema::setGlobalSchema(const QMetaObject *metaObject, const AceTreeStandardSchema &schema) {
    globalSpecMap->insert(metaObject, schema);
}

void AceTreeStandardSchema::clearGlobalSchemaCache(const QMetaObject *metaObject) {
    if (!metaObject) {
        // Clear all cache
        globalSpecMapComplete->clear();
        return;
    }

    // Clear derived classes' cache
    for (auto it = globalSpecMapComplete->begin(); it != globalSpecMapComplete->end();) {
        if (it.key()->inherits(metaObject)) {
            it = globalSpecMapComplete->erase(it);
            continue;
        }
        it++;
    }
}

//===========================================================================


//===========================================================================
// AceTreeEntityVector

using VectorSignals = AceTreeEntityVectorPrivate::Signals;
using VectorSignalIndexes = QHash<const QMetaObject *, QSharedPointer<VectorSignals>>;
Q_GLOBAL_STATIC(VectorSignalIndexes, vectorIndexes)

static VectorSignals *findVectorSignal(const QMetaObject *metaObject) {
    auto it = vectorIndexes->find(metaObject);
    if (it != vectorIndexes->end()) {
        return it->data();
    }

    VectorSignals s;
    QHash<QString, QPair<QMetaMethod *, bool (*)(QMetaMethod *)>> fields{
        {
         "inserted", {
                &s.inserted,
                [](QMetaMethod *method) {
                    return QRegularExpression(R"(inserted\(int,QVector<\w+\*>\))")
                        .match(method->methodSignature())
                        .hasMatch(); //
                },
            }, },
        {"aboutToMove",
         {
             &s.aboutToMove,
             [](QMetaMethod *method) {
                 return method->methodSignature() == "aboutToMove(int,int,int)"; //
             },
         }},
        {"moved",
         {
             &s.moved,
             [](QMetaMethod *method) {
                 return method->methodSignature() == "moved(int,int,int)"; //
             },
         }},
        {"aboutToRemove",
         {
             &s.aboutToRemove,
             [](QMetaMethod *method) {
                 return QRegularExpression(R"(aboutToRemove\(int,QVector<\w+\*>\))")
                     .match(method->methodSignature())
                     .hasMatch(); //
             },
         }},
        {"removed",
         {
             &s.removed,
             [](QMetaMethod *method) {
                 return method->methodSignature() == "removed(int,int)"; //
             },
         }},
    };

    int methodCount = metaObject->methodCount();

    for (int i = 0; i < methodCount; ++i) {
        QMetaMethod method = metaObject->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            auto it1 = fields.find(method.name());
            if (it1 == fields.end())
                continue;
            auto &ref = it1.value();
            if (!ref.second(&method))
                continue;
            *ref.first = method;
        }
    }

    it = vectorIndexes->insert(metaObject, QSharedPointer<VectorSignals>::create(s));
    return it->data();
}

AceTreeEntityVectorPrivate::AceTreeEntityVectorPrivate(AceTreeEntityExtra *extra)
    : AceTreeStandardEntityPrivate(extra, AceTreeStandardEntity::Vector) {
    _signals = nullptr;
}

AceTreeEntityVectorPrivate::~AceTreeEntityVectorPrivate() {
}

void AceTreeEntityVectorPrivate::deferred_init() {
    AceTreeStandardEntityPrivate::deferred_init();

    Q_Q(AceTreeEntityVector);
    _signals = findVectorSignal(q->metaObject());
}

void AceTreeEntityVectorPrivate::event(AceTreeEvent *event) {
    AceTreeStandardEntityPrivate::event(event);

    Q_Q(AceTreeEntityVector);
    auto doIt = [q](AceTreeRowsInsDelEvent *e, QMetaMethod &mm) {
        if (!mm.isValid()) {
            return;
        }
        const auto &index = e->index();
        const auto &items = e->children();
        mm.invoke(q,                                                                 //
                  QGenericArgument("int", &index),                                   //
                  QGenericArgument(QMetaType::typeName(mm.parameterType(1)), &items) //
        );
    };

    switch (event->type()) {
        case AceTreeEvent::RowsInsert: {
            doIt(static_cast<AceTreeRowsInsDelEvent *>(event), _signals->inserted);
            break;
        }
        case AceTreeEvent::RowsAboutToMove:
        case AceTreeEvent::RowsMove: {
            auto e = static_cast<AceTreeRowsMoveEvent *>(event);
            auto &mm = event->type() == AceTreeEvent::RowsAboutToMove ? _signals->aboutToMove : _signals->moved;
            if (!mm.isValid()) {
                return;
            }

            Q_Q(AceTreeEntityVector);
            const auto &index = e->index();
            const auto &count = e->count();
            const auto &dest = e->destination();
            mm.invoke(q,                               //
                      QGenericArgument("int", &index), //
                      QGenericArgument("int", &count), //
                      QGenericArgument("int", &dest)   //
            );
            break;
        }
        case AceTreeEvent::RowsAboutToRemove: {
            doIt(static_cast<AceTreeRowsInsDelEvent *>(event), _signals->aboutToRemove);
            break;
        }
        case AceTreeEvent::RowsRemove: {
            doIt(static_cast<AceTreeRowsInsDelEvent *>(event), _signals->removed);
            break;
        }
        default:
            break;
    }
}

AceTreeEntityVector::AceTreeEntityVector(QObject *parent) : AceTreeEntityVector(nullptr, parent) {
}

AceTreeEntityVector::AceTreeEntityVector(AceTreeEntityExtra *extra, QObject *parent)
    : AceTreeEntityVector(*new AceTreeEntityVectorPrivate(extra), parent) {
}

AceTreeEntityVector::~AceTreeEntityVector() {
}

AceTreeEntityVector::AceTreeEntityVector(AceTreeEntityVectorPrivate &d, QObject *parent)
    : AceTreeStandardEntity(d, parent) {
    d.init();
}

namespace AceTreePrivate {
    using VV = AceTreeEntityVectorHelper<AceTreeEntity>;
    class AA : public AceTreeEntityVector, public VV {};
    static const uintptr_t offset_v = intptr_t(static_cast<VV *>(reinterpret_cast<AA *>(255))) - 255;
} // namespace AceTreePrivate

AceTreeStandardEntity *AceTreeEntityVector::get_entity_helper(const void *ptr) {
    return reinterpret_cast<AceTreeStandardEntity *>(uintptr_t(ptr) - AceTreePrivate::offset_v);
}

//===========================================================================


//===========================================================================
// AceTreeEntityRecordTable

using RecordTableSignals = AceTreeEntityRecordTablePrivate::Signals;
using RecordTableSignalIndexes = QHash<const QMetaObject *, QSharedPointer<RecordTableSignals>>;
Q_GLOBAL_STATIC(RecordTableSignalIndexes, recordTableIndexes)

static RecordTableSignals *findRecordTableSignal(const QMetaObject *metaObject) {
    auto it = recordTableIndexes->find(metaObject);
    if (it != recordTableIndexes->end()) {
        return it->data();
    }

    RecordTableSignals s;
    QHash<QString, QPair<QMetaMethod *, bool (*)(QMetaMethod *)>> fields{
        {"inserted",
         {
             &s.inserted,
             [](QMetaMethod *method) {
                 return QRegularExpression(R"(inserted\(int,\w+\*\))").match(method->methodSignature()).hasMatch(); //
             },
         }},
        {"aboutToRemove",
         {
             &s.aboutToRemove,
             [](QMetaMethod *method) -> bool {
                 return QRegularExpression(R"(aboutToRemove\(int,\w+\*\))")
                     .match(method->methodSignature())
                     .hasMatch(); //
             },
         }},
        {"removed",
         {
             &s.removed,
             [](QMetaMethod *method) -> bool {
                 return method->methodSignature() == "removed(int)"; //
             },
         }},
    };

    int methodCount = metaObject->methodCount();
    for (int i = 0; i < methodCount; ++i) {
        QMetaMethod method = metaObject->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            auto it1 = fields.find(method.name());
            if (it1 == fields.end())
                continue;
            auto &ref = it1.value();
            if (!ref.second(&method))
                continue;
            *ref.first = method;
        }
    }

    it = recordTableIndexes->insert(metaObject, QSharedPointer<RecordTableSignals>::create(s));
    return it->data();
}

AceTreeEntityRecordTablePrivate::AceTreeEntityRecordTablePrivate(AceTreeEntityExtra *extra)
    : AceTreeStandardEntityPrivate(extra, AceTreeStandardEntity::RecordTable) {
    _signals = nullptr;
}

AceTreeEntityRecordTablePrivate::~AceTreeEntityRecordTablePrivate() {
}

void AceTreeEntityRecordTablePrivate::deferred_init() {
    AceTreeStandardEntityPrivate::deferred_init();

    Q_Q(AceTreeStandardEntity);
    _signals = findRecordTableSignal(q->metaObject());
}

void AceTreeEntityRecordTablePrivate::event(AceTreeEvent *event) {
    AceTreeStandardEntityPrivate::event(event);

    Q_Q(AceTreeEntityRecordTable);
    auto doIt = [q](AceTreeRecordEvent *e, QMetaMethod &mm) {
        if (!mm.isValid()) {
            return;
        }
        const auto &seq = e->sequence();
        const auto &item = e->child();
        mm.invoke(q,                                                                //
                  QGenericArgument("int", &seq),                                    //
                  QGenericArgument(QMetaType::typeName(mm.parameterType(1)), &item) //
        );
    };

    switch (event->type()) {
        case AceTreeEvent::RecordAdd: {
            doIt(static_cast<AceTreeRecordEvent *>(event), _signals->inserted);
            break;
        }
        case AceTreeEvent::RecordAboutToRemove: {
            doIt(static_cast<AceTreeRecordEvent *>(event), _signals->aboutToRemove);
            break;
        }
        case AceTreeEvent::RecordRemove: {
            doIt(static_cast<AceTreeRecordEvent *>(event), _signals->removed);
            break;
        }
        default:
            break;
    }
}

AceTreeEntityRecordTable::AceTreeEntityRecordTable(QObject *parent) : AceTreeEntityRecordTable(nullptr, parent) {
}

AceTreeEntityRecordTable::AceTreeEntityRecordTable(AceTreeEntityExtra *extra, QObject *parent)
    : AceTreeEntityRecordTable(*new AceTreeEntityRecordTablePrivate(extra), parent) {
}

AceTreeEntityRecordTable::~AceTreeEntityRecordTable() {
}

AceTreeEntityRecordTable::AceTreeEntityRecordTable(AceTreeEntityRecordTablePrivate &d, QObject *parent)
    : AceTreeStandardEntity(d, parent) {
    d.init();
}

namespace AceTreePrivate {
    using RR = AceTreeEntityRecordTableHelper<AceTreeEntity>;
    class BB : public AceTreeEntityRecordTable, public RR {};
    static const uintptr_t offset_r = intptr_t(static_cast<RR *>(reinterpret_cast<BB *>(255))) - 255;
} // namespace AceTreePrivate

AceTreeStandardEntity *AceTreeEntityRecordTable::get_entity_helper(const void *ptr) {
    return reinterpret_cast<AceTreeStandardEntity *>(uintptr_t(ptr) - AceTreePrivate::offset_r);
}

//===========================================================================


//===========================================================================
// AceTreeEntityMapping

AceTreeEntityMappingPrivate::AceTreeEntityMappingPrivate(AceTreeEntityExtra *extra)
    : AceTreeStandardEntityPrivate(extra, AceTreeStandardEntity::Mapping) {
}

AceTreeEntityMappingPrivate::~AceTreeEntityMappingPrivate() {
}

AceTreeEntityMapping::AceTreeEntityMapping(QObject *parent) : AceTreeEntityMapping(nullptr, parent) {
}

AceTreeEntityMapping::AceTreeEntityMapping(AceTreeEntityExtra *extra, QObject *parent)
    : AceTreeEntityMapping(*new AceTreeEntityMappingPrivate(extra), parent) {
}

AceTreeEntityMapping::~AceTreeEntityMapping() {
}

AceTreeEntityMapping::AceTreeEntityMapping(AceTreeEntityMappingPrivate &d, QObject *parent)
    : AceTreeStandardEntity(d, parent) {
    d.init();
}

//===========================================================================
// AceTreeEntityMappingExtra

AceTreeEntityMappingExtra::AceTreeEntityMappingExtra()
    : AceTreeEntityNotifyExtra(*new AceTreeEntityMappingExtraPrivate()) {
}

AceTreeEntityMappingExtra::~AceTreeEntityMappingExtra() {
}

AceTreeEntityMapping *AceTreeEntityMappingExtra::entity() const {
    Q_D(const AceTreeEntityMappingExtra);
    return qobject_cast<AceTreeEntityMapping *>(d->entity);
}

void AceTreeEntityMappingExtra::addChildPointer(const QString &key,
                                                const std::function<void(AceTreeEntity *)> &callback) {
    Q_D(const AceTreeEntityMappingExtra);
    AceTreeEntityMappingPrivate::get(reinterpret_cast<AceTreeEntityMapping *>(d->entity))
        ->childPostAssignRefs.insert(key, callback);
}
