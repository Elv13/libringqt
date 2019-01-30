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

#include "picocms/collectioninterface.h"
#include "picocms/collectioneditor.h"

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
class LIB_EXPORT Calendar : public QObject, public CollectionInterface
{
    Q_OBJECT

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

    int unsavedCount() const;

    /**
     * Return a sorted list of unsaved events (oldest first)
     *
     * @warning, this function can be CPU intensive due to the sorting
     */
    QList<QSharedPointer<Event> > unsavedEvents() const;

    virtual FlagPack<SupportedFeatures> supportedFeatures() const override;

    bool isLoaded() const;

Q_SIGNALS:
    void loadingFinished();

private:
    CalendarPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Calendar)
};
