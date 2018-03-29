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
#include "calendar.h"

// Qt
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>

// Ring
#include "event.h"
#include <call.h>
#include <account.h>
#include <accountmodel.h>
#include <personmodel.h>
#include <eventmodel.h>
#include <phonedirectorymodel.h>
#include <media/avrecording.h>
#include "libcard/private/event_p.h"
#include "libcard/private/icsbuilder.h"
#include "libcard/private/icsloader.h"

class CalendarEditor final : public CollectionEditor<Event>
{
public:
   explicit CalendarEditor(CollectionMediator<Event>* m);
   virtual bool save       ( const Event* item ) override;
   virtual bool remove     ( const Event* item ) override;
   virtual bool edit       ( Event*       item ) override;
   virtual bool addNew     ( Event*       item ) override;
   virtual bool addExisting( const Event* item ) override;

   //Attributes
   QVector<Event*> m_lItems;

   Calendar* m_pCal {nullptr};

private:
   virtual QVector<Event*> items() const override;


};

class CalendarPrivate final
{
public:
    // Attributes
    Account* m_pAccount {nullptr};
    CalendarEditor* m_pEditor {nullptr};

    Calendar* q_ptr;

    // Helpers
    Event* getEvent(const EventPrivate& data, bool isNew);
    Event* updateEvent(Event* e, const EventPrivate& data);
};

Calendar::Calendar(CollectionMediator<Event>* mediator, Account* a) : CollectionInterface(new CalendarEditor(mediator)),
d_ptr(new CalendarPrivate)
{
    d_ptr->q_ptr      = this;
    d_ptr->m_pAccount = a;
    d_ptr->m_pEditor  = static_cast<CalendarEditor*>(editor<Event>());
    d_ptr->m_pEditor->m_pCal = this;
}

Calendar::~Calendar()
{
    delete d_ptr;
}

bool Calendar::load()
{
    ICSLoader l;
    auto calendarAdapter = std::shared_ptr<VObjectAdapter<Calendar>>(
        new VObjectAdapter<Calendar>
    );

    auto eventAdapter = std::shared_ptr<VObjectAdapter<EventPrivate>>(
        new  VObjectAdapter<EventPrivate>
    );

    // It was very unreadable without it
#define ARGS (EventPrivate* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params)

    eventAdapter->addPropertyHandler("DTSTART", []ARGS {
        self->m_StartTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("DTEND", []ARGS {
        self->m_StopTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("DTSTAMP", []ARGS {
        self->m_RevTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("ATTENDEE", [this]ARGS {
        const QByteArray val = QByteArray::fromRawData(value.data(), value.size());

        Account* a = nullptr;
        Person*  p = nullptr;

        for (auto param : params) {
            const QByteArray pKey = QByteArray::fromRawData(param.first.data (), param.first.size ());
            const QByteArray pVal = QByteArray::fromRawData(param.second.data(), param.second.size());

            if (pKey == "CN")
                self->m_CN = pVal;
            else if (pKey == "UID")
                p = PersonModel::instance().getPlaceHolder(pVal);
            else if (pKey == "X_RING_ACCOUNTID")
                a = AccountModel::instance().getById(pVal);
        }

        self->m_lAttendees << QPair<ContactMethod*, QString> {
            PhoneDirectoryModel::instance().getNumber(val, p, a ? a : account()),
            self->m_CN
        };
    });

    eventAdapter->addPropertyHandler("CATEGORIES", []ARGS {
        const QByteArray val = QByteArray::fromRawData(value.data(), value.size());

        self->m_EventCategory = Event::categoryFromName(val);
    });

    eventAdapter->addPropertyHandler("STATUS", []ARGS {
        const QByteArray val = QByteArray::fromRawData(value.data(), value.size());

        self->m_Status = Event::statusFromName(val);
    });

    eventAdapter->addPropertyHandler("X_RING_DIRECTION", []ARGS {
        self->m_Direction = value == "OUTGOING" ?
            Event::Direction::OUTGOING : Event::Direction::INCOMING;
    });

    eventAdapter->addPropertyHandler("UID", []ARGS {
        self->m_UID = value.data();
    });

    // All events are part of this calendar file, so assume it can be ignored
    calendarAdapter->setObjectFactory([this](const std::basic_string<char>& object_type) -> Calendar* {
        return this;
    });

    // No need to allocate anything for each events, it will happen anyway
    // in Event:: constructor
    EventPrivate e;

    eventAdapter->setObjectFactory([this, &e](const std::basic_string<char>& object_type) -> EventPrivate* {
        e = {};
        e.m_Type = Event::typeFromName(object_type.data());
        return &e;
    });

    calendarAdapter->setFallbackObjectHandler<EventPrivate>(
        [this](
           Calendar* self,
           EventPrivate* child,
           const std::basic_string<char>& name
        ) {
            d_ptr->getEvent(*child, false);
    });

    l.registerVObjectAdaptor("VCALENDAR", calendarAdapter);
    l.registerVObjectAdaptor("VEVENT"   , eventAdapter   );
    l.registerVObjectAdaptor("VJOURNAL" , eventAdapter   );
    l.registerVObjectAdaptor("VTODO"    , eventAdapter   );
    l.registerVObjectAdaptor("VALARM"   , eventAdapter   );

    l.loadFile(path().toLatin1().data());

#undef ARGS

    return true;
}

bool Calendar::reload()
{
    return true;
}

bool Calendar::clear()
{
    return false;
}

QString Calendar::name() const
{
    return QStringLiteral("iCalendar");
}

QString Calendar::category() const
{
    return QStringLiteral("History");
}

QByteArray Calendar::id() const
{
    return "ical"+account()->id();
}

QString Calendar::path() const
{
    static const QString dir = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/iCal/";
    return dir + '/' + account()->id() + ".ics";
}

FlagPack<CollectionInterface::SupportedFeatures> Calendar::supportedFeatures() const
{
    return
      CollectionInterface::SupportedFeatures::NONE       |
      CollectionInterface::SupportedFeatures::LOAD       |
      CollectionInterface::SupportedFeatures::CLEAR      |
      CollectionInterface::SupportedFeatures::MANAGEABLE |
      CollectionInterface::SupportedFeatures::REMOVE     |
      CollectionInterface::SupportedFeatures::EDIT       |
      CollectionInterface::SupportedFeatures::SAVE       |
      CollectionInterface::SupportedFeatures::ADD        ;
}

CalendarEditor::CalendarEditor(CollectionMediator<Event>* m) : CollectionEditor<Event>(m)
{

}

bool CalendarEditor::save( const Event* item )
{
    static QString dir = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/iCal/";
    if (!QDir().mkpath(dir)) {
        qWarning() << "cannot create path for fallbackcollection: " << dir;
        return false;
    }

    ICSBuilder::appendToCalendar(m_pCal, const_cast<Event*>(item), [item](bool success) {
        item->d_ptr->m_IsSaved = success;
    });

    return true;
}

bool CalendarEditor::remove( const Event* item )
{
    return true;
}

bool CalendarEditor::edit( Event* item )
{
    return true;
}

bool CalendarEditor::addNew( Event* item )
{
    return save(item);
}

bool CalendarEditor::addExisting( const Event* item )
{
    m_lItems << const_cast<Event*>(item);
    mediator()->addItem(item);
    return true;
}

QVector<Event*> CalendarEditor::items() const
{
    return m_lItems;
}

bool Calendar::isEnabled() const
{
    return true;
}

QList<QTimeZone*> Calendar::timezones() const
{
    return {};
}

QSharedPointer<Event> Calendar::eventAt(int position) const
{
    if (position < 0 || position >= d_ptr->m_pEditor->m_lItems.size())
        return nullptr;

    return d_ptr->m_pEditor->m_lItems[position]->d_ptr->m_pStrongRef;
}

Account* Calendar::account() const
{
    return d_ptr->m_pAccount;
}

QSharedPointer<Event> Calendar::addEvent(Call* c)
{
    if (!c)
        return nullptr;

    if (c->lifeCycleState() != Call::LifeCycleState::FINISHED)
        return nullptr;

    if (c->calendarEvent()) {
        Q_ASSERT(false);
        return c->calendarEvent();
    }

    EventPrivate b;
    b.m_StartTimeStamp = c->startTimeStamp();
    b.m_StopTimeStamp  = c->stopTimeStamp ();
    b.m_Direction      = c->direction() == Call::Direction::OUTGOING ?
        Event::Direction::OUTGOING : Event::Direction::INCOMING;

    auto e = new Event(b);

    // If this event is newly imported, then "now" is the last time it was
    // modified
    time_t curTime;
    ::time(&curTime);

    e->d_ptr->m_pAccount     = account();
    e->d_ptr->m_RevTimeStamp = curTime;
    e->d_ptr->m_CN           = c->peerName();

    e->d_ptr->m_lAttendees << QPair<ContactMethod*, QString> {
        c->peerContactMethod(), c->peerName()
    };

    // Add the audio recordings
    if (c->hasRecording(Media::Media::Type::AUDIO, Media::Media::Direction::IN)) {
        const auto rec = static_cast<Media::AVRecording*> (
            c->recordings(Media::Media::Type::AUDIO, Media::Media::Direction::IN).first()
        );
        e->attachFile(rec);
    }

    editor<Event>()->addNew(e);

    return e->d_ptr->m_pStrongRef;
}

/**
 * Update an existing event.
 *
 * If the revision timestamp is greater than the older one, squash the new
 * values on top of the old ones.
 */
Event* CalendarPrivate::updateEvent(Event* e, const EventPrivate& data)
{
    if (e->revTimeStamp() < data.m_RevTimeStamp) {
        //TODO
    }

    return e;
}

/// Ensure no duplicates are created by accident
Event* CalendarPrivate::getEvent(const EventPrivate& data, bool isNew)
{
    if (auto ret = data.m_UID.isEmpty() ? nullptr : EventModel::instance().getById(data.m_UID)) {
        return updateEvent(ret.data(), data);
    }

    auto e = new Event(data);

    e->d_ptr->m_pAccount = m_pAccount;

    if (isNew)
        q_ptr->editor<Event>()->addNew(e);
    else
        q_ptr->editor<Event>()->addExisting(e);

    return e;
}

QSharedPointer<Event> Calendar::addEvent(const EventPrivate& data)
{
    auto e = d_ptr->getEvent(data, true);

    return e->d_ptr->m_pStrongRef;
}
