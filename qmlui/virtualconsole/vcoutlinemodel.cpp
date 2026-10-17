#include "vcoutlinemodel.h"

#include <QSet>
#include <QTimer>
#include <algorithm>
#include <functional>

#include "app.h"
#include "inputoutputmap.h"
#include "inputpatch.h"
#include "midiprotocol.h"
#include "outputpatch.h"
#include "qlcinputchannel.h"
#include "qlcinputprofile.h"
#include "vcframe.h"
#include "vcpage.h"
#include "vcwidget.h"
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
                          VCOutlineModel::WidgetIDRole, VCOutlineModel::AssignmentSearchRole })
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

class AssignmentRowsModel final : public QAbstractListModel
{
public:
    enum Role
    {
        ValuesRole = Qt::UserRole + 1
    };

    explicit AssignmentRowsModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
    {
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return QVariant();
        const QVariantMap &row = m_rows.at(index.row());
        if (role == Qt::DisplayRole || role == ValuesRole)
            return row;
        if (role == Qt::UserRole)
            return row.value(QStringLiteral("assignmentKey"));
        return row.value(QString::fromLatin1(roleNames().value(role)));
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {{ValuesRole, "values"}, {Qt::UserRole, "assignmentKey"}};
    }

    void setRows(const QVariantList &rows)
    {
        auto keyOf = [](const QVariantMap &row) { return row.value(QStringLiteral("assignmentKey")).toString(); };
        for (int index = 0; index < rows.size(); ++index)
        {
            const QVariantMap row = rows.at(index).toMap();
            const QString key = keyOf(row);
            int current = -1;
            for (int probe = 0; probe < m_rows.size(); ++probe)
            {
                if (keyOf(m_rows.at(probe)) == key)
                {
                    current = probe;
                    break;
                }
            }
            if (current < 0)
            {
                beginInsertRows(QModelIndex(), index, index);
                m_rows.insert(index, row);
                endInsertRows();
            }
            else
            {
                if (current != index)
                {
                    const int destination = current < index ? index + 1 : index;
                    beginMoveRows(QModelIndex(), current, current, QModelIndex(), destination);
                    m_rows.move(current, index);
                    endMoveRows();
                }
                if (m_rows.at(index) != row)
                {
                    m_rows[index] = row;
                    emit dataChanged(this->index(index, 0), this->index(index, 0),
                                     {Qt::DisplayRole, ValuesRole, Qt::UserRole});
                }
            }
        }
        while (m_rows.size() > rows.size())
        {
            const int last = m_rows.size() - 1;
            beginRemoveRows(QModelIndex(), last, last);
            m_rows.removeLast();
            endRemoveRows();
        }
    }

private:
    QList<QVariantMap> m_rows;
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
    , m_assignmentRowsModel(new AssignmentRowsModel(this))
{
    m_filter->setSourceModel(this);
    connect(vc, &VirtualConsole::pagesCountChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::widgetsChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::structureChanged, this, &VCOutlineModel::scheduleRefresh);
    connect(vc, &VirtualConsole::selectedWidgetsCountChanged, this, &VCOutlineModel::selectionChanged);
    m_diagnosticsTimer.setInterval(1000);
    m_diagnosticsTimer.setSingleShot(false);
    connect(&m_diagnosticsTimer, &QTimer::timeout, this, &VCOutlineModel::refreshAssignments);
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
    const bool synthetic = entry->kind == SubpageRow;
    switch (role)
    {
    case Qt::DisplayRole:
    case LabelRole:
        return nodeLabel(entry);
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
    case AssignmentSearchRole:
        return m_assignmentSearchText.value(entry->key);
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
        {ContainerRole, "isContainer"}, {OrderIndexRole, "orderIndex"},
        {AssignmentSearchRole, "assignmentSearch"}
    };
}

Qt::ItemFlags VCOutlineModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

QAbstractItemModel *VCOutlineModel::filteredModel() const { return m_filter; }
QString VCOutlineModel::filterText() const { return m_filterText; }
int VCOutlineModel::revision() const { return m_revision; }
bool VCOutlineModel::diagnosticsActive() const { return m_diagnosticsActive; }
QString VCOutlineModel::inspectionScopeKey() const { return m_scopeKey; }
QVariantMap VCOutlineModel::assignmentSummary() const { return m_assignmentSummary; }
QAbstractItemModel *VCOutlineModel::assignmentRows() const { return m_assignmentRowsModel; }

void VCOutlineModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    m_filterText = text;
    static_cast<OutlineFilter *>(m_filter)->setText(text);
    emit filterTextChanged();
    ++m_revision;
    emit treeChanged();
    refreshAssignments();
}

void VCOutlineModel::setDiagnosticsActive(bool active)
{
    if (active == m_diagnosticsActive)
        return;
    m_diagnosticsActive = active;
    if (m_diagnosticsActive)
    {
        refreshAssignments();
        m_diagnosticsTimer.start();
    }
    else
        m_diagnosticsTimer.stop();
    emit diagnosticsActiveChanged();
}

QString VCOutlineModel::normalizedScopeKey(const QString &key) const
{
    if (key.isEmpty())
        return QString();
    if (m_nodes.contains(key))
        return key;
    return fallbackScopeKey(key);
}

QString VCOutlineModel::fallbackScopeKey(const QString &missingKey) const
{
    if (m_nodes.contains(missingKey))
        return missingKey;
    for (const QString &key : m_scopeChain)
    {
        if (m_nodes.contains(key))
            return key;
    }
    return QString();
}

void VCOutlineModel::updateScopeChain(const QString &leafKey)
{
    m_scopeChain.clear();
    Node *entry = m_nodes.value(leafKey, nullptr);
    while (entry != nullptr)
    {
        m_scopeChain.append(entry->key);
        entry = entry->parent;
    }
}

void VCOutlineModel::setInspectionScopeKey(const QString &key)
{
    const QString normalized = normalizedScopeKey(key);
    if (normalized == m_scopeKey)
        return;
    m_scopeKey = normalized;
    updateScopeChain(m_scopeKey);
    emit inspectionScopeKeyChanged();
    refreshAssignments();
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
    if (!m_scopeKey.isEmpty() && !m_nodes.contains(m_scopeKey))
    {
        const QString fallback = fallbackScopeKey(m_scopeKey);
        if (fallback != m_scopeKey)
        {
            m_scopeKey = fallback;
            updateScopeChain(m_scopeKey);
            emit inspectionScopeKeyChanged();
        }
    }
    else if (!m_scopeKey.isEmpty())
        updateScopeChain(m_scopeKey);
    refreshAssignments();
    ++m_revision;
    emit treeChanged();
}

QVariantMap VCOutlineModel::emptySummary() const
{
    return {
        {"scopeKey", m_scopeKey},
        {"ownWidgetID", -1},
        {"assignedWidgetCount", 0},
        {"inputBindingCount", 0},
        {"ownAssignedWidgetCount", 0},
        {"ownInputBindingCount", 0},
        {"descendantAssignedWidgetCount", 0},
        {"descendantInputBindingCount", 0}
    };
}

QString VCOutlineModel::nodeLabel(const Node *entry) const
{
    if (entry == nullptr)
        return QString();
    if (entry->kind == SubpageRow)
    {
        const auto *frame = qobject_cast<const VCFrame *>(entry->widget.data());
        const QStringList labels = frame ? frame->pageLabels() : QStringList();
        if (entry->subpage >= 0 && entry->subpage < labels.size())
            return labels.at(entry->subpage);
        return tr("Page %1").arg(entry->subpage + 1);
    }
    if (entry->widget == nullptr)
        return tr("<Unavailable>");
    return entry->widget->caption().isEmpty() ? tr("<Unnamed>") : entry->widget->caption();
}

QString VCOutlineModel::nodePath(const Node *entry) const
{
    QStringList labels;
    const Node *current = entry;
    while (current != nullptr && current != m_root.get())
    {
        labels.prepend(nodeLabel(current));
        current = current->parent;
    }
    return labels.join(QStringLiteral(" / "));
}

void VCOutlineModel::refreshAssignments()
{
    if (!m_diagnosticsActive)
        return;

    auto toVariantMap = [](const QMap<QString, QVariant> &map) {
        QVariantMap values;
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            values.insert(it.key(), it.value());
        return values;
    };
    const auto midiMessageTypeToString = [](QLCMIDIProtocol::MidiInputMessageType type) {
        switch (type)
        {
        case QLCMIDIProtocol::MidiInputMessageType::ControlChange: return QStringLiteral("controlChange");
        case QLCMIDIProtocol::MidiInputMessageType::Note: return QStringLiteral("note");
        case QLCMIDIProtocol::MidiInputMessageType::NoteAftertouch: return QStringLiteral("noteAftertouch");
        case QLCMIDIProtocol::MidiInputMessageType::ProgramChange: return QStringLiteral("programChange");
        case QLCMIDIProtocol::MidiInputMessageType::ChannelAftertouch: return QStringLiteral("channelAftertouch");
        case QLCMIDIProtocol::MidiInputMessageType::PitchWheel: return QStringLiteral("pitchWheel");
        case QLCMIDIProtocol::MidiInputMessageType::BeatPlayback: return QStringLiteral("beatPlayback");
        case QLCMIDIProtocol::MidiInputMessageType::BeatClock: return QStringLiteral("beatClock");
        case QLCMIDIProtocol::MidiInputMessageType::BeatStop: return QStringLiteral("beatStop");
        case QLCMIDIProtocol::MidiInputMessageType::Unknown:
        default: return QStringLiteral("unknown");
        }
    };
    const auto midiSelectionStateToString = [](QLCMIDIProtocol::MidiChannelSelectionState state) {
        switch (state)
        {
        case QLCMIDIProtocol::MidiChannelSelectionState::Fixed: return QStringLiteral("fixed");
        case QLCMIDIProtocol::MidiChannelSelectionState::Omni: return QStringLiteral("omni");
        case QLCMIDIProtocol::MidiChannelSelectionState::Invalid: return QStringLiteral("invalid");
        case QLCMIDIProtocol::MidiChannelSelectionState::Unknown:
        default: return QStringLiteral("unknown");
        }
    };
    const auto midiFeedbackStateToString = [](QLCMIDIProtocol::MidiFeedbackChannelState state) {
        switch (state)
        {
        case QLCMIDIProtocol::MidiFeedbackChannelState::Fixed: return QStringLiteral("fixed");
        case QLCMIDIProtocol::MidiFeedbackChannelState::Omni: return QStringLiteral("omni");
        case QLCMIDIProtocol::MidiFeedbackChannelState::Invalid: return QStringLiteral("invalid");
        case QLCMIDIProtocol::MidiFeedbackChannelState::Unknown:
        default: return QStringLiteral("unknown");
        }
    };
    const auto selectionLabel = [](const QLCMIDIProtocol::MidiChannelSelectionDescriptor &selection) {
        switch (selection.state)
        {
        case QLCMIDIProtocol::MidiChannelSelectionState::Fixed:
            return QObject::tr("Channel %1").arg(selection.channelIndex + 1);
        case QLCMIDIProtocol::MidiChannelSelectionState::Omni:
            return QObject::tr("OMNI");
        case QLCMIDIProtocol::MidiChannelSelectionState::Invalid:
            return QObject::tr("Invalid");
        case QLCMIDIProtocol::MidiChannelSelectionState::Unknown:
        default:
            return QObject::tr("Unknown");
        }
    };
    const auto feedbackLabel = [](const QLCMIDIProtocol::MidiFeedbackChannelDescriptor &feedback) {
        switch (feedback.state)
        {
        case QLCMIDIProtocol::MidiFeedbackChannelState::Fixed:
            return QObject::tr("Channel %1").arg(feedback.channelIndex + 1);
        case QLCMIDIProtocol::MidiFeedbackChannelState::Omni:
            return QObject::tr("OMNI");
        case QLCMIDIProtocol::MidiFeedbackChannelState::Invalid:
            return QObject::tr("Invalid");
        case QLCMIDIProtocol::MidiFeedbackChannelState::Unknown:
        default:
            return QObject::tr("Unknown");
        }
    };
    const auto parseMidiChannelParam = [](const QVariantMap &params, const QString &key, bool midiRoute) {
        struct ParseResult
        {
            bool present = false;
            bool valid = false;
            int value = -1;
            QVariant raw;
        };

        ParseResult result;
        result.present = midiRoute && params.contains(key);
        if (!result.present)
            return result;

        result.raw = params.value(key);
        bool ok = false;
        const int parsed = result.raw.toInt(&ok);
        result.valid = ok;
        result.value = ok ? parsed : -1;
        return result;
    };
    const auto isMidiPlugin = [](const QString &pluginName) {
        return pluginName.compare(QStringLiteral("MIDI"), Qt::CaseInsensitive) == 0 ||
               pluginName.contains(QStringLiteral("midi"), Qt::CaseInsensitive);
    };

    struct InputRouteInfo
    {
        QString availability = QStringLiteral("unresolved");
        bool resolved = false;
        QString plugin;
        QString deviceName;
        int line = -1;
        QString uid;
        QString status;
        QVariantMap parameters;
        QString profileName;
        QLCInputProfile *profile = nullptr;
        bool pluginIsMidi = false;
    };

    struct OutputRouteInfo
    {
        QString availability = QStringLiteral("unresolved");
        bool resolved = false;
        QString plugin;
        QString deviceName;
        int line = -1;
        QString uid;
        QVariantMap parameters;
        bool pluginIsMidi = false;
    };

    auto *app = qobject_cast<App *>(m_vc->view());
    Doc *doc = app ? app->doc() : nullptr;
    InputOutputMap *ioMap = doc ? doc->inputOutputMap() : nullptr;
    const quint32 universesCount = ioMap ? ioMap->universesCount() : 0;
    QHash<quint32, InputRouteInfo> inputRoutes;
    QHash<quint32, OutputRouteInfo> outputRoutes;
    const auto resolveInputRoute = [&](quint32 universe) {
        InputRouteInfo info;
        if (ioMap == nullptr)
        {
            info.availability = QStringLiteral("unknown");
            return info;
        }
        if (universe >= universesCount)
        {
            info.availability = QStringLiteral("invalidUniverse");
            return info;
        }
        InputPatch *patch = ioMap->inputPatch(universe);
        if (patch == nullptr || !patch->isPatched())
            return info;

        info.availability = QStringLiteral("resolved");
        info.resolved = true;
        info.plugin = patch->pluginName();
        info.pluginIsMidi = isMidiPlugin(info.plugin);
        info.deviceName = patch->inputName();
        info.line = int(patch->input());
        info.uid = patch->inputUID();
        info.profileName = patch->profileName();
        info.status = ioMap->inputPluginStatus(info.plugin, patch->input());
        info.parameters = toVariantMap(patch->getPluginParameters());
        info.profile = patch->profile();
        return info;
    };
    const auto resolveOutputRoute = [&](quint32 universe) {
        OutputRouteInfo info;
        if (ioMap == nullptr)
        {
            info.availability = QStringLiteral("unknown");
            return info;
        }
        if (universe >= universesCount)
        {
            info.availability = QStringLiteral("invalidUniverse");
            return info;
        }
        OutputPatch *patch = ioMap->feedbackPatch(universe);
        if (patch == nullptr || !patch->isPatched())
            return info;

        info.availability = QStringLiteral("resolved");
        info.resolved = true;
        info.plugin = patch->pluginName();
        info.pluginIsMidi = isMidiPlugin(info.plugin);
        info.deviceName = patch->outputName();
        info.line = int(patch->output());
        info.uid = patch->outputUID();
        info.parameters = toVariantMap(patch->getPluginParameters());
        return info;
    };

    QVariantList rows;
    QHash<QString, QStringList> searchTokens;
    QHash<QString, int> sharedSourceOccurrences;
    auto appendSearchToken = [&](const QString &key, const QString &token) {
        if (token.isEmpty())
            return;
        searchTokens[key].append(token);
    };

    for (Node *entry : m_nodes)
    {
        if (entry == nullptr || entry->widget == nullptr ||
            (entry->kind != WidgetRow && entry->kind != PageRow))
            continue;

        const QString key = entry->key;
        const QString path = nodePath(entry);
        const QList<QSharedPointer<QLCInputSource>> sources = entry->widget->inputSources();
        int occurrence = 0;
        for (const QSharedPointer<QLCInputSource> &source : sources)
        {
            if (source.isNull())
                continue;

            const quint32 sourceUniverse = source->universe();
            const quint32 packedChannel = source->channel();
            const quint32 sourceChannel = packedChannel & 0xFFFF;
            const quint32 sourcePage = source->page();
            const bool sourceValid = source->isValid();
            if (!inputRoutes.contains(sourceUniverse))
                inputRoutes.insert(sourceUniverse, resolveInputRoute(sourceUniverse));
            if (!outputRoutes.contains(sourceUniverse))
                outputRoutes.insert(sourceUniverse, resolveOutputRoute(sourceUniverse));
            const InputRouteInfo inputRoute = inputRoutes.value(sourceUniverse);
            const OutputRouteInfo outputRoute = outputRoutes.value(sourceUniverse);
            const bool midiInputRoute = inputRoute.resolved && inputRoute.pluginIsMidi;
            const bool midiOutputRoute = outputRoute.resolved && outputRoute.pluginIsMidi;
            const QString midiChannelKey = QStringLiteral("midichannel");
            const auto inputMidiParam = parseMidiChannelParam(inputRoute.parameters, midiChannelKey, midiInputRoute);
            const auto outputMidiParam = parseMidiChannelParam(outputRoute.parameters, midiChannelKey, midiOutputRoute);
            const int feedbackLowerModifier = source->feedbackExtraParams(QLCInputFeedback::LowerValue).toInt();
            const int feedbackUpperModifier = source->feedbackExtraParams(QLCInputFeedback::UpperValue).toInt();
            const int feedbackMonitorModifier = source->feedbackExtraParams(QLCInputFeedback::MonitorValue).toInt();
            const QLCMIDIProtocol::MidiInputChannelDescriptor midiInput = midiInputRoute ?
                QLCMIDIProtocol::describeInputChannel(sourceChannel) :
                QLCMIDIProtocol::MidiInputChannelDescriptor();
            const QLCMIDIProtocol::MidiChannelSelectionDescriptor midiSelection =
                QLCMIDIProtocol::describeChannelSelection(midiInputRoute, inputMidiParam.present, inputMidiParam.value);
            const QLCMIDIProtocol::MidiChannelSelectionDescriptor outputSelection =
                QLCMIDIProtocol::describeChannelSelection(midiOutputRoute, outputMidiParam.present, outputMidiParam.value);
            const QLCMIDIProtocol::MidiFeedbackChannelDescriptor midiFeedbackLower =
                QLCMIDIProtocol::describeFeedbackChannel(outputSelection, feedbackLowerModifier);
            const QLCMIDIProtocol::MidiFeedbackChannelDescriptor midiFeedbackUpper =
                QLCMIDIProtocol::describeFeedbackChannel(outputSelection, feedbackUpperModifier);
            const QLCMIDIProtocol::MidiFeedbackChannelDescriptor midiFeedbackMonitor =
                QLCMIDIProtocol::describeFeedbackChannel(outputSelection, feedbackMonitorModifier);
            QLCMIDIProtocol::MidiFeedbackChannelDescriptor midiFeedback;
            if (midiFeedbackLower.state == midiFeedbackUpper.state &&
                midiFeedbackLower.channelIndex == midiFeedbackUpper.channelIndex &&
                midiFeedbackLower.state == midiFeedbackMonitor.state &&
                midiFeedbackLower.channelIndex == midiFeedbackMonitor.channelIndex)
            {
                midiFeedback = midiFeedbackUpper;
            }
            else
            {
                midiFeedback.state = QLCMIDIProtocol::MidiFeedbackChannelState::Unknown;
                midiFeedback.channelIndex = 0;
            }

            const QString inputAvailability = sourceValid ? inputRoute.availability : QStringLiteral("unresolved");
            const QString outputAvailability = sourceValid ? outputRoute.availability : QStringLiteral("unresolved");
            const QString sourceIdentity = midiInputRoute ?
                QStringLiteral("midi:%1:%2:%3:%4")
                    .arg(sourceUniverse)
                    .arg(int(midiInput.type))
                    .arg(midiInput.hasMessageValue ? midiInput.messageValue : -1)
                    .arg(midiInput.packedMidiChannelIndex) :
                QStringLiteral("generic:%1:%2")
                    .arg(sourceUniverse)
                    .arg(sourceChannel);
            sharedSourceOccurrences[sourceIdentity] += 1;
            const QString midiMessageLabel = midiInputRoute ?
                QStringLiteral("%1 %2")
                    .arg(midiMessageTypeToString(midiInput.type))
                    .arg(midiInput.hasMessageValue ? QString::number(midiInput.messageValue) : QStringLiteral("-")) :
                QStringLiteral("Unknown");
            QString midiSelectionLabel = selectionLabel(midiSelection);
            if (midiSelection.state == QLCMIDIProtocol::MidiChannelSelectionState::Omni)
            {
                if (midiInputRoute)
                    midiSelectionLabel = QObject::tr("OMNI (assigned channel %1)").arg(midiInput.packedMidiChannelIndex + 1);
            }
            const QString outputSelectionLabel = selectionLabel(outputSelection);
            const QString midiFeedbackLowerLabel = feedbackLabel(midiFeedbackLower);
            const QString midiFeedbackUpperLabel = feedbackLabel(midiFeedbackUpper);
            const QString midiFeedbackMonitorLabel = feedbackLabel(midiFeedbackMonitor);
            const QString midiFeedbackLabel = midiFeedback.state == QLCMIDIProtocol::MidiFeedbackChannelState::Unknown &&
                                              (midiFeedbackLowerLabel != midiFeedbackUpperLabel ||
                                               midiFeedbackLowerLabel != midiFeedbackMonitorLabel) ?
                QObject::tr("Lower %1, Upper %2, Monitor %3")
                    .arg(midiFeedbackLowerLabel, midiFeedbackUpperLabel, midiFeedbackMonitorLabel) :
                feedbackLabel(midiFeedback);
            QString sourceLabel = QStringLiteral("U%1 CH%2").arg(sourceUniverse + 1).arg(sourceChannel + 1);
            if (ioMap != nullptr)
            {
                QString uniName;
                QString chName;
                if (ioMap->inputSourceNames(source, uniName, chName))
                    sourceLabel = QStringLiteral("%1 / %2").arg(uniName, chName);
            }
            if (midiInputRoute && inputRoute.profile != nullptr)
            {
                const QLCInputChannel *control = inputRoute.profile->channel(midiInput.qlcChannel);
                if (control != nullptr)
                    sourceLabel = QStringLiteral("%1 / %2").arg(sourceLabel, control->name());
            }
            QString deviceGroupKey = QStringLiteral("input:unresolved");
            QString deviceGroupLabel = tr("Unresolved input route");
            if (inputAvailability == QStringLiteral("invalidUniverse"))
            {
                deviceGroupKey = QStringLiteral("input:invalid:%1").arg(sourceUniverse);
                deviceGroupLabel = tr("Invalid universe U%1").arg(sourceUniverse + 1);
            }
            else if (inputRoute.resolved)
            {
                deviceGroupKey = QStringLiteral("input:%1:%2:%3")
                    .arg(inputRoute.plugin, QString::number(inputRoute.line), inputRoute.uid);
                deviceGroupLabel = inputRoute.deviceName.isEmpty() ?
                    QStringLiteral("%1 #%2").arg(inputRoute.plugin).arg(inputRoute.line + 1) :
                    inputRoute.deviceName;
            }

            QVariantMap row{
                {"assignmentKey", QStringLiteral("%1:%2").arg(key).arg(occurrence++)},
                {"widgetID", entry->widgetID},
                {"widgetKey", key},
                {"widgetPath", path},
                {"sourceID", source->id()},
                {"sourceUniverse", int(sourceUniverse)},
                {"sourceChannelRaw", int(packedChannel)},
                {"sourceChannel", int(sourceChannel)},
                {"sourcePage", int(sourcePage)},
                {"sourceIdentity", sourceIdentity},
                {"sourceLabel", sourceLabel},
                {"deviceGroupKey", deviceGroupKey},
                {"deviceGroupLabel", deviceGroupLabel},
                {"deviceGroupStart", false},
                {"inputDeviceName", inputRoute.deviceName},
                {"inputPlugin", inputRoute.plugin},
                {"inputLine", inputRoute.line},
                {"inputUID", inputRoute.uid},
                {"inputStatus", inputRoute.status},
                {"inputParameters", inputRoute.parameters},
                {"inputProfileName", inputRoute.profileName},
                {"outputDeviceName", outputRoute.deviceName},
                {"outputPlugin", outputRoute.plugin},
                {"outputLine", outputRoute.line},
                {"outputUID", outputRoute.uid},
                {"outputParameters", outputRoute.parameters},
                {"inputRouteAvailability", inputAvailability},
                {"outputRouteAvailability", outputAvailability},
                {"midiRouteResolved", midiInputRoute},
                {"midiOutputRouteResolved", midiOutputRoute},
                {"midiMessageType", midiMessageTypeToString(midiInput.type)},
                {"midiMessageLabel", midiMessageLabel},
                {"midiQlcChannel", int(midiInput.qlcChannel)},
                {"midiPackedChannelIndex", int(midiInput.packedMidiChannelIndex)},
                {"midiMessageValue", int(midiInput.messageValue)},
                {"midiHasMessageValue", midiInput.hasMessageValue},
                {"midiSelectionState", midiSelectionStateToString(midiSelection.state)},
                {"midiSelectionLabel", midiSelectionLabel},
                {"midiSelectionChannelIndex", int(midiSelection.channelIndex)},
                {"midiOutputSelectionState", midiSelectionStateToString(outputSelection.state)},
                {"midiOutputSelectionLabel", outputSelectionLabel},
                {"midiOutputSelectionChannelIndex", int(outputSelection.channelIndex)},
                {"midiParameterPresent", inputMidiParam.present},
                {"midiParameterValue", inputMidiParam.present ? inputMidiParam.raw : QVariant()},
                {"midiParameterValid", inputMidiParam.valid},
                {"midiOutputParameterPresent", outputMidiParam.present},
                {"midiOutputParameterValue", outputMidiParam.present ? outputMidiParam.raw : QVariant()},
                {"midiOutputParameterValid", outputMidiParam.valid},
                {"midiFeedbackState", midiFeedbackStateToString(midiFeedback.state)},
                {"midiFeedbackLabel", midiFeedbackLabel},
                {"midiFeedbackChannelIndex", int(midiFeedback.channelIndex)},
                {"midiFeedbackModifier", feedbackUpperModifier},
                {"midiFeedbackLowerState", midiFeedbackStateToString(midiFeedbackLower.state)},
                {"midiFeedbackLowerLabel", midiFeedbackLowerLabel},
                {"midiFeedbackLowerChannelIndex", int(midiFeedbackLower.channelIndex)},
                {"midiFeedbackUpperState", midiFeedbackStateToString(midiFeedbackUpper.state)},
                {"midiFeedbackUpperLabel", midiFeedbackUpperLabel},
                {"midiFeedbackUpperChannelIndex", int(midiFeedbackUpper.channelIndex)},
                {"midiFeedbackMonitorState", midiFeedbackStateToString(midiFeedbackMonitor.state)},
                {"midiFeedbackMonitorLabel", midiFeedbackMonitorLabel},
                {"midiFeedbackMonitorChannelIndex", int(midiFeedbackMonitor.channelIndex)},
                {"feedbackLowerValue", int(source->feedbackValue(QLCInputFeedback::LowerValue))},
                {"feedbackUpperValue", int(source->feedbackValue(QLCInputFeedback::UpperValue))},
                {"feedbackMonitorValue", int(source->feedbackValue(QLCInputFeedback::MonitorValue))},
                {"feedbackLowerParams", source->feedbackExtraParams(QLCInputFeedback::LowerValue)},
                {"feedbackUpperParams", source->feedbackExtraParams(QLCInputFeedback::UpperValue)},
                {"feedbackMonitorParams", source->feedbackExtraParams(QLCInputFeedback::MonitorValue)}
            };

            const QString searchable = QStringList{
                path,
                VCWidget::typeToString(entry->widgetType),
                QString::number(entry->widgetID),
                row.value("sourceLabel").toString(),
                QStringLiteral("slot %1").arg(source->id()),
                row.value("inputProfileName").toString(),
                row.value("inputDeviceName").toString(),
                row.value("inputPlugin").toString(),
                row.value("inputUID").toString(),
                QString::number(row.value("inputLine").toInt()),
                row.value("inputStatus").toString(),
                row.value("outputDeviceName").toString(),
                row.value("outputPlugin").toString(),
                row.value("outputUID").toString(),
                QString::number(row.value("outputLine").toInt()),
                QStringLiteral("u%1").arg(sourceUniverse),
                QStringLiteral("packed %1").arg(packedChannel),
                QStringLiteral("ch %1").arg(sourceChannel),
                QStringLiteral("subpage %1").arg(sourcePage),
                midiMessageLabel,
                midiSelectionLabel,
                outputSelectionLabel,
                midiFeedbackLowerLabel,
                midiFeedbackUpperLabel,
                midiFeedbackMonitorLabel,
                QString::number(feedbackLowerModifier),
                QString::number(feedbackUpperModifier),
                QString::number(feedbackMonitorModifier),
                row.value("deviceGroupLabel").toString()
            }.join(QLatin1Char(' '));
            row.insert(QStringLiteral("searchText"), searchable);
            rows.append(row);

            for (Node *cursor = entry; cursor != nullptr && cursor != m_root.get(); cursor = cursor->parent)
                appendSearchToken(cursor->key, searchable);
        }
    }

    QSet<quint32> scopeWidgets;
    quint32 ownWidgetID = VCWidget::invalidId();
    Node *scope = m_scopeKey.isEmpty() ? m_root.get() : m_nodes.value(m_scopeKey, nullptr);
    if (scope == nullptr)
        scope = m_root.get();
    if ((scope->kind == WidgetRow || scope->kind == PageRow) && scope->widget)
        ownWidgetID = scope->widgetID;

    const auto collectScope = [&](auto &&self, Node *entry) -> void {
        if (entry == nullptr)
            return;
        if ((entry->kind == WidgetRow || entry->kind == PageRow) && entry->widget)
            scopeWidgets.insert(entry->widgetID);
        for (const auto &child : entry->children)
            self(self, child.get());
    };
    collectScope(collectScope, scope);

    QVariantList scopedRows;
    QSet<quint32> scopeAssignedWidgets;
    QSet<quint32> ownAssignedWidgets;
    int ownBindings = 0;
    for (QVariant &value : rows)
    {
        QVariantMap row = value.toMap();
        const quint32 widgetID = row.value(QStringLiteral("widgetID")).toUInt();
        if (!scopeWidgets.contains(widgetID))
            continue;
        const QString sourceIdentity = row.value(QStringLiteral("sourceIdentity")).toString();
        const bool sharedAssignment = sharedSourceOccurrences.value(sourceIdentity) > 1;
        row.insert(QStringLiteral("sharedAssignment"), sharedAssignment);
        row.insert(QStringLiteral("sharedAssignmentLabel"), sharedAssignment ? QStringLiteral("Shared physical input address")
                                                                             : QStringLiteral("Not observed shared"));
        const bool own = ownWidgetID != VCWidget::invalidId() && ownWidgetID == widgetID;
        row.insert(QStringLiteral("scopeOwn"), own);
        scopedRows.append(row);
        scopeAssignedWidgets.insert(widgetID);
        if (own)
        {
            ownAssignedWidgets.insert(widgetID);
            ++ownBindings;
        }
    }
    std::sort(scopedRows.begin(), scopedRows.end(), [](const QVariant &lhs, const QVariant &rhs) {
        const QVariantMap left = lhs.toMap();
        const QVariantMap right = rhs.toMap();
        const QString leftGroup = left.value(QStringLiteral("deviceGroupKey")).toString();
        const QString rightGroup = right.value(QStringLiteral("deviceGroupKey")).toString();
        if (leftGroup != rightGroup)
            return leftGroup < rightGroup;
        const int leftLine = left.value(QStringLiteral("inputLine")).toInt();
        const int rightLine = right.value(QStringLiteral("inputLine")).toInt();
        if (leftLine != rightLine)
            return leftLine < rightLine;
        const QString leftPath = left.value(QStringLiteral("widgetPath")).toString();
        const QString rightPath = right.value(QStringLiteral("widgetPath")).toString();
        if (leftPath != rightPath)
            return leftPath < rightPath;
        const QString leftIdentity = left.value(QStringLiteral("sourceIdentity")).toString();
        const QString rightIdentity = right.value(QStringLiteral("sourceIdentity")).toString();
        if (leftIdentity != rightIdentity)
            return leftIdentity < rightIdentity;
        return left.value(QStringLiteral("sourceID")).toUInt() < right.value(QStringLiteral("sourceID")).toUInt();
    });

    QVariantMap summary{
        {"scopeKey", scope == m_root.get() ? QString() : scope->key},
        {"ownWidgetID", ownWidgetID == VCWidget::invalidId() ? -1 : int(ownWidgetID)},
        {"assignedWidgetCount", scopeAssignedWidgets.size()},
        {"inputBindingCount", scopedRows.size()},
        {"ownAssignedWidgetCount", ownAssignedWidgets.size()},
        {"ownInputBindingCount", ownBindings},
        {"descendantAssignedWidgetCount", scopeAssignedWidgets.size() - ownAssignedWidgets.size()},
        {"descendantInputBindingCount", scopedRows.size() - ownBindings}
    };

    QHash<QString, QString> searchText;
    for (auto it = searchTokens.cbegin(); it != searchTokens.cend(); ++it)
        searchText.insert(it.key(), it.value().join(QLatin1Char(' ')));

    QVariantList visibleRows;
    visibleRows.reserve(scopedRows.size());
    QString lastGroupKey;
    for (const QVariant &value : scopedRows)
    {
        QVariantMap row = value.toMap();
        const QString rowSearchText = row.value(QStringLiteral("searchText")).toString();
        if (!m_filterText.isEmpty() && !rowSearchText.contains(m_filterText, Qt::CaseInsensitive))
            continue;
        const QString groupKey = row.value(QStringLiteral("deviceGroupKey")).toString();
        const bool startsGroup = visibleRows.isEmpty() || groupKey != lastGroupKey;
        row.insert(QStringLiteral("deviceGroupStart"), startsGroup);
        lastGroupKey = groupKey;
        visibleRows.append(row);
    }

    if (summary == m_assignmentSummary && visibleRows == m_assignmentRowsCache &&
        searchText == m_assignmentSearchText)
        return;

    QSet<QString> changedKeys;
    for (auto it = m_assignmentSearchText.cbegin(); it != m_assignmentSearchText.cend(); ++it)
    {
        if (searchText.value(it.key()) != it.value())
            changedKeys.insert(it.key());
    }
    for (auto it = searchText.cbegin(); it != searchText.cend(); ++it)
    {
        if (m_assignmentSearchText.value(it.key()) != it.value())
            changedKeys.insert(it.key());
    }

    m_assignmentSummary = summary;
    m_assignmentRowsCache = visibleRows;
    m_assignmentSearchText = searchText;
    static_cast<AssignmentRowsModel *>(m_assignmentRowsModel)->setRows(visibleRows);
    emit assignmentSummaryChanged();

    for (const QString &key : changedKeys)
    {
        Node *entry = m_nodes.value(key, nullptr);
        if (entry == nullptr)
            continue;
        const QModelIndex index = nodeIndex(entry);
        emit dataChanged(index, index, {AssignmentSearchRole});
    }
    if (!m_filterText.isEmpty())
    {
        ++m_revision;
        emit treeChanged();
    }
}
