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
#include "event.h"

class EventPrivate
{
public:
    QByteArray m_UID;
};

Event::Event(QObject* parent) : ItemBase(parent), d_ptr(new EventPrivate)
{
}

Event::~Event()
{
    delete d_ptr;
}

time_t Event::startTimeStamp() const
{
    return 0;
}

time_t Event::stopTimeStamp() const
{
    return 0;
}

time_t Event::revTimeStamp() const
{
    return 0;
}

QTimeZone* Event::timezone() const
{
    return nullptr;
}

QVector<ContactMethod*> Event::attendees() const
{
    return {};
}

Call* Event::toHistoryCall() const
{
    return nullptr;
}

QByteArray Event::uid() const
{
    if (d_ptr->m_UID.isEmpty()) {

    }

    return d_ptr->m_UID;
}

QByteArray Event::categoryName(EventCategory cat) const
{
    //
}

QVariant Event::getCustomProperty(Event::CustomProperties property) const
{
    //
}

void Event::setCustomProperty(Event::CustomProperties property, const QVariant& value)
{
    //
}
