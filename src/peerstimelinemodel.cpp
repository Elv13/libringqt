/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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
#include <contactmethod.h>
#include <person.h>
#include <individual.h>
#include <phonedirectorymodel.h>
#include <private/modelutils.h>
#include <historytimecategorymodel.h>

#define NEVER static_cast<int>(HistoryTimeCategoryModel::HistoryConst::Never)
class SummaryModel;
class RecentCmModel;
class BookmarkedCmModel;

struct ITLNode final
{
    enum InsertStatus {
        NEW = -1
    };

    explicit ITLNode(Individual* ind, time_t t=0):m_pInd(ind),m_Time(t) {}
    bool        m_WhiteList {  false  };
    int         m_Index     { -1      };
    time_t      m_Time      {  0      };
    int         m_CatHead   { -1      };
    Individual* m_pInd      { nullptr };
};

class PeersTimelineModelPrivate final : public QObject
{
    Q_OBJECT
public:
    // Attributes
    std::vector<ITLNode*> m_lRows;
    QHash<Individual*, ITLNode*> m_hMapping;
    bool m_IsInit {false};
    SummaryModel* m_pSummaryModel;
    QSharedPointer<QAbstractItemModel> m_SummaryPtr {nullptr};
    std::vector<ITLNode*> m_lSummaryHead;
    QWeakPointer<RecentCmModel> m_MostRecentCMPtr;
    QWeakPointer<BookmarkedCmModel> m_BookmarkedCMPtr;

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
    void slotLatestUsageChanged(Individual* cm, time_t t);
    void slotIndividualAdded(Individual* ind);
    void slotDataChanged(Individual* i);
    void slotIndividualMerged(Individual*, Individual*);
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

/// Remove the empty timeline categories
class RecentCmModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit RecentCmModel(PeersTimelineModelPrivate* parent) :
        QSortFilterProxyModel(), d_ptr(parent) {}

    PeersTimelineModelPrivate* d_ptr;
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex & srcParent ) const override;
};

/// Only filter the bookmarked entries
class BookmarkedCmModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit BookmarkedCmModel(PeersTimelineModelPrivate* parent) :
        QSortFilterProxyModel(), d_ptr(parent) {}

    PeersTimelineModelPrivate* d_ptr;
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex & srcParent ) const override;
};

PeersTimelineModel& PeersTimelineModel::instance()
{
    static auto m_sInstance = new PeersTimelineModel();

    return *m_sInstance;
}

PeersTimelineModel::PeersTimelineModel() : QAbstractTableModel(QCoreApplication::instance()),
    d_ptr(new PeersTimelineModelPrivate())
{
    d_ptr->q_ptr = this;

    connect(this, &PeersTimelineModel::individualAdded,
        d_ptr, &PeersTimelineModelPrivate::slotIndividualAdded);
    connect(this, &PeersTimelineModel::individualChanged,
        d_ptr, &PeersTimelineModelPrivate::slotDataChanged);
    connect(this, &PeersTimelineModel::individualMerged,
        d_ptr, &PeersTimelineModelPrivate::slotIndividualMerged);
}

PeersTimelineModel::~PeersTimelineModel()
{
    foreach(auto entry, d_ptr->m_hMapping)
        delete entry;

    delete d_ptr;
}

QHash<int,QByteArray> PeersTimelineModel::roleNames() const
{
    return PhoneDirectoryModel::instance().roleNames();
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

/// Get the best position for the CM
std::vector<ITLNode*>::iterator PeersTimelineModelPrivate::getNextIndex(time_t t)
{
    if (m_lRows.empty())
        return m_lRows.begin();

    static ITLNode fake(nullptr);
    fake.m_Time = static_cast<long int>(t);

    return std::upper_bound(m_lRows.begin(), m_lRows.end(), &fake,
        [t](const ITLNode* a, const ITLNode* t2) -> bool {
            return a->m_Time < t2->m_Time;
    });
}

/// Extra code for the integration tests (slow for-loop)
void PeersTimelineModelPrivate::debugState()
{
#ifdef ENABLE_TEST_ASSERTS
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
    auto i = m_hMapping.value(ind);
    Q_ASSERT(i);

    if ((!m_IsInit) || (t > 0 && t <= i->m_Time))
        return;

    const auto it    = getNextIndex(t);
    const auto dtEnd = std::distance(it, m_lRows.end());
    i->m_Time        = t;

    if (i->m_Index == ITLNode::NEW || it == m_lRows.begin()) {
        i->m_Index = dtEnd;
        q_ptr->beginInsertRows({}, dtEnd, dtEnd);
        std::for_each(m_lRows.begin(), it, [](ITLNode* n) {n->m_Index++;});
        m_lRows.insert(it, i);
        q_ptr->endInsertRows();
    } else {
        const auto curIt = m_lRows.begin() + realIndex(i->m_Index);
        const int start(std::distance(curIt, m_lRows.end()));

        q_ptr->beginMoveRows({}, start, start, {}, dtEnd);
        std::for_each(curIt+1, it, [](ITLNode* n) {n->m_Index++;});
        std::rotate(curIt, curIt+1, it);
        i->m_Index = dtEnd;
        q_ptr->endMoveRows();
    }

    if (!dtEnd)
        emit q_ptr->headChanged();

    if (m_pSummaryModel)
        m_pSummaryModel->updateCategories(i, t);

    debugState();
}


void PeersTimelineModelPrivate::
slotIndividualAdded(Individual* ind)
{
    if (ind->isDuplicate())
        return;

    m_hMapping[ind] = new ITLNode(ind);
    slotLatestUsageChanged(ind, ind->lastUsedTime());
}

void PeersTimelineModelPrivate::
slotDataChanged(Individual* ind)
{
    const auto i = m_hMapping.value(ind->masterObject());
    if (!i) return;

    const auto idx = q_ptr->index(i->m_Index, 0);
    emit q_ptr->dataChanged(idx, idx);
}

/// Remove the duplicated CMs from the timeline
void PeersTimelineModelPrivate::slotIndividualMerged(Individual* old, Individual* into)
{
    Q_ASSERT(old->isDuplicate() && !into->isDuplicate() && into->masterObject() == into);
    auto entry = m_hMapping.take(old);

    if (!m_IsInit)
        delete entry;

    if ((!m_IsInit) || !entry)
        return;

    auto realIdx = m_lRows.begin()+realIndex(entry->m_Index);
    Q_ASSERT(entry == *realIdx);

    q_ptr->beginRemoveRows({}, entry->m_Index, entry->m_Index);
    std::for_each(m_lRows.begin(), realIdx, [](ITLNode* n) { n->m_Index--; });
    m_lRows.erase(realIdx);
    q_ptr->endRemoveRows();

    slotLatestUsageChanged(into, into->lastUsedTime());

    // Not as efficient as it can be, but simple and rare
    if (entry->m_CatHead != -1 && m_pSummaryModel)
        m_pSummaryModel->reloadCategories();

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

    for (auto i : qAsConst(m_hMapping)) {
        i->m_Time  = i->m_pInd->lastUsedTime();
        map.insert(i->m_Time, i);
    }

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

    auto cur = d_ptr->m_lSummaryHead[idx.row()];

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
            return QDateTime::fromTime_t(cur->m_Time);
        case (int)PeersTimelineModel::SummaryRoles::DISTANT_DATE:
            return {}; //TODO
    }

    return HistoryTimeCategoryModel::data(idx, role);
}

QHash<int,QByteArray> SummaryModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;
    if (!initRoles) {
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
    if (!d_ptr->m_pSummaryModel) {
        d_ptr->m_pSummaryModel = new SummaryModel(d_ptr);
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
bool SummaryModelProxy::filterAcceptsRow (int row, const QModelIndex & srcParent) const
{
    return (!srcParent.isValid()) && row <= NEVER && row >= 0 && d_ptr->m_lSummaryHead[row];
}

/// Remove all categories without entries
bool RecentCmModel::filterAcceptsRow(int row, const QModelIndex & srcParent) const
{
    if (srcParent.isValid() || row > (int) d_ptr->m_lRows.size())
        return false;

    auto n = d_ptr->m_lRows[d_ptr->realIndex(row)];

    // Also flush is "myself" entries that could have become true after they
    // were first inserted.
    return true; (!n->m_pInd->isSelf()) && (
        n->m_pInd->person() || n->m_WhiteList || n->m_pInd->lastUsedTime()
    );
}

/// Remove all categories without entries
bool BookmarkedCmModel::filterAcceptsRow(int row, const QModelIndex & srcParent) const
{
    if (srcParent.isValid() || row > (int) d_ptr->m_lRows.size())
        return false;

    auto n = d_ptr->m_lRows[d_ptr->realIndex(row)];

    // Also flush is "myself" entries that could have become true after they
    // were first inserted.
    return n->m_pInd->hasBookmarks();
}


/// Only keep the most recently contacted contact method
QSharedPointer<QAbstractItemModel> PeersTimelineModel::deduplicatedTimelineModel() const
{
    if (!d_ptr->m_MostRecentCMPtr) {
        QSharedPointer<RecentCmModel> m = QSharedPointer<RecentCmModel>(new RecentCmModel(d_ptr));
        m->setSourceModel(const_cast<PeersTimelineModel*>(this));
        d_ptr->m_MostRecentCMPtr = m;

        // Extra checks will be performed and the result may change
        emit const_cast<PeersTimelineModel*>(this)->headChanged();

        return m;
    }

    return d_ptr->m_MostRecentCMPtr;
}

QSharedPointer<QAbstractItemModel> PeersTimelineModel::bookmarkedTimelineModel() const
{
    if (!d_ptr->m_BookmarkedCMPtr) {
        QSharedPointer<BookmarkedCmModel> m = QSharedPointer<BookmarkedCmModel>(new BookmarkedCmModel(d_ptr));
        m->setSourceModel(const_cast<PeersTimelineModel*>(this));
        d_ptr->m_BookmarkedCMPtr = m;
        return m;
    }

    return d_ptr->m_BookmarkedCMPtr;
}

bool PeersTimelineModel::isEmpty() const
{
    if (!d_ptr->m_IsInit)
        return false;

    if (auto m = d_ptr->m_MostRecentCMPtr.toStrongRef())
        return !m->rowCount();

    return !rowCount();
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

QModelIndex PeersTimelineModel::contactMethodIndex(ContactMethod* cm) const
{
    if (!cm)
        return {};

    auto n = d_ptr->m_hMapping.value(cm->individual()->masterObject());

    if (!n)
        return {};

    return createIndex(n->m_Index, 0, n);
}

QModelIndex PeersTimelineModel::individualIndex(Individual* i) const
{
    if (!i || !i->lastUsedContactMethod())
        return {};

    auto m = qSharedPointerCast<RecentCmModel>(deduplicatedTimelineModel());

    return m->mapFromSource(contactMethodIndex(i->lastUsedContactMethod()));
}

void PeersTimelineModel::whiteList(Individual* ind)
{
    if (ind->lastUsedTime())
        return;

    auto i = d_ptr->m_hMapping.value(ind->masterObject());
    Q_ASSERT(i);

    i->m_WhiteList = true;
    const auto idx = createIndex(i->m_Index, 0, i);
    emit dataChanged(idx, idx);
}

class PeersTimelineSelectionModelPrivate
{
public:
    QSharedPointer<QAbstractItemModel> m_pModel;
};

PeersTimelineSelectionModel::PeersTimelineSelectionModel() :
    QItemSelectionModel(nullptr),
    d_ptr(new PeersTimelineSelectionModelPrivate)
{
    d_ptr->m_pModel = PeersTimelineModel::instance().deduplicatedTimelineModel();
    setModel(d_ptr->m_pModel.data());
    setParent(d_ptr->m_pModel.data());
}

PeersTimelineSelectionModel::~PeersTimelineSelectionModel()
{
    delete d_ptr;
}

ContactMethod* PeersTimelineSelectionModel::contactMethod() const
{
    const auto cur = currentIndex();

    if (!cur.isValid())
        return nullptr;

    return qvariant_cast<ContactMethod*>(cur.data((int)Ring::Role::Object));
}

void PeersTimelineSelectionModel::setContactMethod(ContactMethod* cm)
{
    auto idx  = PeersTimelineModel::instance().contactMethodIndex(cm);
    auto pidx = qobject_cast<RecentCmModel*>(d_ptr->m_pModel.data())->mapFromSource(idx);

    setCurrentIndex(pidx, QItemSelectionModel::ClearAndSelect);
}

#undef NEVER
#include <peerstimelinemodel.moc>
