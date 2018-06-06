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
#include <QtCore/QTimer>
#include <QtCore/QMutex>

// Ring
#include "event.h"
#include <call.h>
#include <account.h>
#include <accountmodel.h>
#include <personmodel.h>
#include <eventmodel.h>
#include <phonedirectorymodel.h>
#include <localrecordingcollection.h>
#include <media/avrecording.h>
#include "../private/call_p.h"
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
   CalendarPrivate* d_ptr;

private:
   virtual QVector<Event*> items() const override;


};

class CalendarPrivate final : public QObject
{
    Q_OBJECT
public:
    // Attributes
    Account* m_pAccount {nullptr};
    CalendarEditor* m_pEditor {nullptr};
    QMutex m_Mutex;
    bool m_HasDelayedSave {false};
    QSet<Event*> m_lUnsavedEvent;
    bool m_IsLoaded {false};

    Calendar* q_ptr;

    // Heuristics used to decide when it's time to rebuild the on-disk database
    // for performance reason.
    struct {
        uint m_Duplicates {0}; /*!< Previous elements revised in append-only mode          */
        uint m_Unsorted   {0}; /*!< Elements where the stopTimeStamp isn't ordered         */
        uint m_Removed    {0}; /*!< Elements that were discarded but are still in the file */
    } m_GCHeuristics;

    // Helpers
    Event* getEvent(const EventPrivate& data, Event::SyncState st);
    Event* updateEvent(Event* e, const EventPrivate& data);

public Q_SLOTS:
    void slotEventStateChanged(Event::SyncState state, Event::SyncState old);
    void slotSaveOnDisk();
};

Calendar::Calendar(CollectionMediator<Event>* mediator, Account* a) : QObject(nullptr), CollectionInterface(new CalendarEditor(mediator)),
d_ptr(new CalendarPrivate)
{
    d_ptr->q_ptr      = this;
    d_ptr->m_pAccount = a;
    d_ptr->m_pEditor  = static_cast<CalendarEditor*>(editor<Event>());
    d_ptr->m_pEditor->m_pCal = this;
    d_ptr->m_pEditor->d_ptr = d_ptr;
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
            QByteArray pVal = QByteArray::fromRawData(param.second.data(), param.second.size());

            //WARNING Always detach the value before sending into other classes
            // unless it is not stored

            if (pKey == "CN") {
                pVal.detach();
                self->m_CN = pVal;
            }
            else if (pKey == "UID") {
                pVal.detach();
                p = PersonModel::instance().getPlaceHolder(pVal);
            }
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

    // Import the autio recordings
    eventAdapter->addPropertyHandler("ATTACH", []ARGS {
        if (self->m_EventCategory == Event::EventCategory::CALL) {
            Media::AVRecording* rec = nullptr;
            for (auto param : params) {
                if (param.first == "FMTTYPE" && param.second == "audio/x-wav") {
                    rec = LocalRecordingCollection::instance().addFromPath(
                        value.data()
                    );

                    self->m_lAttachedFiles << rec;

                    Q_ASSERT(rec->type() == Media::Attachment::BuiltInTypes::AUDIO_RECORDING);
                }
            }
        }
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

    // Do not add the events yet, batch those insertion once the newest event
    // is known to avoid triggering thousand of peers timeline updates.
    QList<Event*> events;

    calendarAdapter->setFallbackObjectHandler<EventPrivate>(
        [this, &events](
           Calendar* self,
           EventPrivate* child,
           const std::basic_string<char>& name
        ) {
            events << d_ptr->getEvent(*child, Event::SyncState::SAVED);
    });

    l.registerVObjectAdaptor("VCALENDAR", calendarAdapter);
    l.registerVObjectAdaptor("VEVENT"   , eventAdapter   );
    l.registerVObjectAdaptor("VJOURNAL" , eventAdapter   );
    l.registerVObjectAdaptor("VTODO"    , eventAdapter   );
    l.registerVObjectAdaptor("VALARM"   , eventAdapter   );

    l.loadFile(path().toLatin1().data());

#undef ARGS

    //TODO add batching to the collection system

    // Read backward to improve the odds of the newest being added first
    while (!events.isEmpty()) {
        auto e = events.takeLast();
        if (!e->collection()) {
            if (e->syncState() == Event::SyncState::NEW)
                editor<Event>()->addNew(e);
            else
                editor<Event>()->addExisting(e);
        }
    }

    d_ptr->m_IsLoaded = true;
    emit loadingFinished();

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
    if (item->isSaved())
        return false;

    static QString dir = (QStandardPaths::writableLocation(QStandardPaths::DataLocation)) + "/iCal/";
    if (!QDir().mkpath(dir)) {
        qWarning() << "cannot create path for the calendar: " << dir;
        return false;
    }
// Q_ASSERT(false);
    {
        QMutexLocker l(&d_ptr->m_Mutex);
        d_ptr->m_lUnsavedEvent << const_cast<Event*>(item);
        if (!d_ptr->m_HasDelayedSave)
            QTimer::singleShot(0, d_ptr, &CalendarPrivate::slotSaveOnDisk);

        d_ptr->m_HasDelayedSave = true;
    }

    return true;
}

bool CalendarEditor::remove( const Event* item )
{
    if (item->syncState() == Event::SyncState::CANCELLED ||
      item->syncState() == Event::SyncState::DISCARDED)
        return true;

    d_ptr->m_GCHeuristics.m_Removed++;

    time_t curTime;
    ::time(&curTime);
    const_cast<Event*>(item)->d_ptr->m_RevTimeStamp = curTime;

    {
        QMutexLocker l(&d_ptr->m_Mutex);
        d_ptr->m_lUnsavedEvent << const_cast<Event*>(item);
        if (!d_ptr->m_HasDelayedSave)
            QTimer::singleShot(0, d_ptr, &CalendarPrivate::slotSaveOnDisk);

        d_ptr->m_HasDelayedSave = true;
    }

    return true;
}

bool CalendarEditor::edit( Event* item )
{
    return true;
}

bool CalendarEditor::addNew( Event* item )
{
    return addExisting(item) && save(item);
}

bool CalendarEditor::addExisting( const Event* item )
{
    Q_ASSERT(!item->collection());
    const_cast<Event*>(item)->setCollection(m_pCal);
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

bool Calendar::isLoaded() const
{
    return d_ptr->m_IsLoaded;
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

static void updateEvent(Event* e, Call* c)
{
    //bool updated = false;

    if (c->stopTimeStamp() > e->stopTimeStamp()) {
        e->setStopTimeStamp(c->stopTimeStamp());
        //updated = true;
    }

    //TODO

}

QSharedPointer<Event> Calendar::addEvent(Call* c)
{
    if (!c)
        return nullptr;

    if (c->lifeCycleState() != Call::LifeCycleState::FINISHED)
        return nullptr;

    if (auto e = c->calendarEvent()) {
        updateEvent(e.data(), c);
        return e;
    }

    EventPrivate b;
    b.m_StartTimeStamp  = c->startTimeStamp();
    b.m_StopTimeStamp   = c->stopTimeStamp ();
    b.m_Status          = Event::Status::FINAL;
    b.m_HasImportedCall = true;
    b.m_Status          = c->isMissed() ?
        Event::Status::X_MISSED : Event::Status::FINAL;
    b.m_Direction       = c->direction() == Call::Direction::OUTGOING ?
        Event::Direction::OUTGOING : Event::Direction::INCOMING;

    auto e = new Event(b, Event::SyncState::NEW);

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

    c->d_ptr->m_LegacyFields.m_pEvent = e->d_ptr->m_pStrongRef;

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
    Q_ASSERT(!data.m_UID.isEmpty());

    // Turn the placeholder into the "real" event
    if (e->syncState() == Event::SyncState::PLACEHOLDER) {
        e->rebuild(data, Event::SyncState::SAVED);
        e->d_ptr->m_pAccount = m_pAccount;
        e->setCollection(q_ptr);
        return e;
    }

    if (e->revTimeStamp() < data.m_RevTimeStamp) {
        qWarning() << "Attempting to update an event from the future, ignoring" << e;
        return e;
    }

    if (data.m_StopTimeStamp > e->stopTimeStamp()) {
        m_GCHeuristics.m_Unsorted++;
        e->d_ptr->m_StopTimeStamp = data.m_StopTimeStamp;
    }

    // Note that this never triggers a GC pass. Since updateEvent is called
    // during load time, it is improbable it will hit the threshold as the
    // previous time it was saved, the GC would have been executed automatically
    m_GCHeuristics.m_Duplicates++;

    return e;
}

/// Ensure no duplicates are created by accident
Event* CalendarPrivate::getEvent(const EventPrivate& data, Event::SyncState st)
{
    if (data.m_UID.size())

    if (auto ret = data.m_UID.isEmpty() ? nullptr : EventModel::instance().getById(data.m_UID)) {
        return updateEvent(ret.data(), data);
    }

    auto e = new Event(data, st);

    e->d_ptr->m_pAccount = m_pAccount;

    return e;
}

QSharedPointer<Event> Calendar::addEvent(const EventPrivate& data)
{
    auto e = d_ptr->getEvent(data, Event::SyncState::NEW)->d_ptr->m_pStrongRef;

    if (!e->collection()) {
        if (e->syncState() == Event::SyncState::NEW)
            editor<Event>()->addNew(e.data());
        else
            editor<Event>()->addExisting(e.data());
    }

    return e;
}

void CalendarPrivate::slotEventStateChanged(Event::SyncState state, Event::SyncState old)
{
    Q_UNUSED(old)

    switch(state) {
        case Event::SyncState::DISCARDED:
            m_GCHeuristics.m_Removed++;
            break;
        case Event::SyncState::MODIFIED:
            m_GCHeuristics.m_Duplicates++;
            break;
        case Event::SyncState::RESCHEDULED:
            m_GCHeuristics.m_Duplicates++;
            [[clang::fallthrough]];
        case Event::SyncState::IMPORTED:
            m_GCHeuristics.m_Unsorted++;
            break;
        case Event::SyncState::SAVED:
        case Event::SyncState::SYNCHRONIZED:
        case Event::SyncState::CANCELLED:
        case Event::SyncState::ERROR:
        case Event::SyncState::NEW:
        case Event::SyncState::PLACEHOLDER:
        case Event::SyncState::COUNT__:
            break;
    }

    // Decide if it's worth running the garbage collection.
    // The unsorted entries are worth more because they slow down startup
    // while the other 2 are mostly harmless beside more I/O.
    const int current = m_GCHeuristics.m_Removed
        + 3*m_GCHeuristics.m_Unsorted
        + 2*m_GCHeuristics.m_Duplicates;

    // A rather random value. Over time it can be modified to reflect reality
    constexpr static const int threshold = 200;

    if (current > threshold) {
        QMutexLocker l(&m_Mutex);
        if (!m_HasDelayedSave)
            QTimer::singleShot(0, this, &CalendarPrivate::slotSaveOnDisk);

        m_HasDelayedSave = true;
    }

}

void CalendarPrivate::slotSaveOnDisk()
{
    // Decide if it's worth running the garbage collection.
    // The unsorted entries are worth more because they slow down startup
    // while the other 2 are mostly harmless beside more I/O.
    const int current = m_GCHeuristics.m_Removed
        + 3*m_GCHeuristics.m_Unsorted
        + 2*m_GCHeuristics.m_Duplicates;

    // A rather random value. Over time it can be modified to reflect reality
    constexpr static const int threshold = 200;

    if (current > threshold) {
        qDebug() << "Compacting the event history" << q_ptr;
        ICSBuilder::rebuild(q_ptr);
    }
    else if (ICSBuilder::save(q_ptr)) {
        m_lUnsavedEvent.clear();
    }

    m_HasDelayedSave = false;
}

int Calendar::unsavedCount() const
{
    return d_ptr->m_lUnsavedEvent.size();
}

QList<QSharedPointer<Event> > Calendar::unsavedEvents() const
{
    QMutexLocker l(&d_ptr->m_Mutex);

    QList<QSharedPointer<Event> > ret;

    for (auto e : qAsConst(d_ptr->m_lUnsavedEvent))
        ret << e->d_ptr->m_pStrongRef;

    return ret;
}

#include <calendar.moc>
