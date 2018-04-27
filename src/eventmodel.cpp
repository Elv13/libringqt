/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
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
#include "eventmodel.h"

//Ring
#include "account.h"
#include "individual.h"
#include "private/contactmethod_p.h"
#include "libcard/event.h"
#include "libcard/private/event_p.h"
#include "libcard/private/eventmodel_p.h"

#include <stdio.h>

/**
 * Keep both a global vector of events and a linked list.
 *
 * At this point everything is semi-unordered. The linked list is used to
 * generate aggregates and can be sorted properly if the need arise.
 */
struct EventModelNode {
    Event*          m_pEvent                   {nullptr};
    EventModelNode* m_pNextByContactMethod     {nullptr};
    EventModelNode* m_pPreviousByContactMethod {nullptr};
    EventModelNode* m_pNextByIndividual        {nullptr};
    EventModelNode* m_pPreviousByIndividual    {nullptr};

    /**
     * Once events are sorted, insert them sorted.
     *
     * The daisy chaining will be used to later build timelines.
     */
    enum SortMode : uchar {
        NONE = 0x0,
        CM   = 0x1 << 0,
        IND  = 0x1 << 1,
    };

    FlagPack<SortMode> m_SortMode {SortMode::NONE};
};

struct ContactMethodEvents
{
    Event* m_pNewest       {nullptr}; /*!< Highest stopTimeStamp () */
    Event* m_pOldest       {nullptr}; /*!< Lowest  startTimeStamp() */
    Event* m_pUnsortedTail {nullptr}; /*!< Lastest addition         */
    Event* m_pUnsortedHead {nullptr}; /*!< The first addition       */

    //FIXME this sucks and forces to track all events all the time.
    // Given events are synchronized across devices, they are expected in semi
    // random order and keeping a sorted vector for every CM becomes very
    // CPU/memcpy intensive.
    // I have no time left until release to finish the lazy sorting system.
    QVector< QSharedPointer<Event> > m_lEvents;
};

inline static int cmp(EventModelNode *a, EventModelNode *b) {
    return a->m_pEvent->startTimeStamp() - b->m_pEvent->startTimeStamp();
}

/**
 * FIXME
 *
 * The idea behind this was to keep the events in a linked list as long as
 * possible because sorting them isn't required 99% of the time. But it's
 * incomplete and would take too long to fix.
 */

/*
 * This file is copyright 2001 Simon Tatham.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL SIMON TATHAM BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
template<
    EventModelNode* EventModelNode::*next     = &EventModelNode::m_pNextByContactMethod    ,
    EventModelNode* EventModelNode::*previous = &EventModelNode::m_pPreviousByContactMethod,
    EventModelNode::SortMode         mode     = EventModelNode::SortMode::CM
>
EventModelNode* listsort(EventModelNode *list, bool is_circular, bool is_double) {
    EventModelNode *p, *q, *e, *tail, *oldhead;
    int insize, nmerges, psize, qsize, i;

    /*
    * Silly special case: if `list' was passed in as NULL, return
    * NULL immediately.
    */
    if (!list)
        return NULL;

    insize = 1;

    while (1) {
        p = list;
        oldhead = list; /* only used for circular linkage */
        list = NULL;
        tail = NULL;

        nmerges = 0;  /* count number of merges we do in this pass */

        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;

            for (i = 0; i < insize; i++) {
                psize++;

                if (is_circular)
                    q = (q->*(next) == oldhead ? NULL : q->*(next));
                else
                    q = q->*(next);
                        if (!q) break;
            }

            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;

            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {

                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {
                    /* p is empty; e must come from q. */
                    e = q; q = q->*(next); qsize--;
                    if (is_circular && q == oldhead)
                        q = NULL;
                } else if (qsize == 0 || !q) {
                    /* q is empty; e must come from p. */
                    e = p; p = p->*(next); psize--;
                    if (is_circular && p == oldhead)
                        p = NULL;
                } else if (cmp(p,q) <= 0) {
                    /* First element of p is lower (or same);
                    * e must come from p. */
                    e = p; p = p->*(next); psize--;
                    if (is_circular && p == oldhead)
                        p = NULL;
                } else {
                    /* First element of q is lower; e must come from q. */
                    e = q; q = q->*(next); qsize--;
                    if (is_circular && q == oldhead)
                        q = NULL;
                }

                        /* add the next element to the merged list */
                if (tail) {
                    tail->*(next) = e;
                } else {
                    list = e;
                }
                if (is_double) {
                    /* Maintain reverse pointers in a doubly linked list. */
                    e->*(previous) = tail;
                }
                tail = e;
            }

            p->m_SortMode |= mode;
            q->m_SortMode |= mode;

            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }

        if (is_circular) {
            tail->*(next) = list;
            if (is_double)
                list->*(previous) = tail;
        } else {
            tail->*(next) = NULL;
        }

        /* If we have done only one merge, we're finished. */
        if (nmerges <= 1) /* allow for nmerges==0, the empty list case */
            return list;

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}

EventModel::EventModel(QObject* parent)
  : QAbstractListModel(parent)
  , CollectionManagerInterface<Event>(this)
  , d_ptr(new EventModelPrivate())
{
    d_ptr->q_ptr = this;
}

EventModel& EventModel::instance()
{
    static auto instance = new EventModel(QCoreApplication::instance());
    return *instance;
}

EventModel::~EventModel()
{
    // The EventModel owns the events, but given they are shared, there is no
    // way to know if it's really the last reference, so delete them carefully.
    while (d_ptr->m_lEvent.size()) {
        EventModelNode* n = d_ptr->m_lEvent.takeLast();

        n->m_pEvent->setParent(nullptr);

        // If m_pStrongRef is the last reference, the destructor is called.
        // Otherwise it will assert if any class attempts to get new references
        n->m_pEvent->d_ptr->m_pStrongRef = nullptr;
    }

    delete d_ptr;
}

QHash<int,QByteArray> EventModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static std::atomic_flag initRoles {ATOMIC_FLAG_INIT};

    if (!initRoles.test_and_set()) {
        QHash<int, QByteArray>::const_iterator i;
        for (i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); ++i)
            roles[i.key()] = i.value();

        roles[Event::Roles::REVISION_COUNT  ] = "revisionCount";
        roles[Event::Roles::UID             ] = "uid";
        roles[Event::Roles::BEGIN_TIMESTAMP ] = "beginTimestamp";
        roles[Event::Roles::END_TIMESTAMP   ] = "endTimestamp";
        roles[Event::Roles::UPDATE_TIMESTAMP] = "updateTimestamp";
        roles[Event::Roles::EVENT_CATEGORY  ] = "eventCategory";
        roles[Event::Roles::DIRECTION       ] = "direction";
        roles[Event::Roles::HAS_AV_RECORDING] = "hasAVRecording";
        roles[Event::Roles::STATUS          ] = "status";
    }

    return roles;
}

QVariant EventModel::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    const EventModelNode* info = d_ptr->m_lEvent[index.row()];

    return info->m_pEvent->roleData(role);
}

int EventModel::rowCount( const QModelIndex& parent ) const
{
    if (!parent.isValid())
        return d_ptr->m_lEvent.size();
    return 0;
}

QItemSelectionModel* EventModel::defaultSelectionModel() const
{
    return nullptr;
}

bool EventModel::addItemCallback(const Event* item)
{
    if (Q_UNLIKELY(d_ptr->m_hUids.contains(item->uid()))) {
        qWarning() << "An event with the same name was created twice, this is a bug" << item->uid();
    }

    // If this happens, the index will be corrupted
    if (Q_UNLIKELY(item->d_ptr->m_pTracker)) {
        qWarning() << "addItemCallback called twice for the same event";
        Q_ASSERT(false);
    }

    d_ptr->m_hUids[item->uid()] = const_cast<Event*>(item);
    connect(item, &QObject::destroyed, d_ptr, &EventModelPrivate::slotFixCache);

    beginInsertRows({} ,d_ptr->m_lEvent.size(),d_ptr->m_lEvent.size());

    auto n = new EventModelNode {
        const_cast<Event*>(item),
        nullptr,
        nullptr
    };

    d_ptr->m_lEvent << n;

    item->d_ptr->m_pTracker = n;

    endInsertRows();

    // Update the ContactMethod and handle sorting (if any)
    for (auto pair : qAsConst(item->d_ptr->m_lAttendees)) {
        auto cm = pair.first; //TODO C++17

        if (!cm->d_ptr->m_pEvents)
            cm->d_ptr->m_pEvents = new ContactMethodEvents;

        if ((!cm->d_ptr->m_pEvents->m_pOldest) || cm->d_ptr->m_pEvents->m_pOldest->startTimeStamp() > item->startTimeStamp()) {
            cm->d_ptr->m_pEvents->m_pOldest = const_cast<Event*>(item);
        }

        // Update the unsorted Event_by_CM linked list
        if (cm->d_ptr->m_pEvents->m_pUnsortedTail) {
            Q_ASSERT(cm->d_ptr->m_pEvents->m_pUnsortedTail->d_ptr->m_pTracker);

            auto tail = cm->d_ptr->m_pEvents->m_pUnsortedTail->d_ptr->m_pTracker;

            Q_ASSERT(!tail->m_pNextByContactMethod);

            tail->m_pNextByContactMethod  = n;
            n->m_pPreviousByContactMethod = tail;
        }

        cm->d_ptr->m_pEvents->m_pUnsortedTail = const_cast<Event*>(item);

        if ((!cm->d_ptr->m_pEvents->m_pNewest) || cm->d_ptr->m_pEvents->m_pNewest->stopTimeStamp() <= item->stopTimeStamp()) {
            cm->d_ptr->m_pEvents->m_pNewest = const_cast<Event*>(item);
        }

        //FIXME someday, do better than that
        cm->d_ptr->m_pEvents->m_lEvents << item->d_ptr->m_pStrongRef;
        cm->d_ptr->setLastUsed(item->stopTimeStamp());
        cm->d_ptr->addTimeRange(item->startTimeStamp(), item->stopTimeStamp());

        auto ref = const_cast<Event*>(item)->ref();

        emit cm->eventAdded(ref);
        emit cm->individual()->eventAdded(ref);
    }

    return true;
}

bool EventModel::removeItemCallback(const Event* item)
{
    for (auto pair : qAsConst(item->d_ptr->m_lAttendees)) {
        auto cm = pair.first; //TODO C++17
        auto ref = const_cast<Event*>(item)->ref();
        emit cm->eventDetached(ref);
        emit cm->individual()->eventDetached(ref);
    }

    return true;
}

QSharedPointer<Event> EventModel::getById(const QByteArray& eventId, bool placeholder) const
{
    if (eventId.isEmpty())
        return nullptr;

    if (auto e = d_ptr->m_hUids.value(eventId))
        return e->d_ptr->m_pStrongRef;

    if (placeholder && !eventId.isEmpty()) {
        auto e = new Event({}, Event::SyncState::PLACEHOLDER);
        connect(e, &QObject::destroyed, d_ptr, &EventModelPrivate::slotFixCache);
        e->d_ptr->m_UID = eventId;
        d_ptr->m_hUids[eventId] = e;
        return e->d_ptr->m_pStrongRef;
    }

    return nullptr;
}

/**
 * Use a merge sort on the linked list.
 *
 * With some luck, it will already be sorted. The "normal" use case is to have
 * a single account with a single calendar. Even if there is multiple accounts
 * and multiple devices, the chance they will talk to the same contact method
 * is virtually impossible.
 *
 * The reasoning for this algorithm is two fold:
 *
 *  * It prevents keeping a vector of all events for all ContactMethod all the
 *    time. This has a much lower memory usage.
 *  * If the sorting is done late enough, all event collections will be loaded
 *    and the odds of adding event in the middle of the vector ( O(N) ) and much
 *    lower than at the end ( O(1) ).
 *
 *
 */
void EventModelPrivate::sort(ContactMethod* cm)
{
    if (!cm)
        return;

    if (!cm->d_ptr->m_pEvents)
        cm->d_ptr->m_pEvents = new ContactMethodEvents;

    if (!cm->d_ptr->m_pEvents->m_pUnsortedHead)
        return;

    Q_ASSERT(cm->d_ptr->m_pEvents->m_pUnsortedTail);
    Q_ASSERT(cm->d_ptr->m_pEvents->m_pUnsortedHead->d_ptr->m_pTracker);

    listsort(cm->d_ptr->m_pEvents->m_pUnsortedHead->d_ptr->m_pTracker, false, false);

//     SortMode
}

/// Same as above, for on the invididual granularity
void EventModelPrivate::sort(Individual* ind)
{
//     listsort <
//         &EventModelNode::m_pNextByIndividual,
//         &EventModelNode::m_pPreviousByIndividual,
//         EventModelNode::SortMode::IND
//     >
//     (cm->d_ptr->m_pEvents->m_pUnsortedHead->d_ptr->m_pTracker, false, false);
}

/**
 * Fix the events linked list before destroying one of the CM d_ptr gets
 * deleted.
 *
 * @param ContactMethod* dest Will merge "src" events into "dest"
 * @param ContactMethod* src Will get destroyed
 */
void EventModelPrivate::mergeEvents(ContactMethod* dest, ContactMethod* src)
{
    // The source has no events, there is nothing to do
    if (!src->d_ptr->m_pEvents || !src->d_ptr->m_pEvents->m_pUnsortedHead)
        return;

    // The destination has no events, use the source ones
    if (dest->d_ptr->m_pEvents && (!dest->d_ptr->m_pEvents->m_pUnsortedHead) && src->d_ptr->m_pEvents) {
        (*dest->d_ptr->m_pEvents) = (*src->d_ptr->m_pEvents);
        return;
    }
    else if (src->d_ptr->m_pEvents && !dest->d_ptr->m_pEvents) {
        dest->d_ptr->m_pEvents = src->d_ptr->m_pEvents;
        return;
    }
    else if (!dest->d_ptr->m_pEvents)
        dest->d_ptr->m_pEvents = new ContactMethodEvents;

    // If that happens, there is a race condition when inserting events
    // This is not the right place to fix it, it's already too late
    Q_ASSERT(src->d_ptr->m_pEvents->m_pUnsortedHead->d_ptr->m_pTracker);

    // Unsorted merge (append)
    for (auto e = src->d_ptr->m_pEvents->m_pUnsortedHead->d_ptr->m_pTracker; e; e = e->m_pNextByContactMethod) {
        dest->d_ptr->m_pEvents->m_pUnsortedTail->d_ptr->m_pTracker->m_pNextByContactMethod = e;

        e->m_pPreviousByContactMethod = dest->d_ptr->m_pEvents->m_pUnsortedTail->d_ptr->m_pTracker;

        dest->d_ptr->m_pEvents->m_pUnsortedTail = e->m_pEvent;
    }

    if (src->d_ptr->m_pEvents->m_pNewest && (
      (!dest->d_ptr->m_pEvents->m_pNewest) ||
      src->d_ptr->m_pEvents->m_pNewest->stopTimeStamp() >
      dest->d_ptr->m_pEvents->m_pNewest->stopTimeStamp()
    ))
        dest->d_ptr->m_pEvents->m_pNewest = src->d_ptr->m_pEvents->m_pNewest;

    if (src->d_ptr->m_pEvents->m_pOldest && (
      (!dest->d_ptr->m_pEvents->m_pOldest) ||
      src->d_ptr->m_pEvents->m_pOldest->startTimeStamp() <
      dest->d_ptr->m_pEvents->m_pOldest->startTimeStamp()
    ))
        dest->d_ptr->m_pEvents->m_pOldest = src->d_ptr->m_pEvents->m_pOldest;
}

void EventModelPrivate::slotFixCache()
{
    auto e = qobject_cast<Event*>(sender());

    if (!e)
        return;

    m_hUids.remove(e->uid());
}

QSharedPointer<Event> EventModel::nextEvent(const QSharedPointer<Event>& e, ContactMethod* cm) const
{
    return {};
}

QSharedPointer<Event> EventModel::nextEvent(const QSharedPointer<Event>& e, const QSharedPointer<Individual>& cm) const
{
    return {};
}

QSharedPointer<Event> EventModel::previousEvent(const QSharedPointer<Event>& e, ContactMethod* cm) const
{
    return {};
}

QSharedPointer<Event> EventModel::previousEvent(const QSharedPointer<Event>& e, const QSharedPointer<Individual>& cm) const
{
    return {};
}

QSharedPointer<Event> EventModel::oldest(const ContactMethod* cm) const
{
    if (!cm->d_ptr->m_pEvents)
        return nullptr;

    if (!cm->d_ptr->m_pEvents->m_pOldest)
        return nullptr;

    return cm->d_ptr->m_pEvents->m_pOldest->d_ptr->m_pStrongRef;
}

QSharedPointer<Event> EventModel::newest(const ContactMethod* cm) const
{
    if (!cm->d_ptr->m_pEvents)
        return nullptr;

    if (!cm->d_ptr->m_pEvents->m_pNewest)
        return nullptr;

    return cm->d_ptr->m_pEvents->m_pNewest->d_ptr->m_pStrongRef;
}

const QVector< QSharedPointer<Event> >& EventModelPrivate::events(const ContactMethod* cm) const
{
    if (!cm->d_ptr->m_pEvents)
        cm->d_ptr->m_pEvents = new ContactMethodEvents;

    return cm->d_ptr->m_pEvents->m_lEvents;
}

#include <eventmodel.moc>
