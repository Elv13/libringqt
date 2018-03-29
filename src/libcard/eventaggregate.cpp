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

#include <libcard/private/event_p.h>
#include <eventmodel.h>

class EventAggregatePrivate
{
public:

    // Attributes
    EventAggregateNode* m_pRoot {nullptr};


    // Helper
    void splitGroup(EventAggregateNode* toSplit, EventAggregateNode* splitWith);
};


EventAggregate::EventAggregate() : QObject(&EventModel::instance())
{

}

EventAggregate::~EventAggregate()
{
    delete d_ptr;
}

QSharedPointer<QAbstractItemModel> EventAggregate::groupedView() const
{
    return {};
}

QSharedPointer<QAbstractItemModel> EventAggregate::calendarView() const
{
    return {};
}

QSharedPointer<EventAggregate> EventAggregate::buildFromContactMethod(ContactMethod* cm)
{
    auto ret =  QSharedPointer<EventAggregate>(new EventAggregate);

    return ret;
}

QSharedPointer<EventAggregate> EventAggregate::buildFromIndividual(ContactMethod* cm)
{
    auto ret =  QSharedPointer<EventAggregate>(new EventAggregate);

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
