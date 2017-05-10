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

// Ring
#include <contactmethod.h>
#include <phonedirectorymodel.h>
#include <private/modelutils.h>

struct CMTimelineNode final
{
    explicit CMTimelineNode(ContactMethod* cm, time_t t=0):m_pCM(cm),m_Time(t) {}
    int            m_Index { -1      };
    time_t         m_Time  {  0      };
    ContactMethod* m_pCM   { nullptr };
};

class PeersTimelineModelPrivate : public QObject
{
    Q_OBJECT
public:
    // Attributes
    std::vector<CMTimelineNode*> m_lRows;
    QHash<ContactMethod*, CMTimelineNode*> m_hMapping;
    bool m_IsInit {false};

    // Helpers
    int init();
    void debugState();

    // Invert the list access to maximize the entropy of the end of the list.
    // The has a better performance profile when you consider the recently
    // used CMs have the higher chances of being used again.
    inline int realIndex(int effective) {return m_lRows.size() - effective - 1;}

    // Get the next index *based on the current list*
    std::vector<CMTimelineNode*>::iterator getNextIndex(time_t t);

    PeersTimelineModel* q_ptr;

public Q_SLOTS:
    void slotLatestUsageChanged(ContactMethod* cm, time_t t);
    void slotContactMethodAdded(const QModelIndex& i, int first, int last);
    void slotContactMethodMerged(ContactMethod*, ContactMethod*);
    void slotDataChanged(const QModelIndex& tl, const QModelIndex& br);
};

PeersTimelineModel& PeersTimelineModel::instance()
{
    static auto m_sInstance = new PeersTimelineModel();

    return *m_sInstance;
}

PeersTimelineModel::PeersTimelineModel() : QAbstractListModel(QCoreApplication::instance()),
    d_ptr(new PeersTimelineModelPrivate())
{
    d_ptr->q_ptr = this;
    connect(&PhoneDirectoryModel::instance(), &PhoneDirectoryModel::lastUsedChanged,
        d_ptr, &PeersTimelineModelPrivate::slotLatestUsageChanged);
    connect(&PhoneDirectoryModel::instance(), &PhoneDirectoryModel::rowsInserted,
        d_ptr, &PeersTimelineModelPrivate::slotContactMethodAdded);
    connect(&PhoneDirectoryModel::instance(), &PhoneDirectoryModel::contactMethodMerged,
        d_ptr, &PeersTimelineModelPrivate::slotContactMethodMerged);
    connect(&PhoneDirectoryModel::instance(), &PhoneDirectoryModel::dataChanged,
        d_ptr, &PeersTimelineModelPrivate::slotDataChanged);
}

PeersTimelineModel::~PeersTimelineModel()
{
    delete d_ptr;
}

QHash<int,QByteArray> PeersTimelineModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        QHash<int, QByteArray>::const_iterator i;
        for (i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); ++i)
            roles[i.key()] = i.value();
    }

    return roles;
}

bool PeersTimelineModel::setData( const QModelIndex& idx, const QVariant &value, int role)
{
    if (!idx.isValid()) return false;
    return static_cast<CMTimelineNode*>(idx.internalPointer())->m_pCM->setRoleData(value, role);
}

QVariant PeersTimelineModel::data( const QModelIndex& idx, int role) const
{
    if (!idx.isValid())
        return {};
    return static_cast<CMTimelineNode*>(idx.internalPointer())->m_pCM->roleData(role);
}

int PeersTimelineModel::rowCount( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d_ptr->init();
}

QModelIndex PeersTimelineModel::index( int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column || parent.isValid() || row >= (int) d_ptr->m_lRows.size())
        return {};

    return createIndex(row, 0, d_ptr->m_lRows[d_ptr->realIndex(row)]);
}

/// Get the best position for the CM
std::vector<CMTimelineNode*>::iterator PeersTimelineModelPrivate::getNextIndex(time_t t)
{
    if (m_lRows.empty())
        return m_lRows.begin();

    static CMTimelineNode fake(nullptr);
    fake.m_Time = static_cast<long int>(t);

    return std::upper_bound(m_lRows.begin(), m_lRows.end(), &fake,
        [t](const CMTimelineNode* a, const CMTimelineNode* t2) -> bool {
            return a->m_Time < t2->m_Time;
    });
}

/// Extra code for the integration tests (slow for-loop)
void PeersTimelineModelPrivate::debugState()
{
//#if ENABLE_TEST_ASSERTS //TODO uncomment once it's stable enough
    bool correct(true), correct2(true);
    for (uint i = 0; i < m_lRows.size()-1; i++) {
        correct2 &= m_lRows[i]->m_Time  <= m_lRows[i+1]->m_Time;
        correct  &= m_lRows[i]->m_Index == m_lRows[i+1]->m_Index+1;
    }
    Q_ASSERT(correct );
    Q_ASSERT(correct2);
//#endif
}

/**
 * Insert or move contact methods in the model
 */
void PeersTimelineModelPrivate::slotLatestUsageChanged(ContactMethod* cm, time_t t)
{
    // Don't load anything until the model is in use
    if ((!m_IsInit) || cm->isDuplicate() || cm->isSelf())
        return;

    auto i = m_hMapping.value(cm);
    if (!i) {
        i = new CMTimelineNode(cm);
        m_hMapping[cm] = i;
    }

    if (t && t == i->m_Time)
        return;

    const auto it    = getNextIndex(t);
    const auto dtEnd = std::distance(it, m_lRows.end());
    i->m_Time        = t;

    if (i->m_Index == -1 || it == m_lRows.begin()) {
        i->m_Index = dtEnd;

        q_ptr->beginInsertRows({}, dtEnd, dtEnd);
        std::for_each(m_lRows.begin(), it, [](CMTimelineNode* n) {n->m_Index++;});
        m_lRows.insert(it, i);
        q_ptr->endInsertRows();
    } else {
        const auto curIt = m_lRows.begin() + realIndex(i->m_Index);
        const int start(std::distance(curIt, m_lRows.end()));

        q_ptr->beginMoveRows({}, start, start, {}, dtEnd);
        std::for_each(curIt+1, it, [](CMTimelineNode* n) {n->m_Index++;});
        std::rotate(curIt, curIt+1, it);
        i->m_Index = dtEnd;
        q_ptr->endMoveRows();
    }

    debugState();
}

void PeersTimelineModelPrivate::
slotContactMethodAdded(const QModelIndex& i, int first, int last)
{
    if (i.isValid())
        return;

    const auto m = &PhoneDirectoryModel::instance();
    const auto r = static_cast<int>(PhoneDirectoryModel::Role::Object);

    ModelUtils::for_each_role<ContactMethod*>(m, r, [this](ContactMethod* cm) {
        slotLatestUsageChanged(cm, cm->lastUsed());
    }, first, last);
}

void PeersTimelineModelPrivate::
slotDataChanged(const QModelIndex& tl, const QModelIndex& br)
{
    const auto m = &PhoneDirectoryModel::instance();
    const auto r = static_cast<int>(PhoneDirectoryModel::Role::Object);
    ModelUtils::for_each_role<ContactMethod*>(m, r, [this](ContactMethod* cm) {
        if (auto i = m_hMapping.value(cm)) {
            const auto idx = q_ptr->index(i->m_Index, 0);
            emit q_ptr->dataChanged(idx, idx);
        }
    }, tl.row(), br.row());
}

/// Remove the duplicated CMs from the timeline
void PeersTimelineModelPrivate::slotContactMethodMerged(ContactMethod* cm, ContactMethod*)
{
    Q_ASSERT(cm->isDuplicate());
    if ((!m_IsInit) || !m_hMapping.contains(cm))
        return;

    auto entry   = m_hMapping.value(cm);
    auto realIdx = m_lRows.begin()+realIndex(entry->m_Index);
    Q_ASSERT(entry == *realIdx);

    q_ptr->beginRemoveRows({}, entry->m_Index, entry->m_Index);
    std::for_each(m_lRows.begin(), realIdx, [](CMTimelineNode* n) { n->m_Index--; });
    m_lRows.erase(realIdx);
    q_ptr->endRemoveRows();

    m_hMapping.remove(cm);
    delete entry;
    debugState();
}

/// Get the current contact methods and sort them
int PeersTimelineModelPrivate::init()
{
    if (m_IsInit)
        return m_lRows.size();

    m_IsInit = true;

    // Create a QList for the easy insertion, then convert it to a model
    QMultiMap<time_t, ContactMethod*> map;
    const auto m = &PhoneDirectoryModel::instance();
    const auto r = static_cast<int>(PhoneDirectoryModel::Role::Object);

    ModelUtils::for_each_role<ContactMethod*>(m, r, [&map](ContactMethod* cm) {
        if ((!cm->isDuplicate()) && !cm->isSelf())
            map.insert(cm->lastUsed(), cm);
    });

    int pos(map.size());

    for (auto cm : qAsConst(map)) {
        auto i         = new CMTimelineNode(cm, cm->lastUsed());
        i->m_Index     = --pos;
        m_hMapping[cm] = i;
        m_lRows.push_back(i);
    }

    return m_lRows.size();
}

#include <peerstimelinemodel.moc>
