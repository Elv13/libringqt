/***************************************************************************
 *   Copyright (C) 2019 by Blue Systems                                    *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "timelinefilter.h"

// Qt
#include <QtCore/QTimer>

// LibRingQt
#include <individual.h>
#include <individualtimelinemodel.h>
#include <libcard/flagutils.h>

class TimelineFilterPrivate final : public QTimer
{
    Q_OBJECT
public:

    enum class Flags : uchar {
        NONE     = 0x0 << 0,
        MESSAGES = 0x1 << 0,
        CALLS    = 0x1 << 1,
        GROUPS   = 0x1 << 2,
        COUNT__  = 0x1 << 3,
    };

    FlagPack<Flags> m_Flags {FlagPack<Flags>() |
        Flags::MESSAGES |
        Flags::CALLS    |
        Flags::GROUPS
    };

    Individual* m_pIndividual {nullptr};
    QSharedPointer<QAbstractItemModel> m_pTimelineModel {nullptr};

    TimelineFilter* q_ptr;

public Q_SLOTS:
    void slotTimeout();
};

using Flags = TimelineFilterPrivate::Flags;

TimelineFilter::TimelineFilter(QObject* parent) : QSortFilterProxyModel(parent),
    d_ptr(new TimelineFilterPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->setSingleShot(true);
    connect(d_ptr, &QTimer::timeout, d_ptr, &TimelineFilterPrivate::slotTimeout);
}

TimelineFilter::~TimelineFilter()
{
    delete d_ptr;
}

Individual* TimelineFilter::individual() const
{
    return d_ptr->m_pIndividual;
}

void TimelineFilter::setIndividual(Individual* i)
{
    d_ptr->m_pIndividual    = i;
    d_ptr->m_pTimelineModel = i ? i->timelineModel() : nullptr;

    emit changed();
    d_ptr->start();
}

bool TimelineFilter::areMessagesShown() const
{
    return d_ptr->m_Flags & Flags::MESSAGES;
}

void TimelineFilter::setShowMessages(bool v)
{
    d_ptr->m_Flags.set(Flags::MESSAGES, v);
    emit changed();
}

bool TimelineFilter::areCallsShown() const
{
    return d_ptr->m_Flags & Flags::CALLS;
}

void TimelineFilter::setShowCalls(bool v)
{
    d_ptr->m_Flags.set(Flags::CALLS, v);
    Q_ASSERT(areCallsShown() == v);

    emit changed();
}

bool TimelineFilter::areEmptyGroupsShown() const
{
    return d_ptr->m_Flags & Flags::GROUPS;
}

void TimelineFilter::setShowEmptyGroups(bool v)
{
    d_ptr->m_Flags.set(Flags::GROUPS, v);
    Q_ASSERT(areEmptyGroupsShown() == v);
    emit changed();
}

int TimelineFilter::initDelay() const
{
    return d_ptr->interval();
}

void TimelineFilter::setInitDelay(int v)
{
    if (d_ptr->isActive()) {
        d_ptr->stop();
        d_ptr->setInterval(v);
        d_ptr->start();
    }
    else
        d_ptr->setInterval(v);
}

void TimelineFilterPrivate::slotTimeout()
{
    q_ptr->setSourceModel(
        m_pTimelineModel? m_pTimelineModel.data() : nullptr
    );
}

bool TimelineFilter::filterAcceptsRow(int source_row, const QModelIndex &sourceParent) const
{
    const QModelIndex idx = sourceModel()->index(source_row, 0, sourceParent);

    if (!idx.isValid())
        return false;

    const auto et = (IndividualTimelineModel::NodeType) idx.data(
        (int)IndividualTimelineModel::Role::NodeType
    ).toInt();

    switch(et) {
        case IndividualTimelineModel::NodeType::CALL:
            return areCallsShown();
        case IndividualTimelineModel::NodeType::SNAPSHOT:
        case IndividualTimelineModel::NodeType::TEXT_MESSAGE:
            return areMessagesShown();
        case IndividualTimelineModel::NodeType::CALL_GROUP:
            return areCallsShown() || areEmptyGroupsShown();
        case IndividualTimelineModel::NodeType::SNAPSHOT_GROUP:
        case IndividualTimelineModel::NodeType::SECTION_DELIMITER:
            return areMessagesShown() || areEmptyGroupsShown();
        case IndividualTimelineModel::NodeType::TIME_CATEGORY:
        case IndividualTimelineModel::NodeType::AUDIO_RECORDING:
        case IndividualTimelineModel::NodeType::EMAIL:
        case IndividualTimelineModel::NodeType::RECORDINGS:
            return true;
    }

    return true;
}

QModelIndex TimelineFilter::mapFromSource(const QModelIndex& idx) const
{
    return QSortFilterProxyModel::mapFromSource(idx);
}

QModelIndex TimelineFilter::mapToSource(const QModelIndex& idx) const
{
    return QSortFilterProxyModel::mapToSource(idx);
}

#include <timelinefilter.moc>
