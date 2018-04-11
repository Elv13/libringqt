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

#include <libcard/event.h>

// Defined in EventModel and used to keep track of the attendees
struct EventModelNode;

// "Really" private data too sensitive to be shared event in the private API
struct EventInternals;

/**
 * Superclass of all type of event model nodes.
 *
 * This allows multiple models to share the same memory tree.
 */
struct EventMetaNode
{
    enum class Type {
        EVENT     ,
        CALENDAR  ,
        HISTORY   ,
        EVENT_DATA,
        TIME_CAT  ,
        CALL_GROUP,
        ROOT      ,
    } m_Type {Type::EVENT};
    uint m_Index{0};
    time_t m_StartTime {0};
    time_t m_EndTime   {0};
};

/**
 * The event private data
 */
class EventPrivate
{
public:
    QByteArray m_UID;
    time_t m_StartTimeStamp {0};
    time_t m_StopTimeStamp  {0};
    time_t m_RevTimeStamp   {0};
    uint   m_RevCounter     {0};
    QString m_CN;
    Account* m_pAccount {nullptr};
    QList<Media::Attachment*> m_lAttachedFiles;
    Event::EventCategory  m_EventCategory {Event::EventCategory::CALL};
    Event::Direction m_Direction {Event::Direction::OUTGOING};
    Event::Status m_Status {Event::Status::CANCELLED};
    Event::Type m_Type {Event::Type::VJOURNAL};
    QList< QPair<ContactMethod*, QString> > m_lAttendees;

    /**
     * Either the call was from this session or it's an imported Call from the
     * old sflphone history.
     */
    bool m_HasImportedCall {false};

    EventModelNode* m_pTracker {nullptr};

    EventInternals* m_pInternals {nullptr};

    /**
     * Use a strong ref so event managed by collections don't get accidentally
     * deleted.
     */
    QSharedPointer<Event> m_pStrongRef;
};
