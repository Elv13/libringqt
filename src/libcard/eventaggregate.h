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
#pragma once

#include <typedefs.h>

// Qt
#include <QtCore/QObject>
class QAbstractItemModel;

// Ring
class EventAggregatePrivate;
class EventAggregateIterator;
class ContactMethod;
class Individual;

/**
 * This class creates "virtual" views of the EventModel.
 *
 * It is possible to add an arbitrary number of ContactMethods, Person or
 * Individuals and then arrange the resulting events in various topologies.
 */
class LIB_EXPORT EventAggregate : public QObject
{
    Q_OBJECT
public:

    virtual ~EventAggregate();

    /// Group similar events together
    QSharedPointer<QAbstractItemModel> groupedView() const;

    /// A grid with cells representing days and their children events for that day
    QSharedPointer<QAbstractItemModel> calendarView() const;

    static QSharedPointer<EventAggregate> buildFromContactMethod(ContactMethod* cm);
    static QSharedPointer<EventAggregate> buildFromIndividual(ContactMethod* cm);

    /** The correct way to create multi-party aggregates is to create them
     * individually then merge them. Most of the memory and computation will be
     * shared.
     */
    static QSharedPointer<EventAggregate> merge(
        const QList<QSharedPointer<EventAggregate> >& source
    );


private:
    explicit EventAggregate();

    EventAggregatePrivate* d_ptr;
    Q_DECLARE_PRIVATE(EventAggregate)
};
