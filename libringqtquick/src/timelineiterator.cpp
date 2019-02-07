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
#include "timelineiterator.h"

// Qt
#include <QtCore/QTimer>

// Ring
#include <individual.h>
#include <individualtimelinemodel.h>

class TimelineIteratorPrivate final : public QObject
{
    Q_OBJECT
public:
    // Attributes
    Individual* m_pCurrentIndividual {nullptr};
    QSharedPointer<QAbstractItemModel> m_pTimeline;
    QPersistentModelIndex m_FirstVisibleIndex;
    QPersistentModelIndex m_LastVisibleIndex;
    QPersistentModelIndex m_CurrentIndex;
    QPersistentModelIndex m_LastKnownIndex;
    QString m_SearchString;

    // Helpers
    void findNewest();

    TimelineIterator* q_ptr;

public Q_SLOTS:
    void slotUnreadCountChanged();
    void slotInserted(const QModelIndex& parent, int first, int last);
};

TimelineIterator::TimelineIterator(QObject* parent) : QObject(parent),
    d_ptr(new TimelineIteratorPrivate())
{
    d_ptr->q_ptr = this;
}

TimelineIterator::~TimelineIterator()
{
    delete d_ptr;
}

Individual* TimelineIterator::currentIndividual() const
{
    return d_ptr->m_pCurrentIndividual;
}

void TimelineIterator::setCurrentIndividual(Individual* i)
{
    if (d_ptr->m_pCurrentIndividual) {
        disconnect(d_ptr->m_pTimeline.data(), &QAbstractItemModel::rowsInserted,
            d_ptr, &TimelineIteratorPrivate::slotInserted);

        disconnect(d_ptr->m_pCurrentIndividual, &Individual::unreadCountChanged,
            d_ptr, &TimelineIteratorPrivate::slotUnreadCountChanged);

        // Release the shared pointer
        d_ptr->m_pTimeline = nullptr;
    }

    d_ptr->m_pCurrentIndividual = i;

    if (i) {
        d_ptr->m_pTimeline = i->timelineModel();
        d_ptr->findNewest();

        connect(d_ptr->m_pTimeline.data(), &QAbstractItemModel::rowsInserted,
            d_ptr, &TimelineIteratorPrivate::slotInserted);

        connect(d_ptr->m_pCurrentIndividual, &Individual::unreadCountChanged,
            d_ptr, &TimelineIteratorPrivate::slotUnreadCountChanged);
    }

    emit changed();
}

QModelIndex TimelineIterator::firstVisibleIndex() const
{
    return d_ptr->m_FirstVisibleIndex;
}

void TimelineIterator::setFirstVisibleIndex(const QModelIndex& i)
{
    d_ptr->m_FirstVisibleIndex = i;
    emit changed();
}

QModelIndex TimelineIterator::lastVisibleIndex() const
{
    return d_ptr->m_LastVisibleIndex;
}

void TimelineIterator::setLastVisibleIndex(const QModelIndex& i)
{
    d_ptr->m_LastVisibleIndex = i;
    emit changed();
}

QModelIndex TimelineIterator::currentIndex() const
{
    return d_ptr->m_CurrentIndex;
}

void TimelineIterator::setCurrentIndex(const QModelIndex& i)
{
    d_ptr->m_CurrentIndex = i;
    emit changed();
}

QString TimelineIterator::searchString() const
{
    return d_ptr->m_SearchString;
}

void TimelineIterator::setSearchString(const QString& s)
{
    d_ptr->m_SearchString = s;
    emit changed();
}

int TimelineIterator::unreadCount() const
{
    return d_ptr->m_pCurrentIndividual ?
        d_ptr->m_pCurrentIndividual->unreadTextMessageCount() : 0;
}

int TimelineIterator::bookmarkCount() const
{
//     return d_ptr->m_pCurrentIndividual ? d_ptr->m_pCurrentIndividual-> : 0;
    return 0;
}

int TimelineIterator::searchResultCount() const
{
//     return d_ptr->m_pCurrentIndividual ? d_ptr->m_pCurrentIndividual-> : 0;
    return 0;
}

void TimelineIterator::proposePreviousUnread() const
{

}

void TimelineIterator::proposeNextUnread() const
{

}

void TimelineIterator::proposePreviousBookmark() const
{

}

void TimelineIterator::proposeNextBookmark() const
{

}

void TimelineIterator::proposePreviousResult() const
{

}

void TimelineIterator::proposeNextResult() const
{

}

void TimelineIterator::proposeNewest() const
{
    emit proposeIndex(d_ptr->m_LastKnownIndex);
}

void TimelineIterator::proposeOldest() const
{

}

bool TimelineIterator::hasMore() const
{
    return d_ptr->m_LastVisibleIndex.isValid() &&
        d_ptr->m_LastVisibleIndex != d_ptr->m_LastKnownIndex;
}

void TimelineIteratorPrivate::slotUnreadCountChanged()
{
    emit q_ptr->changed();
}

void TimelineIteratorPrivate::slotInserted(const QModelIndex& parent, int first, int last)
{
    QPersistentModelIndex p = parent;

    // Make sure not to act on an item before the view own slotRowsInserted is
    // executed.
    QTimer::singleShot(0, [this, last, p]() {
        emit q_ptr->changed();

        // Update this after to so the client side logic can check if it was at the
        // end
        m_LastKnownIndex = m_pTimeline->index(last, 0, p);
        emit q_ptr->contentAdded();
    });
}

void TimelineIteratorPrivate::findNewest()
{
    QModelIndex parent = m_pTimeline->index(m_pTimeline->rowCount()-1, 0);

    while (int rc = m_pTimeline->rowCount(parent))
        parent = m_pTimeline->index(rc - 1, 0, parent);

    m_LastKnownIndex = parent;
}

QModelIndex TimelineIterator::newestIndex() const
{
    return d_ptr->m_LastKnownIndex;
}

#include <timelineiterator.moc>
