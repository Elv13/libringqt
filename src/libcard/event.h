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

// qt
#include <QtCore/QTimeZone>
#include <QtCore/QSharedPointer>

#include <typedefs.h>
#include <itembase.h>
#include <itemdataroles.h>

class EventPrivate;
class ContactMethod;
class Call;
class Account;

namespace Media {
    class Attachment;
}

namespace Serializable {
    class Group;
}

/**
 * This class represent an event as defined by rfc5545.
 *
 * It allows to store what happening in a calendar style format. It can later
 * be serialized as (for example) an iCalendar file(s) or a database.
 *
 * It is meant to replace the previous .ini representation used to store the
 * call logs or the SQLite database used by some other clients. That file was
 * fine to represent calls but isn't enough for the boarder scope of Ring as a
 * multimedia platform. It also made synchronization of the history across
 * multiple devices harder that it should have been. iCalendar merging is a well
 * understood and studied problem and a better fit for a distributed
 * application.
 *
 * An event is a stand alone object rather that a superclass of Call and
 * TextMessageGroup to allow the `ItemBase` magic to be used on them without
 * interference from this class.
 *
 * It is still worthy to have the calls "manageable" in collections even without
 * the history event attached to them for synchronization and transfer between
 * devices.
 *
 * @see https://tools.ietf.org/html/rfc5545
 */
class LIB_EXPORT Event : public ItemBase
{
    Q_OBJECT
    friend class Calendar; // factory
    friend class CalendarEditor; // serialization
    friend class Serializable::Group; // update the timestamps on new messages
    friend class EventModel; // manager

public:
    Q_PROPERTY(time_t startTimeStamp READ startTimeStamp CONSTANT)
    Q_PROPERTY(time_t stopTimeStamp  READ stopTimeStamp  CONSTANT)

    enum Roles {
        REVISION_COUNT = static_cast<int>(Ring::Role::UserRole) + 1,
        UID             ,
        BEGIN_TIMESTAMP ,
        END_TIMESTAMP   ,
        UPDATE_TIMESTAMP,
        EVENT_CATEGORY  ,
        DIRECTION       ,
    };

    /**
     * The object "class".
     *
     * Each standard event classes have their own standard properties and
     * peculiarities.
     *
     * VJOURNAL is the most useful to log and synchronize past activities.
     */
    enum class Type {
        VEVENT  , /*!< "Real" events with busy time                          */
        VTODO   , /*!< Future things that will require future actions        */
        VALARM  , /*!< Future and (maybe) repeated events with notifications */
        VJOURNAL, /*!< Logs of past activity (no busy time)                  */
    };
    Q_ENUM(Event::Type)

    /**
     * List the supported event types.
     *
     * To avoid unnecessary abstractions, the list is managed manually. This is
     * implemented in conformance with rfc5545 section 3.8.1.2.
     *
     * Custom categories are not directly supported and may cause data loss
     * when saving or synchronizing external events.
     */
    enum class EventCategory {
        OTHER         = 0x01 << 0, /*!< Default contructor value                           */
        CALL          = 0x01 << 1, /*!< rfc5545 section 5 recommends the "PHONE CALL" name */
        DATA_TRANSFER = 0x01 << 2, /*!< Either a download or upload                        */
        MESSAGE_GROUP = 0x01 << 3, /*!< Messages part of the same conversation             */
        ALL           = 0xFFFF,
        COUNT__,
    };
    Q_ENUM(Event::EventCategory)

    /**
     * A finite number of supported attachments for an event.
     *
     * In accordance to rfc5545 section 3.2.8, each attachment has a MIME types
     * (rfc2046), a variable number of parameters and a value (either has an
     * URI, as a payload or as a CID).
     */
    enum class SupportedAttachments {
        AUDIO_RECORDING ,
        TRANSFERRED_FILE,
    };

    /**
     * These properties are (allowed) extensions to the iCal file format to
     * handle use case unique to Ring.
     */
    enum class CustomProperties {
        X_RING_CALLID,
        X_RING_DIRECTION,
    };

    /**
     * Values for the CustomProperties::X_RING_DIRECTION property.
     */
    enum class Direction {
        INCOMING, /*!< Someone has called      */
        OUTGOING, /*!< The user called someone */
    };
    Q_ENUM(Direction)

    /**
     * The status of the entry.
     *
     * The RFC allow custom values to be specified. Here, only X_MISSED is
     * necessary because otherwise the standard values map well into the data
     * model.
     */
    enum class Status {
        TENTATIVE , // 3.8.1.11
        IN_PROCESS, // 3.8.1.11 "IN-PROCESS"
        CANCELLED , // 3.8.1.11
        FINAL     , // 3.8.1.11
        X_MISSED  , // non-standard
    };

    virtual ~Event();

    time_t startTimeStamp() const;
    time_t stopTimeStamp () const;

    EventCategory eventCategory() const;
    Type type() const;

    bool isSaved() const;

    QVariant getCustomProperty(CustomProperties property) const;

    void setCustomProperty(CustomProperties property, const QVariant& value);

    /**
     * The display name (CN).
     */
    QString displayName() const;
    void setDisplayName(const QString& cn);

    /**
     * When the element was last revised.
     *
     * This is used to be able to use append-only strategy when writing the
     * calendar file. If the `UID` is present multiple time, the newest is
     * used.
     */
    time_t revTimeStamp() const; // section-3.8.7.2

    QTimeZone* timezone() const; // section 3.2.19

    Direction direction() const;
    Status status() const;

    /**
     * An event can have attendees from multiple accounts, but has to be
     * owned by a single account.
     *
     * Given how unlikely multiple accounts events are and the enormous extra
     * complexity it would bring, it's wiser to ignore it and have a well
     * defined ownership model.
     */
    Account* account() const;

    /**
     * Allow local files to be attached to an even such as a transcript,
     * screenshots or recordings.
     */
    QList<Media::Attachment*> attachedFiles() const; // section-3.8.1.1 //TODO use Media::File?
    void attachFile(Media::Attachment* file);
    void detachFile(Media::Attachment* file);

    /**
     * Each attendees with the name they used for that event
     */
    QList< QPair<ContactMethod*, QString> > attendees() const;

    /**
     * Define an truly unique identifier for the event that encompass some
     * useful metadata.
     *
     * Contain the startTimeStamp + '-' + (stopTimeStamp-startTimeStamp) + \
     *  + '-' (8 random ASCII chars) @ accountId + .ring.cx
     *
     * Note that stopTimeStamp may change, but not startTimeStamp. So the UID
     * **CANNOT** be parsed to reconstruct anything other than maybe the
     * account (but should *not* do that since that would be a mild privilege
     * escalation where the attacker can cause actions to be associated with
     * another account).
     */
    QByteArray uid() const;

    /**
     * It is possible to update events by using the same UID and a newer
     * DTSTAMP. This is useful to prevent rewriting the complete database
     * everytime something needs to be updated.
     *
     * The calendar keeps track of the number of updated entries and can
     * squash / garbage collect the data when it judge beneficial.
     *
     * This is also a very important aspect of the future cross device
     * synchronization where contact UIDs (vCard) could be added later to
     * past events.
     */
    int revisionCount() const;

    /**
     * Convert the event into a Call object.
     *
     * It can return nullptr if the event is not a call.
     */
    Call* toHistoryCall() const;

    static QByteArray categoryName(EventCategory cat);
    static QByteArray statusName(Status st);
    static EventCategory categoryFromName(const QByteArray& name);
    static Status statusFromName(const QByteArray& name);
    static Type typeFromName(const QByteArray& name);
    static QByteArray typeName(Type t);

    QVariant roleData(int role) const;

private:
    /**
     * The `attrs` are read only and used to load the initial (and often
     * immutable) properties. This class is intended to be managed/created by
     * the Calendars, so there is no point in making any of this public.
     */
    explicit Event(const EventPrivate& attrs, QObject* parent = nullptr);

    EventPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Event)
};

Q_DECLARE_METATYPE(Event*)
Q_DECLARE_METATYPE(QSharedPointer<Event>)
Q_DECLARE_METATYPE(Event::Direction)
DECLARE_ENUM_FLAGS(Event::EventCategory)
