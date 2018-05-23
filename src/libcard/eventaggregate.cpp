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
#include "eventaggregate.h"

// qt
#include <QtCore/QAbstractListModel>

// ring
#include <libcard/private/event_p.h>
#include <libcard/private/eventmodel_p.h>
#include <eventmodel.h>
#include <individual.h>
#include <contactmethod.h>

// libstdc++
#include <new>

struct Multi //TODO
{
    QVector<ContactMethod*> m_lCms;
    QVector<Individual*> m_lInd;
};

/**
 * A group of multiple [audio or video] call event.
 */
struct EventCallGroup final : public EventMetaNode
{
    explicit EventCallGroup() {m_Type = EventMetaNode::Type::CALL_GROUP;}

    enum class Level {
        LEVEL1, /*!< Split the group when there is any media in between*/
        LEVEL2, /*!< Split group when someone else was *CALLED*        */
    } m_Level {Level::LEVEL1};


};

struct EventTimeCategory final : public EventMetaNode
{
    explicit EventTimeCategory() {m_Type = EventMetaNode::Type::TIME_CAT;}
};

struct CalendarViewNode final : public EventMetaNode
{
    explicit CalendarViewNode() {m_Type = EventMetaNode::Type::CALENDAR;}

    enum class Mode {
        DAY  ,
        WEEK ,
        MONTH,
        YEAR ,
    };

    QString m_Name;

    union {
        QVector<CalendarViewNode*>     m_lChildren;
        QVector<QSharedPointer<Event>> m_lEvents;
    };
};

struct RootEventNode final : public EventMetaNode
{
    explicit RootEventNode() {m_Type = EventMetaNode::Type::ROOT;}

    QVector<EventTimeCategory*> m_lTimeCategories;
    QVector<CalendarViewNode* > m_lYears;
};

/// Holds the children of an event in the activity timeline
struct EventTimelineNode final : public EventMetaNode
{
    explicit EventTimelineNode() {m_Type = EventMetaNode::Type::EVENT_DATA;}
};

struct EventHistoryNode final : public EventMetaNode
{
    explicit EventHistoryNode() {m_Type = EventMetaNode::Type::HISTORY;}
};

struct EventAggregateNode final : public EventMetaNode
{
    explicit EventAggregateNode() {m_Type = EventMetaNode::Type::EVENT;}
    QSharedPointer<Event> m_pEvent;
    CalendarViewNode*     m_pCalendarDay  {nullptr};
    EventTimelineNode*    m_pTimelineNode {nullptr};
};

class EventAggregatePrivate final : public QObject
{
    Q_OBJECT
public:
    explicit EventAggregatePrivate() noexcept : QObject(nullptr) {}
    virtual ~EventAggregatePrivate();

    enum class Mode {
        CONTACT_METHOD, /*!< Only a single contact method and nothing else */
        INDIVIDUAL    , /*!< A single individual                           */
        MULTI         , /*!< Multiple individuals or contact methods       */
    } m_Mode {Mode::CONTACT_METHOD};

    // Depends on the m_Mode
//     union { //FIXME it worked, but it was unstable because of unrelated reasons and I wanted to debug less places
        Individual* m_pIndividual;
        Multi* m_Multi;
        ContactMethod* m_pContactMethod;
//     };

    // Attributes
    RootEventNode* m_pRoot { new RootEventNode() };

    //HACK no time to do better
    QVector<QSharedPointer<Event>> m_lAllEvents;

    QSharedPointer<QAbstractItemModel> m_pUnsortedListModel {nullptr};

    QSharedPointer<EventAggregate> m_pWeakSelf {nullptr}; //FIXME use QWeakPointer


    // Helper
    void splitGroup(EventAggregateNode* toSplit, EventAggregateNode* splitWith);

public Q_SLOTS:
    void slotAttendeeAdded(ContactMethod* cm);
    void slotEventChanged();
    void slotEventAdded(QSharedPointer<Event> e);
    void slotEventDetached(QSharedPointer<Event> e);
};


class UnsortedEventListView final : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit UnsortedEventListView(const QSharedPointer<EventAggregatePrivate>& d_ptr);
    virtual ~UnsortedEventListView();

    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    virtual int rowCount( const QModelIndex& parent = {} ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    virtual QModelIndex parent( const QModelIndex& index ) const override;
    virtual QModelIndex index( int row, int column, const QModelIndex& parent=QModelIndex()) const override;

private:
    // This is a private class, no need for d_ptr
    const ContactMethod* m_pCM;
    QMetaObject::Connection m_cCallBack;
    QSharedPointer<EventAggregatePrivate> d_ptr;

    // Prevent the aggregate from being deleted while the model is visible
    QSharedPointer<EventAggregate> m_pParentRef {nullptr};
};

EventAggregate::EventAggregate() : QObject(&EventModel::instance())
{
//     d_ptr = (EventAggregatePrivate*) malloc(sizeof(EventAggregatePrivate));
//     d_ptr = new(d_ptr) EventAggregatePrivate();
    d_ptr = QSharedPointer<EventAggregatePrivate>(new EventAggregatePrivate());

    d_ptr->m_pUnsortedListModel = nullptr;
    d_ptr->m_pWeakSelf = QSharedPointer<EventAggregate>(this);
}

EventAggregate::~EventAggregate()
{
//     d_ptr->m_pUnsortedListModel.~QWeakPointer<QAbstractItemModel>();
//     d_ptr->m_pWeakSelf.~QSharedPointer<EventAggregate>();
//     free(d_ptr);
}

// It cannot be called due to the union, but has to exist
EventAggregatePrivate::~EventAggregatePrivate()
{}

QSharedPointer<QAbstractItemModel> EventAggregate::groupedView() const
{
    return {};
}

QSharedPointer<QAbstractItemModel> EventAggregate::calendarView() const
{
    return {};
}

const QVector< QSharedPointer<Event> > EventAggregate::events() const
{
    return d_ptr->m_lAllEvents;
}

QSharedPointer<QAbstractItemModel> EventAggregate::unsortedListView() const
{
    if (!d_ptr->m_pUnsortedListModel) {
        QSharedPointer<QAbstractItemModel> ret(
            qobject_cast<QAbstractItemModel*>( new UnsortedEventListView(d_ptr))
        );
        d_ptr->m_pUnsortedListModel = ret;
        return ret;
    }

    return d_ptr->m_pUnsortedListModel;
}

QSharedPointer<EventAggregate> EventAggregate::build(ContactMethod* cm)
{
    auto ret = QSharedPointer<EventAggregate>(new EventAggregate);

    ret->d_ptr->m_Mode = EventAggregatePrivate::Mode::CONTACT_METHOD;

    ret->d_ptr->m_lAllEvents = EventModel::instance().d_ptr->events(cm);

    int i=0;
    for (auto e : qAsConst(ret->d_ptr->m_lAllEvents))
        e->setProperty("__singleAggregate", i++); //HACK it has to work-ish ASAP at any cost

    connect(cm, &ContactMethod::eventAdded, ret->d_ptr.data(), &EventAggregatePrivate::slotEventAdded);
    connect(cm, &ContactMethod::eventDetached, ret->d_ptr.data(), &EventAggregatePrivate::slotEventDetached);

    return ret;
}

QSharedPointer<EventAggregate> EventAggregate::build(Individual* ind)
{
    Q_ASSERT(ind);
    auto ret = QSharedPointer<EventAggregate>(new EventAggregate);
    ret->d_ptr->m_Mode = EventAggregatePrivate::Mode::INDIVIDUAL;


    ind->forAllNumbers([ret](ContactMethod* cm) {
        ret->d_ptr->m_lAllEvents.append(EventModel::instance().d_ptr->events(cm));
    });

    int i=0;
    for (auto e : qAsConst(ret->d_ptr->m_lAllEvents))
        e->setProperty("__singleAggregate", i++); //HACK it has to work-ish ASAP at any cost

    connect(ind, &Individual::eventAdded, ret->d_ptr.data(), &EventAggregatePrivate::slotEventAdded);
    connect(ind, &Individual::eventDetached, ret->d_ptr.data(), &EventAggregatePrivate::slotEventDetached);

    return ret;
}


QSharedPointer<EventAggregate> EventAggregate::merge(const QList<QSharedPointer<EventAggregate> >& source)
{
    auto ret =  QSharedPointer<EventAggregate>(new EventAggregate);

    return ret;
}

void EventAggregatePrivate::splitGroup(EventAggregateNode* toSplit, EventAggregateNode* splitWith)
{
    //
}

void EventAggregatePrivate::slotAttendeeAdded(ContactMethod* cm)
{

}

void EventAggregatePrivate::slotEventChanged()
{
    if (QSharedPointer<QAbstractItemModel> ptr = m_pUnsortedListModel) {
        auto m = qobject_cast<UnsortedEventListView*>(ptr.data());
    }
}

void EventAggregatePrivate::slotEventAdded(QSharedPointer<Event> e)
{
    e->setProperty("__singleAggregate", m_lAllEvents.size()); //HACK it has to work-ish ASAP at any cost

    m_lAllEvents << e;

    /*if (QSharedPointer<QAbstractItemModel> ptr = m_pUnsortedListModel) {
        auto m = qobject_cast<UnsortedEventListView*>(ptr.data());
    }*/
}

void EventAggregatePrivate::slotEventDetached(QSharedPointer<Event> e)
{

}

UnsortedEventListView::UnsortedEventListView(const QSharedPointer<EventAggregatePrivate>& d) :
    QAbstractListModel(d.data()), d_ptr(d), m_pParentRef(d->m_pWeakSelf)
{
}

UnsortedEventListView::~UnsortedEventListView()
{

}

QVariant UnsortedEventListView::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    return d_ptr->m_lAllEvents[index.row()]->roleData(role);
}

int UnsortedEventListView::rowCount( const QModelIndex& parent ) const
{
    return d_ptr->m_lAllEvents.count();
}

QHash<int,QByteArray> UnsortedEventListView::roleNames() const
{
    return EventModel::instance().roleNames();
}

QModelIndex UnsortedEventListView::parent( const QModelIndex& index ) const
{
    return {};
}

QModelIndex UnsortedEventListView::index( int row, int column, const QModelIndex& parent ) const
{
    if (column || row < 0 || row >= d_ptr->m_lAllEvents.count())
        return {};

    if (parent.isValid())
        return {};

    // The pointer is necessary to avoid lookup when `emit dataChanged()` is
    // required. This is why this model overrides ::parent and ::index
    return createIndex(row, 0, d_ptr->m_lAllEvents[row].data());
}


#include <eventaggregate.moc>
