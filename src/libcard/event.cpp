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

// Std
#include <ctime>

// Ring
#include <call.h>
#include "libcard/private/event_p.h"

Event::Event(Event::EventBean attrs, QObject* parent) : ItemBase(parent), d_ptr(new EventPrivate)
{
    if (attrs.startTimeStamp)
        d_ptr->m_StartTimeStamp = attrs.startTimeStamp;

    if (attrs.stopTimeStamp)
        d_ptr->m_StopTimeStamp = attrs.stopTimeStamp;

    d_ptr->m_Direction = attrs.direction;
}

Event::~Event()
{
    delete d_ptr;
}

time_t Event::startTimeStamp() const
{
    return d_ptr->m_StartTimeStamp;
}

time_t Event::stopTimeStamp() const
{
    return d_ptr->m_StopTimeStamp;
}

time_t Event::revTimeStamp() const
{
    return d_ptr->m_RevTimeStamp;
}

QTimeZone* Event::timezone() const
{
    static auto tz = QTimeZone::systemTimeZone();
    return &tz;
}

Call* Event::toHistoryCall() const
{
    return nullptr;
}

QByteArray Event::uid() const
{
    if (d_ptr->m_UID.isEmpty()) {
        // The UID cannot be generated yet, so the event CANNOT exist
        Q_ASSERT(startTimeStamp());

        QString seed = QString::number( std::abs(0x11111111 * rand()), 16).toUpper();


        d_ptr->m_UID = QString("%1-%2-%3@ring.cx")
            .arg(startTimeStamp())
            .arg(stopTimeStamp() - startTimeStamp())
            .arg(seed.left(std::min(seed.size(), 8))).toLatin1();
    }

    return d_ptr->m_UID;
}

QByteArray Event::categoryName(EventCategory cat)
{
    switch(cat) {
        case EventCategory::CALL:
            return "PHONE CALL";
        case EventCategory::DATA_TRANSFER:
            return "DATA TRANSFER";
        case EventCategory::MESSAGE_GOUP:
            return "TEXT MESSAGES";
    }
}

QVariant Event::getCustomProperty(Event::CustomProperties property) const
{
    //TODO
}

void Event::setCustomProperty(Event::CustomProperties property, const QVariant& value)
{
    //TODO
}

bool Event::isSaved() const
{
    return d_ptr->m_IsSaved;
}

QString Event::displayName() const
{
    return d_ptr->m_CN;
}

void Event::setDisplayName(const QString& cn)
{
    d_ptr->m_CN = cn;
}

Event::EventCategory Event::eventCategory() const
{
    return d_ptr->m_EventCategory;
}

Event::Direction Event::direction() const
{
    return d_ptr->m_Direction;
}

Event::Status Event::status() const
{
    return d_ptr->m_Status;
}

QList< QPair<ContactMethod*, QString> > Event::attendees() const
{
    return d_ptr->m_lAttendees;
}

Account* Event::account() const
{
    return d_ptr->m_pAccount;
}

QList<Media::Attachment*> Event::attachedFiles() const
{
    return d_ptr->m_lAttachedFiles;
}

void Event::attachFile(Media::Attachment* file)
{
    d_ptr->m_lAttachedFiles << file;
}

void Event::detachFile(Media::Attachment* file)
{
    d_ptr->m_lAttachedFiles.removeAll(file);
}
