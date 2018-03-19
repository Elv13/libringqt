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
    Account* m_pAccount {nullptr};
    CalendarEditor* m_pEditor {nullptr};
};

Calendar::Calendar(CollectionMediator<Event>* mediator, Account* a) : CollectionInterface(new CalendarEditor(mediator)),
d_ptr(new CalendarPrivate)
{
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

    auto eventAdapter = std::shared_ptr<VObjectAdapter<Event::EventBean>>(
        new  VObjectAdapter<Event::EventBean>
    );

    static const QHash<QByteArray, Event::EventCategory> m_hCategoriesMapper {
        {"PHONE_CALL", Event::EventCategory::CALL},
    };

    /*
    UID:1507286415-0-294D8D29@ring.cx
    DTSTART;TZID=America/Toronto:1507286415
    DTEND;TZID=America/Toronto:1507286415
    DTSTAMP;TZID=America/Toronto:1521092836
    X_RING_DIRECTION;VALUE=STRING:OUTGOING
    ORGANIZER;CN="elv13withappimage3";X_RING_ACCOUNTID=545895f087fbd23b:ring:c569cd72626800fec17a14ca5d962fcd59f4af8f@bootstrap.ring.cx
    ATTENDEE;CN="4444444444444444444444444444444444444444":ring:4444444444444444444444444444444444444444
    */

    eventAdapter->addPropertyHandler("DTSTART", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        self->startTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("DTEND", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        self->stopTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("DTSTAMP", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        self->revTimeStamp = QString(value.data()).toInt();
    });

    eventAdapter->addPropertyHandler("CATEGORIES", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        const QByteArray val = QByteArray::fromRawData(value.data(), value.size()+1);

        self->category = m_hCategoriesMapper.value(val);
    });

    eventAdapter->addPropertyHandler("X_RING_DIRECTION", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        self->direction = value == "OUTGOING" ?
            Event::Direction::OUTGOING : Event::Direction::INCOMING;
    });

    eventAdapter->addPropertyHandler("UID", [](Event::EventBean* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        self->uid = value.data();
    });

    calendarAdapter->setObjectFactory([this](const std::basic_string<char>& object_type) -> Calendar* {
        return this;
    });

    Event::EventBean e;

    eventAdapter->setObjectFactory([this, &e](const std::basic_string<char>& object_type) -> Event::EventBean* {
        e = {};
        return &e;
    });

    calendarAdapter->setFallbackObjectHandler<Event::EventBean>(
        [this](
           Calendar* self,
           Event::EventBean* child,
           const std::basic_string<char>& name
        ) {
            editor<Event>()->addExisting(new Event(*child));
    });


    l.registerVObjectAdaptor("VCALENDAR", calendarAdapter);
    l.registerVObjectAdaptor("VEVENT"   , eventAdapter   );

    l.loadFile(path().toLatin1().data());

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
    qDebug() << "ADD EXISTING!" << item;
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

Event* Calendar::eventAt(int position) const
{
    if (position < 0 || position >= d_ptr->m_pEditor->m_lItems.size())
        return nullptr;

    return d_ptr->m_pEditor->m_lItems[position];
}

Account* Calendar::account() const
{
    return d_ptr->m_pAccount;
}

QSharedPointer<Event> Calendar::addFromCall(Call* c)
{
    if (!c)
        return nullptr;

    if (c->lifeCycleState() != Call::LifeCycleState::FINISHED)
        return nullptr;

    if (c->calendarEvent()) {
        Q_ASSERT(false);
        return c->calendarEvent();
    }

    Event::EventBean b;
    b.startTimeStamp = c->startTimeStamp();
    b.stopTimeStamp  = c->stopTimeStamp ();
    b.direction      = c->direction() == Call::Direction::OUTGOING ?
        Event::Direction::OUTGOING : Event::Direction::INCOMING;

    auto e = new Event(b, c);

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

    return QSharedPointer<Event>(e);
}
