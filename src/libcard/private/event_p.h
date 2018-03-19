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

class EventPrivate
{
public:
    QByteArray m_UID;
    time_t m_StartTimeStamp {0};
    time_t m_StopTimeStamp  {0};
    time_t m_RevTimeStamp   {0};
    uint   m_RevCounter     {0};
    QString m_CN;
    bool m_IsSaved {false};
    Account* m_pAccount {nullptr};
    QList<Media::Attachment*> m_lAttachedFiles;
    Event::EventCategory  m_EventCategory {Event::EventCategory::CALL};
    Event::Direction m_Direction {Event::Direction::OUTGOING};
    Event::Status m_Status {Event::Status::CANCELLED};
    QList< QPair<ContactMethod*, QString> > m_lAttendees;

};
