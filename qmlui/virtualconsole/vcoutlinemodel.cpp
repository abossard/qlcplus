#include "vcoutlinemodel.h"

#include <QSet>
#include <QTimer>
#include <algorithm>
#include <functional>

#include "vcframe.h"
#include "virtualconsole.h"

namespace
{
class OutlineFilter final : public QSortFilterProxyModel
{
public:
    explicit OutlineFilter(QObject *parent) : QSortFilterProxyModel(parent)
    {
        setRecursiveFilteringEnabled(true);
    }

    void setText(const QString &text)
    {
        m_text = text;
        setFilterFixedString(text);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        if (m_text.isEmpty())
            return true;
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        for (int role : { VCOutlineModel::LabelRole, VCOutlineModel::TypeNameRole,
                          VCOutlineModel::WidgetIDRole })
        {
            if (role == VCOutlineModel::WidgetIDRole &&
                sourceModel()->data(index, VCOutlineModel::RowKindRole).toInt() == VCOutlineModel::SubpageRow)
                continue;
            if (sourceModel()->data(index, role).toString().contains(m_text, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QString m_text;
};
}

struct VCOutlineModel::Node
{
    QString key;
    QPointer<VCWidget> widget;
    quint32 widgetID = VCWidget::invalidId();
    int widgetType = VCWidget::UnknownWidget;
    RowKind kind = WidgetRow;
    int pageIndex = -1;
    int subpage = 0;
    Node *parent = nullptr;
    std::vector<std::unique_ptr<Node>> children;
};

struct VCOutlineModel::Row
{
    QString key;
    VCWidget *widget;
    RowKind kind;
    int pageIndex;
    int subpage;
    std::vector<Row> children;
};

VCOutlineModel::VCOutlineModel(VirtualConsole *vc, QObject *parent)
    : QAbstractItemModel(parent)
    , m_vc(vc)
    , m_root(std::make_unique<Node>())
    , m_filter(new OutlineFilter(this))
{
    m_filter->setSourceModel(this);
    connect(vc, &VirtualConsole::pagesCountChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::widgetsChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::structureChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::selectedWidgetsCountChanged, this, &VCOutlineModel::selectionChanged);
    selectionChanged();
    refresh();
}

VCOutlineModel::~VCOutlineModel() = default;

VCOutlineModel::Node *VCOutlineModel::node(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<Node *>(index.internalPointer()) : m_root.get();
}

int VCOutlineModel::nodeRow(const Node *entry) const
{
    if (!entry || !entry->parent)
        return -1;
    const auto &siblings = entry->parent->children;
    const auto found = std::find_if(siblings.begin(), siblings.end(),
                                   [entry](const auto &item) { return item.get() == entry; });
    return found == siblings.end() ? -1 : static_cast<int>(found - siblings.begin());
}

QModelIndex VCOutlineModel::nodeIndex(const Node *entry) const
{
    const int row = nodeRow(entry);
    return row < 0 ? QModelIndex() : createIndex(row, 0, const_cast<Node *>(entry));
}

QModelIndex VCOutlineModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0 || (parent.isValid() && parent.column() != 0))
        return QModelIndex();
    Node *owner = node(parent);
    if (static_cast<size_t>(row) >= owner->children.size())
        return QModelIndex();
    return createIndex(row, column, owner->children[static_cast<size_t>(row)].get());
}

QModelIndex VCOutlineModel::parent(const QModelIndex &index) const
{
    return index.isValid() ? nodeIndex(node(index)->parent) : QModelIndex();
}

int VCOutlineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return 0;
    return static_cast<int>(node(parent)->children.size());
}

int VCOutlineModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QVariant VCOutlineModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    const Node *entry = node(index);
    VCWidget *widget = entry->widget;
    const auto *frame = qobject_cast<VCFrame *>(widget);
    const bool synthetic = entry->kind == SubpageRow;
    switch (role)
    {
    case Qt::DisplayRole:
    case LabelRole:
        if (synthetic)
        {
            const QStringList labels = frame ? frame->pageLabels() : QStringList();
            return entry->subpage >= 0 && entry->subpage < labels.size() ?
                       labels.at(entry->subpage) : tr("Page %1").arg(entry->subpage + 1);
        }
        if (!widget)
            return tr("<Unavailable>");
        return widget->caption().isEmpty() ? tr("<Unnamed>") : widget->caption();
    case WidgetIDRole:
        return synthetic ? QVariant(-1) : QVariant(entry->widgetID);
    case WidgetTypeRole:
        return synthetic ? -1 : entry->widgetType;
    case TypeNameRole:
        return entry->kind == PageRow ? tr("Page") : synthetic ? tr("Subpage") :
                                                              VCWidget::typeToString(entry->widgetType);
    case RowKindRole:
        return entry->kind;
    case PageIndexRole:
        return entry->pageIndex;
    case SubpageIndexRole:
        return entry->subpage;
    case ParentIDRole:
        return synthetic ? QVariant(entry->widgetID) :
               entry->parent && entry->parent != m_root.get() ? QVariant(entry->parent->widgetID) : QVariant(-1);
    case SelectedRole:
        return entry->kind == WidgetRow && m_selectedIDs.contains(entry->widgetID);
    case IconSourceRole:
        return synthetic || entry->kind == PageRow ? QStringLiteral("qrc:/folder.svg") :
                                                     m_vc->widgetIcon(entry->widgetType);
    case RowKeyRole:
        return entry->key;
    case ParentKeyRole:
        return entry->parent ? entry->parent->key : QString();
    case ContainerRole:
        return entry->kind != WidgetRow || entry->widgetType == VCWidget::FrameWidget ||
                                          entry->widgetType == VCWidget::SoloFrameWidget;
    case OrderIndexRole:
        return nodeRow(entry);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VCOutlineModel::roleNames() const
{
    return {
        {LabelRole, "label"}, {WidgetIDRole, "widgetID"}, {WidgetTypeRole, "widgetType"},
        {TypeNameRole, "typeName"}, {RowKindRole, "rowKind"}, {PageIndexRole, "pageIndex"},
        {SubpageIndexRole, "subpageIndex"}, {ParentIDRole, "parentID"}, {SelectedRole, "selected"},
        {IconSourceRole, "iconSource"}, {RowKeyRole, "rowKey"}, {ParentKeyRole, "parentKey"},
        {ContainerRole, "isContainer"}, {OrderIndexRole, "orderIndex"}
    };
}

Qt::ItemFlags VCOutlineModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

QAbstractItemModel *VCOutlineModel::filteredModel() const { return m_filter; }
QString VCOutlineModel::filterText() const { return m_filterText; }
int VCOutlineModel::revision() const { return m_revision; }

void VCOutlineModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    static_cast<OutlineFilter *>(m_filter)->setText(text);
    emit filterTextChanged();
    ++m_revision;
    emit treeChanged();
}

QModelIndex VCOutlineModel::indexForKey(const QString &key) const
{
    return m_filter->mapFromSource(nodeIndex(m_nodes.value(key, nullptr)));
}

QVariantMap VCOutlineModel::rowData(const QString &key) const
{
    QVariantMap values;
    const QModelIndex index = nodeIndex(m_nodes.value(key, nullptr));
    if (!index.isValid())
        return values;
    const auto roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        values.insert(QString::fromLatin1(it.value()), data(index, it.key()));
    return values;
}

void VCOutlineModel::collectVisible(const QModelIndex &parent, const QSet<QString> &expanded,
                                   QStringList &keys) const
{
    for (int row = 0; row < m_filter->rowCount(parent); ++row)
    {
        const QModelIndex child = m_filter->index(row, 0, parent);
        const QString key = m_filter->data(child, RowKeyRole).toString();
        keys.append(key);
        if (!m_filterText.isEmpty() || expanded.contains(key))
            collectVisible(child, expanded, keys);
    }
}

QStringList VCOutlineModel::visibleKeys(const QStringList &expandedKeys) const
{
    QStringList keys;
    collectVisible(QModelIndex(), QSet<QString>(expandedKeys.begin(), expandedKeys.end()), keys);
    return keys;
}

QVariantList VCOutlineModel::visibleWidgetIDs(const QStringList &expandedKeys) const
{
    QVariantList ids;
    for (const QString &key : visibleKeys(expandedKeys))
    {
        Node *entry = m_nodes.value(key);
        if (entry && entry->kind == WidgetRow && entry->widget)
            ids.append(entry->widgetID);
    }
    return ids;
}

void VCOutlineModel::scheduleRefresh()
{
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QTimer::singleShot(0, this, [this]() { m_refreshPending = false; refresh(); });
}

void VCOutlineModel::watch(VCWidget *widget)
{
    if (m_connections.contains(widget))
        return;
    QList<QMetaObject::Connection> connections;
    connections.append(connect(widget, &VCWidget::captionChanged, this, [this, widget]() {
        widgetDataChanged(widget, {LabelRole});
    }));
    connections.append(connect(widget, &VCWidget::pageChanged, this, &VCOutlineModel::scheduleRefresh));
    connections.append(connect(widget, &QObject::destroyed, this, [this, widget]() {
        m_connections.remove(widget);
        scheduleRefresh();
    }));
    if (auto *frame = qobject_cast<VCFrame *>(widget))
    {
        connections.append(connect(frame, &VCFrame::childOrderChanged, this, &VCOutlineModel::scheduleRefresh));
        connections.append(connect(frame, &VCFrame::multiPageModeChanged, this, &VCOutlineModel::scheduleRefresh));
        connections.append(connect(frame, &VCFrame::totalPagesNumberChanged, this, &VCOutlineModel::scheduleRefresh));
        connections.append(connect(frame, &VCFrame::pageLabelsChanged, this, [this, widget]() {
            widgetDataChanged(widget, {LabelRole});
        }));
    }
    m_connections.insert(widget, connections);
}

void VCOutlineModel::widgetDataChanged(VCWidget *widget, const QList<int> &roles)
{
    for (Node *entry : m_nodes)
    {
        if (entry->widget == widget)
        {
            const QModelIndex index = nodeIndex(entry);
            emit dataChanged(index, index, roles);
        }
    }
    if (!m_filterText.isEmpty())
    {
        ++m_revision;
        emit treeChanged();
    }
}

void VCOutlineModel::selectionChanged()
{
    QSet<quint32> selected;
    for (const QVariant &id : m_vc->selectedWidgetIDs())
        selected.insert(id.toUInt());
    QSet<quint32> changed = m_selectedIDs;
    changed.subtract(selected);
    for (quint32 id : selected)
        if (!m_selectedIDs.contains(id))
            changed.insert(id);
    m_selectedIDs = selected;
    m_selectionChanges.unite(changed);
    if (m_selectionPending || m_selectionChanges.isEmpty())
        return;
    m_selectionPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_selectionPending = false;
        const QSet<quint32> changed = std::move(m_selectionChanges);
        m_selectionChanges.clear();
        for (quint32 id : changed)
        {
            Node *entry = m_nodes.value(QStringLiteral("widget:%1").arg(id), nullptr);
            if (!entry || entry->kind != WidgetRow)
                continue;
            const QModelIndex index = nodeIndex(entry);
            emit dataChanged(index, index, {SelectedRole});
        }
    });
}

void VCOutlineModel::reconcile(Node *owner, const std::vector<Row> &rows)
{
    for (size_t position = 0; position < rows.size(); ++position)
    {
        const Row &row = rows[position];
        Node *entry = m_nodes.value(row.key, nullptr);
        QList<int> changedRoles;
        if (entry && entry->parent != owner)
            changedRoles.append({ParentIDRole, ParentKeyRole});
        if (entry && nodeRow(entry) != static_cast<int>(position))
            changedRoles.append(OrderIndexRole);
        if (!entry)
        {
            beginInsertRows(nodeIndex(owner), static_cast<int>(position), static_cast<int>(position));
            auto added = std::make_unique<Node>();
            added->key = row.key;
            added->widget = row.widget;
            added->widgetID = row.widget->id();
            added->widgetType = row.widget->type();
            added->kind = row.kind;
            added->pageIndex = row.pageIndex;
            added->subpage = row.subpage;
            added->parent = owner;
            entry = added.get();
            owner->children.insert(owner->children.begin() + position, std::move(added));
            m_nodes.insert(row.key, entry);
            endInsertRows();
        }
        else if (entry->parent != owner || nodeRow(entry) != static_cast<int>(position))
        {
            Node *previous = entry->parent;
            const int oldRow = nodeRow(entry);
            const int destination = static_cast<int>(position) +
                                    (previous == owner && oldRow < static_cast<int>(position) ? 1 : 0);
            if (!beginMoveRows(nodeIndex(previous), oldRow, oldRow, nodeIndex(owner), destination))
            {
                qWarning() << "Cannot update VC outline row" << row.key;
                continue;
            }
            auto moved = std::move(previous->children[static_cast<size_t>(oldRow)]);
            previous->children.erase(previous->children.begin() + oldRow);
            entry->parent = owner;
            owner->children.insert(owner->children.begin() + position, std::move(moved));
            endMoveRows();
        }
        if (entry->widget != row.widget || entry->kind != row.kind)
            changedRoles = roleNames().keys();
        if (entry->pageIndex != row.pageIndex)
            changedRoles.append(PageIndexRole);
        if (entry->subpage != row.subpage)
            changedRoles.append(SubpageIndexRole);
        entry->widget = row.widget;
        entry->widgetID = row.widget->id();
        entry->widgetType = row.widget->type();
        entry->kind = row.kind;
        entry->pageIndex = row.pageIndex;
        entry->subpage = row.subpage;
        if (!changedRoles.isEmpty())
        {
            const QModelIndex index = nodeIndex(entry);
            emit dataChanged(index, index, changedRoles);
        }
        watch(row.widget);
        reconcile(entry, row.children);
    }
}

void VCOutlineModel::removeMissing(Node *owner, const QSet<QString> &keys)
{
    const auto retained = [&keys](auto &&self, const Node *entry) -> bool {
        if (keys.contains(entry->key))
            return true;
        for (const auto &child : entry->children)
            if (self(self, child.get()))
                return true;
        return false;
    };
    const auto unregister = [this](auto &&self, const Node *entry) -> void {
        for (const auto &child : entry->children)
            self(self, child.get());
        m_nodes.remove(entry->key);
    };
    for (int row = static_cast<int>(owner->children.size()) - 1; row >= 0; --row)
    {
        Node *entry = owner->children[static_cast<size_t>(row)].get();
        if (!retained(retained, entry))
        {
            beginRemoveRows(nodeIndex(owner), row, row);
            unregister(unregister, entry);
            owner->children.erase(owner->children.begin() + row);
            endRemoveRows();
        }
        else
            removeMissing(entry, keys);
    }
}

void VCOutlineModel::refresh()
{
    QSet<QString> keys;
    std::function<Row(VCWidget *, RowKind, int, int)> makeRow;
    makeRow = [&](VCWidget *widget, RowKind kind, int pageIndex, int subpage) {
        Row row{kind == SubpageRow ? QStringLiteral("subpage:%1:%2").arg(widget->id()).arg(subpage) :
                                    QStringLiteral("widget:%1").arg(widget->id()),
                widget, kind, pageIndex, subpage, {}};
        keys.insert(row.key);
        if (kind == SubpageRow)
            return row;
        if (auto *frame = qobject_cast<VCFrame *>(widget))
        {
            const QList<VCWidget *> children = frame->children(false);
            if (frame->multiPageMode())
            {
                QList<int> pages;
                for (int page = 0; page < frame->totalPagesNumber(); ++page)
                    pages.append(page);
                for (VCWidget *child : children)
                    if (!pages.contains(child->page()))
                        pages.append(child->page());
                std::sort(pages.begin(), pages.end());
                for (int page : pages)
                {
                    Row pageRow = makeRow(frame, SubpageRow, pageIndex, page);
                    for (VCWidget *child : children)
                        if (child->page() == page)
                            pageRow.children.push_back(makeRow(child, WidgetRow, pageIndex, page));
                    row.children.push_back(std::move(pageRow));
                }
            }
            else
            {
                for (VCWidget *child : children)
                    row.children.push_back(makeRow(child, WidgetRow, pageIndex, child->page()));
            }
        }
        return row;
    };
    std::vector<Row> rows;
    for (int page = 0; page < m_vc->pagesCount(); ++page)
        if (VCPage *frame = m_vc->page(page))
            rows.push_back(makeRow(frame, PageRow, page, 0));
    removeMissing(m_root.get(), keys);
    reconcile(m_root.get(), rows);
    removeMissing(m_root.get(), keys);
    ++m_revision;
    emit treeChanged();
}
