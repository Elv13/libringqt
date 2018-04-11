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

#include "typedefs.h"

//Qt
#include <QtCore/QAbstractTableModel>
class QTimer;
class QItemSelectionModel;

//Ring
#include "collectionmanagerinterface.h"
class Account;
class Calendar;
class EventModelPrivate;
#include <libcard/event.h>

/**
 * This model holds all the event.
 *
 * Compared to the previous history support. This model is designed to
 * create submodels based around the concept of identity.
 *
 * It supports grouping the similar events and creating models with multiple
 * individuals.
 *
 */
class LIB_EXPORT EventModel : public QAbstractListModel, public CollectionManagerInterface<Event>
{
   Q_OBJECT

   friend class ContactMethod; // calls into the private API when deduplicating itself
   friend class EventAggregate; // use the private getters to get references on the event list
public:

    virtual ~EventModel();

    // Model functions
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual int rowCount( const QModelIndex& parent = {}) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    QItemSelectionModel* defaultSelectionModel() const;

    QSharedPointer<Event> getById(const QByteArray& eventId, bool placeholder = false) const;

    // Keep event sorting centralized
    inline QSharedPointer<Event> nextEvent(const QSharedPointer<Event>& e, ContactMethod* cm) const;
    inline QSharedPointer<Event> nextEvent(const QSharedPointer<Event>& e, const QSharedPointer<Individual>& cm) const;
    inline QSharedPointer<Event> previousEvent(const QSharedPointer<Event>& e, ContactMethod* cm) const;
    inline QSharedPointer<Event> previousEvent(const QSharedPointer<Event>& e, const QSharedPointer<Individual>& cm) const;

    inline QSharedPointer<Event> oldest(const ContactMethod* cm) const;
    inline QSharedPointer<Event> newest(const ContactMethod* cm) const;

    static EventModel& instance();

Q_SIGNALS:
    void calendarLoaded(Calendar* cal);

private:
    explicit EventModel(QObject* parent = nullptr);

    EventModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(EventModel)

    //Collection interface
    virtual bool addItemCallback   (const Event* item) override;
    virtual bool removeItemCallback(const Event* item) override;

};
