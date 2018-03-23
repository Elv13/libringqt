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
#include <account.h>
#include "libcard/private/event_p.h"

Event::Event(const EventPrivate& attrs, QObject* parent) : ItemBase(parent), d_ptr(new EventPrivate)
{
    (*d_ptr) = attrs;
    d_ptr->m_pStrongRef = QSharedPointer<Event>(this);
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

        const QString accId(account() ? account()->id() : "void");

        d_ptr->m_UID = QString("%1-%2-%3@%4.ring.cx")
            .arg(startTimeStamp())
            .arg(stopTimeStamp() - startTimeStamp())
            .arg(seed.left(std::min(seed.size(), 8)))
            .arg(accId).toLatin1();
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
        case EventCategory::MESSAGE_GROUP:
            return "TEXT MESSAGES";
        case EventCategory::ALL:
        case EventCategory::COUNT__:
            Q_ASSERT(false);
    }

    // Memory corruption, it's already game over
    Q_ASSERT(false);
    return {};
}

QByteArray Event::statusName(Status st)
{
    switch(st) {
        case Event::Status::TENTATIVE:
            return "TENTATIVE";
        case Event::Status::IN_PROCESS:
            return "IN-PROCESS";
        case Event::Status::CANCELLED:
            return "CANCELLED";
        case Event::Status::FINAL:
            return "FINAL";
        case Event::Status::X_MISSED:
            return "X-MISSED";
    }

    // Memory corruption, it's already game over
    Q_ASSERT(false);
    return {};
}

Event::EventCategory Event::categoryFromName(const QByteArray& name)
{
    static QHash<QByteArray, Event::EventCategory> vals {
        { "PHONE CALL"   , EventCategory::CALL           },
        { "DATA TRANSFER", EventCategory::DATA_TRANSFER  },
        { "TEXT MESSAGES", EventCategory::MESSAGE_GROUP  },
    };

    return vals[name];
}

Event::Status Event::statusFromName(const QByteArray& name)
{
    static QHash<QByteArray, Event::Status> vals  {
        { "TENTATIVE" , Event::Status::TENTATIVE  },
        { "IN-PROCESS", Event::Status::IN_PROCESS },
        { "CANCELLED" , Event::Status::CANCELLED  },
        { "FINAL"     , Event::Status::FINAL      },
        { "X-MISSED"  , Event::Status::X_MISSED   },
    };

    return vals[name];
}

Event::Type Event::typeFromName(const QByteArray& name)
{
    static QHash<QByteArray, Event::Type> vals  {
        { "VEVENT"  , Event::Type::VEVENT   },
        { "VTODO"   , Event::Type::VTODO    },
        { "VALARM"  , Event::Type::VALARM   },
        { "VJOURNAL", Event::Type::VJOURNAL },
    };

    return vals[name];
}

QByteArray Event::typeName(Event::Type t)
{
    switch(t) {
        case Type::VEVENT:
            return "VEVENT";
        case Type::VTODO:
            return "VTODO";
        case Type::VALARM:
            return "VALARM";
        case Type::VJOURNAL:
            return "VJOURNAL";
    }

    Q_ASSERT(false);
    return {};
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

Event::Type Event::type() const
{
    return d_ptr->m_Type;
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

int Event::revisionCount() const
{
    return d_ptr->m_RevCounter;
}

QVariant Event::roleData(int role) const
{
    switch(role) {
        case Qt::DisplayRole:
        case Event::Roles::UID             :
            return uid();
        case Event::Roles::REVISION_COUNT  :
            return QVariant::fromValue(revisionCount());
        case Event::Roles::BEGIN_TIMESTAMP :
            return QVariant::fromValue(startTimeStamp());
        case Event::Roles::END_TIMESTAMP   :
            return QVariant::fromValue(stopTimeStamp());
        case Event::Roles::UPDATE_TIMESTAMP:
            return QVariant::fromValue(revTimeStamp());
        case Event::Roles::EVENT_CATEGORY  :
            return QVariant::fromValue(eventCategory());
        case Event::Roles::DIRECTION       :
            return QVariant::fromValue(direction());
    }

    return {};
}
