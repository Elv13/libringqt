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

// Qt
#include <QtCore/QUrl>

// Ring
#include <call.h>
#include "flagutils.h"
#include <account.h>
#include <contactmethod.h>
#include <individual.h>
#include "libcard/private/event_p.h"
#include "libcard/matrixutils.h"

/**
 * Use a second "level" is private data alongside ::EventPrivate.
 *
 * EventPrivate is used internally for building events and managing some shared
 * internal metadata. It isn't the place to track the state.
 */
struct EventInternals
{
    Event::SyncState m_SyncState {Event::SyncState::NEW};

    /**
     * The action that affect the SyncState.
     *
     * Note that ::Event itself doesn't do much since it's the Calendar job
     * to synchronize everything.
     */
    enum class EditActions {
        MODIFY      ,
        DELETE      ,
        SAVE        ,
        CONFIRM_SYNC,
        COUNT__
    };

    Event::SyncState performAction(EditActions action);

    // Attributes
    Event* q_ptr {nullptr};
    static const Matrix2D<Event::SyncState, EditActions, Event::SyncState> m_StateMap;

    // Helper
};

#define ST Event::SyncState::
#define EA EventInternals::EditActions
static EnumClassReordering<EA> acts =
{                             EA::MODIFY  ,  EA::DELETE  ,     EA::SAVE   , EA::CONFIRM_SYNC   };
const Matrix2D<Event::SyncState, EventInternals::EditActions, Event::SyncState> EventInternals::m_StateMap = {
{ST NEW          , {{acts, {ST NEW        , ST DISCARDED , ST SAVED       , ST ERROR        }}}},
{ST IMPORTED     , {{acts, {ST MODIFIED   , ST DISCARDED , ST SAVED       , ST IMPORTED     }}}},
{ST MODIFIED     , {{acts, {ST MODIFIED   , ST DISCARDED , ST SAVED       , ST MODIFIED     }}}},
{ST RESCHEDULED  , {{acts, {ST RESCHEDULED, ST DISCARDED , ST SAVED       , ST MODIFIED     }}}},
{ST SAVED        , {{acts, {ST MODIFIED   , ST DISCARDED , ST SAVED       , ST SYNCHRONIZED }}}},
{ST SYNCHRONIZED , {{acts, {ST MODIFIED   , ST DISCARDED , ST SYNCHRONIZED, ST SYNCHRONIZED }}}},
{ST DISCARDED    , {{acts, {ST ERROR      , ST ERROR     , ST ERROR       , ST DISCARDED    }}}},
{ST CANCELLED    , {{acts, {ST ERROR      , ST DISCARDED , ST ERROR       , ST CANCELLED    }}}},
{ST ERROR        , {{acts, {ST ERROR      , ST ERROR     , ST ERROR       , ST ERROR        }}}},
{ST PLACEHOLDER  , {{acts, {ST ERROR      , ST ERROR     , ST ERROR       , ST ERROR        }}}}
};
#undef EA
#undef ST

// Note that the nullptr parent is intentional. The events are managed using
// shared pointers.
Event::Event(const EventPrivate& attrs, Event::SyncState st) : ItemBase(nullptr),
    d_ptr(new EventPrivate)
{
    (*d_ptr) = attrs;
    d_ptr->m_pStrongRef = QSharedPointer<Event>(this);
    d_ptr->m_pInternals = new EventInternals();
    d_ptr->m_pInternals->q_ptr = this;
    d_ptr->m_pInternals->m_SyncState = st;

    if (!d_ptr->m_UID.isEmpty())
        setObjectName("Event: " + d_ptr->m_UID);
//     Q_ASSERT(st != Event::SyncState::NEW); //TODO remove
}

void Event::rebuild(const EventPrivate& attrs, SyncState st)
{
    Q_ASSERT(syncState() == Event::SyncState::PLACEHOLDER);
    Q_ASSERT(uid() == attrs.m_UID);
    const auto i = d_ptr->m_pInternals;
    (*d_ptr) = attrs;
    d_ptr->m_pInternals = i;
}

Event::~Event()
{
    d_ptr->m_pStrongRef = nullptr;
    delete d_ptr;
}

void Event::setStopTimeStamp(time_t t)
{
    if (syncState() == Event::SyncState::PLACEHOLDER) {
        qWarning() << "Trying to modify an event currently loading";
        Q_ASSERT(false);
    }

    if (t == d_ptr->m_StopTimeStamp)
        return;

    d_ptr->m_pInternals->m_SyncState = Event::SyncState::RESCHEDULED;

    d_ptr->m_StopTimeStamp = t;
    emit changed();
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

        const_cast<Event*>(this)->setObjectName("Event: " + d_ptr->m_UID);
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
    if (syncState() == Event::SyncState::PLACEHOLDER) {
        qWarning() << "Trying to modify an event currently loading";
        Q_ASSERT(false);
    }
    //TODO
}

bool Event::isSaved() const
{
    switch(syncState()) {
        case Event::SyncState::NEW:
        case Event::SyncState::IMPORTED:
        case Event::SyncState::MODIFIED:
        case Event::SyncState::RESCHEDULED:
            return false;
        case Event::SyncState::DISCARDED:
        case Event::SyncState::SAVED:
        case Event::SyncState::SYNCHRONIZED:
        case Event::SyncState::CANCELLED:
        case Event::SyncState::ERROR:
            return true;
        case Event::SyncState::COUNT__:
            Q_ASSERT(false);
    }

    Q_ASSERT(false);
    return false;
}

///Generate an human readable string from the difference between StartTimeStamp and StopTimeStamp (or 'now')
QString Event::length() const
{
   if (stopTimeStamp() == startTimeStamp())
      return QString(); //Invalid

   int nsec =0;
   if (stopTimeStamp())
      nsec = stopTimeStamp() - startTimeStamp();//If the call is over
   else { //Time to now
      time_t curTime;
      ::time(&curTime);
      nsec = curTime - startTimeStamp();
   }
   if (nsec/3600)
      return QStringLiteral("%1:%2:%3 ").arg((nsec%(3600*24))/3600).arg(((nsec%(3600*24))%3600)/60,2,10,QChar('0')).arg(((nsec%(3600*24))%3600)%60,2,10,QChar('0'));
   else
      return QStringLiteral("%1:%2 ").arg(nsec/60,2,10,QChar('0')).arg(nsec%60,2,10,QChar('0'));
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

bool Event::hasAttachment(Media::Attachment::BuiltInTypes t) const
{
    for (auto a : qAsConst(d_ptr->m_lAttachedFiles)) {
        if (a->type() == t)
            return true;
    }

    return false;
}

bool Event::hasAttachment(const QUrl& path) const
{
    for (auto a : qAsConst(d_ptr->m_lAttachedFiles)) {
        if (a->path() == path)
            return true;
    }

    return false;
}

int Event::revisionCount() const
{
    return d_ptr->m_RevCounter;
}

QVariant Event::roleData(int role) const
{
    switch(role) {
        case (int) Ring::Role::Object:
            return QVariant::fromValue(ref());

        case (int) Ring::Role::ObjectType:
            return QVariant::fromValue(Ring::ObjectType::Event);
        case (int) Ring::Role::FormattedLastUsed:
            return HistoryTimeCategoryModel::timeToHistoryCategory(stopTimeStamp());
        case (int) Ring::Role::IndexedLastUsed:
            return QVariant(static_cast<int>(
                HistoryTimeCategoryModel::timeToHistoryConst(stopTimeStamp())
            ));
        case (int) Ring::Role::Length:
            return length();
        case (int) Ring::Role::IsPresent:
            return false; //TODO
        case (int) Ring::Role::UnreadTextMessageCount:
            return 0; //TODO
        case (int) Ring::Role::IsBookmarked:
        case (int) Ring::Role::HasActiveCall:
        case (int) Ring::Role::HasActiveVideo:
        case (int) Ring::Role::IsRecording:
            return false;
        case (int) Ring::Role::Name:
        case Qt::DisplayRole:
        case (int) Ring::Role::Number:
        case (int) Ring::Role::URI:
        case Event::Roles::UID             :
            return uid();
        case Event::Roles::REVISION_COUNT  :
            return QVariant::fromValue(revisionCount());
        case Event::Roles::BEGIN_TIMESTAMP :
            return QVariant::fromValue(startTimeStamp());
        case Event::Roles::END_TIMESTAMP   :
        case (int) Ring::Role::LastUsed:
            return QVariant::fromValue(stopTimeStamp());
        case Event::Roles::UPDATE_TIMESTAMP:
            return QVariant::fromValue(revTimeStamp());
        case Event::Roles::EVENT_CATEGORY  :
            return QVariant::fromValue(eventCategory());
        case Event::Roles::DIRECTION       :
            return QVariant::fromValue(direction());
        case Event::Roles::HAS_AV_RECORDING:
            return hasAttachment(Media::Attachment::BuiltInTypes::AUDIO_RECORDING);
        case Event::Roles::STATUS:
            return QVariant::fromValue(status());
        case (int) Ring::Role::State:
        case (int) Ring::Role::FormattedState:
        case (int) Ring::Role::DropState:
            return {};
    }

    return {};
}

Event::SyncState Event::syncState() const
{
    return d_ptr->m_pInternals->m_SyncState;
}

bool Event::hasAttendee(ContactMethod* cm) const
{
    const auto b(d_ptr->m_lAttendees.constBegin()), e(d_ptr->m_lAttendees.constEnd());
    return std::find_if(b, e, [cm](const QPair<ContactMethod*, QString>& other) {
        return cm->d() == other.first->d();
    }) != e;
}

bool Event::hasAttendee(Individual* ind) const
{
    const auto b(d_ptr->m_lAttendees.constBegin()), e(d_ptr->m_lAttendees.constEnd());
    return std::find_if(b, e, [ind](const QPair<ContactMethod*, QString>& other) {
        return ind == other.first->individual();
    }) != e;
}

QSharedPointer<Event> Event::ref() const
{
    Q_ASSERT(d_ptr->m_pStrongRef);
    return d_ptr->m_pStrongRef;
}

Event::SyncState EventInternals::performAction(EventInternals::EditActions action)
{
    auto old = m_SyncState;

    m_SyncState = m_StateMap[old][action];

    if (m_SyncState != old) {
        emit q_ptr->syncStateChanged(m_SyncState, old);
        emit q_ptr->changed();
    }
    //

    return m_SyncState;
}

bool Event::save() const
{
    if (ItemBase::save())
        return d_ptr->m_pInternals->performAction(EventInternals::EditActions::SAVE) !=
            Event::SyncState::ERROR;

    return false;
}

bool Event::edit()
{
    if (ItemBase::edit())
        return d_ptr->m_pInternals->performAction(EventInternals::EditActions::MODIFY) !=
            Event::SyncState::ERROR;

    return false;
}

bool Event::remove()
{
    d_ptr->m_Status = Event::Status::CANCELLED;

    if (ItemBase::remove())
        return d_ptr->m_pInternals->performAction(EventInternals::EditActions::DELETE) !=
            Event::SyncState::ERROR;

    return false;
}

bool Event::addAttendee(ContactMethod* cm, const QString& name)
{
    if (hasAttendee(cm))
        return false;

    const auto ind = cm->individual();

    const bool hasInd = hasAttendee(ind);

    d_ptr->m_lAttendees << QPair<ContactMethod*, QString> {cm, name};

    // Let it happen, someone calling her/himself to leave voicemails or
    // something like this isn't all that unusual.
    if (hasInd) {
        qWarning() << "Trying to add the same indivudual twice to the same event"
            << cm << ind;
        emit attendeeAdded(ind);
    }

    emit attendeeAdded(cm);
    emit changed();

    return true;
}
