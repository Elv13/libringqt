/************************************************************************************
 *   Copyright (C) 2017-2018 by BlueSystems GmbH                                    *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                            *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "peerstimelinemodel.h"

// libstdc++
#include <algorithm>

// Qt
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QDateTime>

// Ring
#include <individual.h>
#include <session.h>
#include <individualdirectory.h>
#include <historytimecategorymodel.h>

#define NEVER static_cast<int>(HistoryTimeCategoryModel::HistoryConst::Never)
class SummaryModel;

struct ITLNode final
{
    enum InsertStatus {
        NEW = -1
    };

    explicit ITLNode(Individual* ind, time_t t=0):m_pInd(ind),m_Time(t) {}
    int         m_Index   { -1      };
    time_t      m_Time    {  0      };
    int         m_CatHead { -1      };
    Individual* m_pInd    { nullptr };
};

class PeersTimelineModelPrivate final : public QObject
{
    Q_OBJECT
public:
    // Attributes
    bool                               m_IsInit        { false };
    SummaryModel*                      m_pSummaryModel {nullptr};
    QSharedPointer<QAbstractItemModel> m_SummaryPtr    {nullptr};
    std::vector<ITLNode*>              m_lRows         {       };
    QHash<Individual*, ITLNode*>       m_hMapping      {       };
    std::vector<ITLNode*>              m_lSummaryHead  {       };

    // Helpers
    int init();
    inline void debugState();

    // Invert the list access to maximize the entropy of the end of the list.
    // The has a better performance profile when you consider the recently
    // used CMs have the higher chances of being used again.
    inline int realIndex(int effective) {return m_lRows.size() - effective - 1;}

    // Get the next index *based on the current list*
    std::vector<ITLNode*>::iterator getNextIndex(time_t t);

    PeersTimelineModel* q_ptr;

public Q_SLOTS:
    void slotLatestUsageChanged( Individual* cm, time_t t            );
    void slotIndividualAdded   ( Individual* ind                     );
    void slotDataChanged       ( Individual* i                       );
    void slotIndividualMerged  ( Individual*, Individual* i = nullptr);
};

/// Create a categorized "table of content" of the entries
class SummaryModel final : public HistoryTimeCategoryModel
{
    Q_OBJECT
public:
    explicit SummaryModel(PeersTimelineModelPrivate* parent);
    virtual ~SummaryModel();
    virtual QVariant data(const QModelIndex& idx, int role ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;
    void updateCategories(ITLNode* node, time_t t);

    PeersTimelineModelPrivate* d_ptr;
public slots:
    void reloadCategories();
};

/// Remove the empty timeline categories
class SummaryModelProxy : public QSortFilterProxyModel
{
public:
    explicit SummaryModelProxy(PeersTimelineModelPrivate* parent) :
        QSortFilterProxyModel(), d_ptr(parent) {}

    PeersTimelineModelPrivate* d_ptr;
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex & srcParent ) const override;
};

PeersTimelineModel::PeersTimelineModel() : QAbstractTableModel(QCoreApplication::instance()),
    d_ptr(new PeersTimelineModelPrivate())
{
    d_ptr->q_ptr = this;

    connect(this, &PeersTimelineModel::individualAdded,
        d_ptr, &PeersTimelineModelPrivate::slotIndividualAdded, Qt::QueuedConnection);
    connect(Session::instance()->individualDirectory(), &IndividualDirectory::individualChanged,
        d_ptr, &PeersTimelineModelPrivate::slotDataChanged, Qt::QueuedConnection);
    connect(this, &PeersTimelineModel::individualMerged,
        d_ptr, &PeersTimelineModelPrivate::slotIndividualMerged, Qt::QueuedConnection);
    connect(this, &PeersTimelineModel::lastUsedIndividualChanged,
        d_ptr, &PeersTimelineModelPrivate::slotLatestUsageChanged, Qt::QueuedConnection);
    connect(this, SIGNAL(selfRemoved(Individual*)),
        d_ptr, SLOT(slotIndividualMerged(Individual*)), Qt::QueuedConnection);
}

PeersTimelineModel::~PeersTimelineModel()
{
    qDeleteAll(d_ptr->m_hMapping);
    delete d_ptr;
}

QHash<int,QByteArray> PeersTimelineModel::roleNames() const
{
    return Session::instance()->individualDirectory()->roleNames();
}

QVariant PeersTimelineModel::data( const QModelIndex& idx, int role) const
{
    if ((!idx.isValid()) || (idx.column() && role != Qt::DisplayRole))
        return {};

    const auto node = d_ptr->m_lRows[d_ptr->realIndex(idx.row())];

    switch(idx.column()) {
        case 1:
            return node->m_pInd->isSelf();
        case 2:
            return (int)node->m_Time;
        case 3:
            return (int)node->m_pInd->lastUsedTime();
    }

    return node->m_pInd->roleData(role);
}

int PeersTimelineModel::rowCount( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d_ptr->init();
}

int PeersTimelineModel::columnCount( const QModelIndex& parent) const
{
    d_ptr->init();
    return parent.isValid() ? 0 : 4;
}

QModelIndex PeersTimelineModel::index(int row, int col, const QModelIndex& p) const
{
    return (row < 0 || row >= (int)d_ptr->m_lRows.size() || p.isValid() || col > 3) ?
        QModelIndex() : createIndex(row, col, d_ptr->m_lRows[d_ptr->realIndex(row)]);
}

/// Get the best position for the CM
std::vector<ITLNode*>::iterator PeersTimelineModelPrivate::getNextIndex(time_t t)
{
    if (m_lRows.empty())
        return m_lRows.begin();

    static ITLNode fake(nullptr);
    fake.m_Time = static_cast<long int>(t);

    return std::upper_bound(m_lRows.begin(), m_lRows.end(), &fake,
        [](const ITLNode* a, const ITLNode* t2) -> bool {
            return a->m_Time < t2->m_Time;
    });
}

/// Extra code for the integration tests (slow for-loop)
void PeersTimelineModelPrivate::debugState()
{
#ifdef ENABLE_TEST_ASSERTS
    if (m_lRows.empty()) return;
    bool correct(true), correct2(true), correct3(true);
    for (uint i = 0; i < m_lRows.size()  - 1; i++) {
        correct  &= m_lRows[i]->m_Time  <= m_lRows[i+1]->m_Time;
        correct2 &= m_lRows[i]->m_Index == m_lRows[i+1]->m_Index+1;
        correct3 &= m_lRows[i]->m_Index == (int) realIndex(i);
    }
    Q_ASSERT(correct );
    Q_ASSERT(correct2);
    Q_ASSERT(correct3);
#endif
}

/**
 * Insert or move contact methods in the model
 */
void PeersTimelineModelPrivate::slotLatestUsageChanged(Individual* ind, time_t t)
{
    // Avoid duplicated map lookups
    if (ind->masterObject() != ind || ind->isSelf())
        return;

    auto i = m_hMapping.value(ind);
    Q_ASSERT(i);

    if ((!m_IsInit) || (i->m_Index != ITLNode::NEW && t <= i->m_Time))
        return;

    const auto it    = getNextIndex(i->m_Time = t);
    const auto dtEnd = std::distance(it, m_lRows.end());

    // Need to be done before otherwise the category might not exist
    if (m_pSummaryModel)
        m_pSummaryModel->updateCategories(i, t);

    if (i->m_Index == ITLNode::NEW || it == m_lRows.begin()) {
        i->m_Index = dtEnd;
        q_ptr->beginInsertRows({}, dtEnd, dtEnd);
        std::for_each(m_lRows.begin(), it, [](ITLNode* n) {n->m_Index++;});
        m_lRows.insert(it, i);
        q_ptr->endInsertRows();
    } else {
        const auto curIt = m_lRows.begin() + realIndex(i->m_Index);
        const int start = std::distance(curIt, m_lRows.end()) -1;

        // The item is already where it belongs
        if (start == dtEnd) return;

        Q_ASSERT(m_lRows[m_lRows.size()-1 - start] == i);
        Q_ASSERT(curIt+1 < it);

        q_ptr->beginMoveRows({}, start, start, {}, dtEnd);
        std::for_each(curIt+1, it, [](ITLNode* n) {n->m_Index++;});
        std::rotate(curIt, curIt+1, it);
        i->m_Index = dtEnd;
        q_ptr->endMoveRows();
    }

    if (!dtEnd)
        emit q_ptr->headChanged();

    debugState();
}

void PeersTimelineModelPrivate::
slotIndividualAdded(Individual* ind)
{
    if (ind->isDuplicate() || ind->isSelf())
        return;

    m_hMapping[ind] = new ITLNode(ind);
    slotLatestUsageChanged(ind, ind->lastUsedTime());
}

void PeersTimelineModelPrivate::
slotDataChanged(Individual* ind)
{
    if (ind->isDuplicate() || ind->isSelf())
        return;

    const auto i = m_hMapping.value(ind->masterObject());
    if (!i) return;

    const auto idx = q_ptr->index(i->m_Index, 0);
    emit q_ptr->dataChanged(idx, idx);
}

/// Remove the duplicated CMs from the timeline
void PeersTimelineModelPrivate::slotIndividualMerged(Individual* old, Individual* into)
{
    Q_ASSERT((!into) || (old->isDuplicate() && !into->isDuplicate() && into->masterObject() == into));
    auto entry = m_hMapping.take(old);

    if ((!m_IsInit) && entry)
        delete entry;

    if ((!m_IsInit) || !entry)
        return;

    auto realIdx = m_lRows.begin()+realIndex(entry->m_Index);
    Q_ASSERT(entry == *realIdx);

    q_ptr->beginRemoveRows({}, entry->m_Index, entry->m_Index);
    std::for_each(m_lRows.begin(), realIdx, [](ITLNode* n) { n->m_Index--; });
    m_lRows.erase(realIdx);

    // Not as efficient as it can be, but simple and rare
    if (entry->m_CatHead != -1 && m_pSummaryModel)
        m_pSummaryModel->reloadCategories();

    q_ptr->endRemoveRows();

    if (into)
        slotLatestUsageChanged(into, into->lastUsedTime());

    delete entry;

    debugState();
}

// Moving elements requires rotating the m_lRows vector. It is very expensive
// and happens a lot of time during initialization. However it's useless as
// long as nothing is displayed. This method allows to delay and batch those
// rotation after initialization.
int PeersTimelineModelPrivate::init()
{
    if (m_IsInit)
        return m_lRows.size();

    m_IsInit = true;

    // Create a QList for the easy insertion, then convert it to a model
    QMultiMap<time_t, ITLNode*> map;

    for (auto i : qAsConst(m_hMapping))
        map.insert(i->m_Time = i->m_pInd->lastUsedTime(), i);

    int pos(map.size());

    q_ptr->beginResetModel();

    for (auto node : qAsConst(map)) {
        node->m_Index = --pos;
        m_lRows.push_back(node);
    }

    q_ptr->endResetModel();

    debugState();

    if (m_pSummaryModel)
        m_pSummaryModel->reloadCategories();

    emit q_ptr->headChanged();

    return m_lRows.size();
}

/******************************************************
 *               Timeline categories                  *
 *****************************************************/

SummaryModel::SummaryModel(PeersTimelineModelPrivate* parent) :
    HistoryTimeCategoryModel(parent), d_ptr(parent)
{
    reloadCategories();
}

SummaryModel::~SummaryModel() {
    d_ptr->m_pSummaryModel = nullptr;
}

QVariant SummaryModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid())
        return {};

    const auto cur = d_ptr->m_lSummaryHead[idx.row()];

    // Compute the total and visible distances between categories
    switch (role) {
        case (int)PeersTimelineModel::SummaryRoles::CATEGORY_ENTRIES: {
            if (!cur) break;

            if (idx.row() == NEVER)
                return ((int) d_ptr->m_lRows.size()) - cur->m_Index;

            int cnt = idx.row()+1;
            auto next = d_ptr->m_lSummaryHead[cnt];

            while((!next) && (++cnt) < NEVER && !(next = d_ptr->m_lSummaryHead[cnt]));

            return (!next) ? 0 : next->m_Index - cur->m_Index;
            } break;
        case (int)PeersTimelineModel::SummaryRoles::TOTAL_ENTRIES:
            return (int) d_ptr->m_lRows.size(); //FIXME use the proxy
        case (int)PeersTimelineModel::SummaryRoles::ACTIVE_CATEGORIES:
            return d_ptr->q_ptr->timelineSummaryModel()->rowCount();
        case (int)PeersTimelineModel::SummaryRoles::RECENT_DATE:
            if (!cur) break;
            return QDateTime::fromTime_t(cur->m_Time);
        case (int)PeersTimelineModel::SummaryRoles::DISTANT_DATE:
            return {}; //TODO
    }

    return HistoryTimeCategoryModel::data(idx, role);
}

QHash<int,QByteArray> SummaryModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
    if (!init_flag.test_and_set()) {
        roles[(int)PeersTimelineModel::SummaryRoles::CATEGORY_ENTRIES ] = "categoryEntries";
        roles[(int)PeersTimelineModel::SummaryRoles::ACTIVE_CATEGORIES] = "activeCategories";
        roles[(int)PeersTimelineModel::SummaryRoles::TOTAL_ENTRIES    ] = "totalEntries";
        roles[(int)PeersTimelineModel::SummaryRoles::RECENT_DATE      ] = "recentDate";
        roles[(int)PeersTimelineModel::SummaryRoles::DISTANT_DATE     ] = "distantDate";
    }

    return roles;
}

QSharedPointer<QAbstractItemModel> PeersTimelineModel::timelineSummaryModel() const
{
    if ((!d_ptr->m_pSummaryModel) && (d_ptr->m_pSummaryModel = new SummaryModel(d_ptr))) {
        auto m = new SummaryModelProxy(d_ptr);
        d_ptr->m_SummaryPtr = QSharedPointer<SummaryModelProxy>(m);

        // Transfer the ownership of m_pSummaryModel to the smart pointer
        m->setSourceModel(d_ptr->m_pSummaryModel);
        d_ptr->m_pSummaryModel->setParent(m);
    }

    return d_ptr->m_SummaryPtr;
}

/// Maintains a secondary index of summary categories
void SummaryModel::updateCategories(ITLNode* node, time_t t)
{
    const uint cat = (int) HistoryTimeCategoryModel::timeToHistoryConst(t);

    Q_ASSERT(d_ptr->m_lSummaryHead.size() > cat);

    if (d_ptr->m_lSummaryHead[cat] == nullptr || t > d_ptr->m_lSummaryHead[cat]->m_Time) {
        d_ptr->m_lSummaryHead[cat] = node;
        node->m_CatHead = cat;
        emit dataChanged(index(cat,0), index(cat, 0));
    }
    else
        node->m_CatHead = -1;
}

/// Perform a full reload instead of adding extra complexity to maintain the index.
void SummaryModel::reloadCategories()
{
    d_ptr->m_lSummaryHead.resize(NEVER+1);
    d_ptr->m_lSummaryHead.assign(NEVER+1, nullptr);

    for (auto n : d_ptr->m_lRows)
        updateCategories(n, n->m_Time);
}

/// Remove all categories without entries
bool SummaryModelProxy::filterAcceptsRow(int row, const QModelIndex & srcParent) const
{
    return (!srcParent.isValid()) && row <= NEVER && row >= 0 && d_ptr->m_lSummaryHead[row];
}

bool PeersTimelineModel::isEmpty() const
{
    return (!d_ptr->m_IsInit) || !rowCount();
}

Individual* PeersTimelineModel::mostRecentIndividual() const
{
    int idx = d_ptr->m_lRows.size();

    // Do not return the user own individual, that's not the intent
    while (idx--) {
        auto i = d_ptr->m_lRows[idx];

        if ((!i->m_pInd->hasProperty<&ContactMethod::isSelf>()) && i->m_pInd->lastUsedTime())
            return i->m_pInd;
    }

    return {};
}

QModelIndex PeersTimelineModel::individualIndex(Individual* i) const
{
    if (!i)
        return {};

    auto n = d_ptr->m_hMapping.value(i->masterObject());
    return n ? createIndex(n->m_Index, 0, n) : QModelIndex();
}

#undef NEVER
#include <peerstimelinemodel.moc>
