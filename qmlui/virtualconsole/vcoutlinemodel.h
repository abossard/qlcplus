#ifndef VCOUTLINEMODEL_H
#define VCOUTLINEMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QVariantMap>
#include <memory>
#include <vector>

class VirtualConsole;
class VCWidget;

class VCOutlineModel final : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *filteredModel READ filteredModel CONSTANT)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int revision READ revision NOTIFY treeChanged)

public:
    enum Role
    {
        LabelRole = Qt::UserRole + 1,
        WidgetIDRole, WidgetTypeRole, TypeNameRole, RowKindRole, PageIndexRole,
        SubpageIndexRole, ParentIDRole, SelectedRole, IconSourceRole, RowKeyRole,
        ParentKeyRole, ContainerRole, OrderIndexRole
    };
    enum RowKind { PageRow, WidgetRow, SubpageRow };
    Q_ENUM(RowKind)

    explicit VCOutlineModel(VirtualConsole *vc, QObject *parent = nullptr);
    ~VCOutlineModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QAbstractItemModel *filteredModel() const;
    QString filterText() const;
    void setFilterText(const QString &text);
    int revision() const;
    Q_INVOKABLE QModelIndex indexForKey(const QString &key) const;
    Q_INVOKABLE QVariantMap rowData(const QString &key) const;
    Q_INVOKABLE QStringList visibleKeys(const QStringList &expandedKeys) const;
    Q_INVOKABLE QVariantList visibleWidgetIDs(const QStringList &expandedKeys) const;

signals:
    void filterTextChanged();
    void treeChanged();

private:
    struct Node;
    struct Row;
    QModelIndex nodeIndex(const Node *node) const;
    Node *node(const QModelIndex &index) const;
    int nodeRow(const Node *node) const;
    void scheduleRefresh();
    void refresh();
    void reconcile(Node *parent, const std::vector<Row> &rows);
    void removeMissing(Node *parent, const QSet<QString> &keys);
    void watch(VCWidget *widget);
    void widgetDataChanged(VCWidget *widget, const QList<int> &roles);
    void selectionChanged();
    void collectVisible(const QModelIndex &parent, const QSet<QString> &expanded, QStringList &keys) const;

    VirtualConsole *m_vc;
    std::unique_ptr<Node> m_root;
    QHash<QString, Node *> m_nodes;
    QHash<VCWidget *, QList<QMetaObject::Connection>> m_connections;
    QSortFilterProxyModel *m_filter;
    QString m_filterText;
    int m_revision = 0;
    bool m_refreshPending = false;
    QSet<quint32> m_selectedIDs;
    QSet<quint32> m_selectionChanges;
    bool m_selectionPending = false;
};

#endif
