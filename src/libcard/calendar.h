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

#include "collectioninterface.h"
#include "collectioneditor.h"

class QTimeZone;

class Event;
class Account;
class Call;
class EventPrivate;

class CalendarPrivate;

/**
 * This class provide a calendar as defined in rfc5545.
 *
 * Calendars are collections of events and the various associated metadatas.
 *
 */
class LIB_EXPORT Calendar : public CollectionInterface
{
public:
    explicit Calendar(CollectionMediator<Event>* mediator, Account* a);
    virtual ~Calendar();

    virtual bool load  () override;
    virtual bool reload() override;
    virtual bool clear () override;

    virtual QString    name     () const override;
    virtual bool       isEnabled() const override;
    virtual QString    category () const override;
    virtual QByteArray id       () const override;

    Account* account() const;

    QString path() const;

    QSharedPointer<Event> addEvent(Call* c);

    /**
     * This version is designed for internal usage only.
     */
    QSharedPointer<Event> addEvent(const EventPrivate& data);

    /**
     * All timezone used by this calendar.
     */
    QList<QTimeZone*> timezones() const;

    QSharedPointer<Event> eventAt(int position) const;

    virtual FlagPack<SupportedFeatures> supportedFeatures() const override;

private:
    CalendarPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Calendar)
};
