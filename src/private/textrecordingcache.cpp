/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "textrecordingcache.h"

// Qt
#include <QtCore/QCryptographicHash>
#include <QtCore/QMimeDatabase>
#include <QtCore/QUrl>

// Ring
#include "contactmethod.h"
#include "account.h"
#include "person.h"
#include "personmodel.h"
#include "accountmodel.h"
#include "eventmodel.h"
#include "libcard/private/event_p.h"
#include "libcard/calendar.h"
#include "media/file.h"
#include "availableaccountmodel.h"
#include "localtextrecordingcollection.h"
#include "phonedirectorymodel.h"

QHash<QByteArray, QWeakPointer<Serializable::Peers>> SerializableEntityManager::m_hPeers;

void Serializable::Peers::addPeer(ContactMethod* cm)
{
    peers.insert(cm);
}

QSharedPointer<Serializable::Peers> Serializable::Peers::join(ContactMethod* cm)
{
    QSet<ContactMethod*> cms = peers;

    // Prevent duplicates
    for (auto cm2 : qAsConst(cms)) {
        if (cm2->d() == cm->d())
            return SerializableEntityManager::peers(cms);
    }

    peers.insert(cm);

    return SerializableEntityManager::peers(cms);
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::peer(ContactMethod* cm)
{
    if (!cm)
        return nullptr;

    const QByteArray sha1 = cm->sha1();
    QSharedPointer<Serializable::Peers> p = m_hPeers.value(sha1);

    if (!p) {
        p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
        p->sha1s << sha1;

        p->addPeer(cm);

        m_hPeers[sha1] = p;
    }

    return p;
}

static QByteArray mashSha1s(const QList<QString>& sha1s)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);

    QByteArray ps;

    for (const QString& sha1 : sha1s) {
        ps += sha1.toLatin1();
    }

    hash.addData(ps);

    //Create a reproducible key for this file
    return hash.result().toHex();
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::peers(const QSet<ContactMethod*>& cms)
{
    QList<QString> sha1s;

    for(const ContactMethod* cm : qAsConst(cms)) {
        sha1s << cm->sha1();
    }

    const QByteArray sha1 = ::mashSha1s(sha1s);

    QSharedPointer<Serializable::Peers> p = m_hPeers.value(sha1);

    if (!p) {
        p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
        p->sha1s = sha1s;
        p->peers = cms;
        m_hPeers[sha1] = p;
    }

    Q_ASSERT(cms.size() == p->peers.size());

    return p;
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::fromSha1(const QByteArray& sha1)
{
    return m_hPeers.value(sha1);
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::fromJson(const QJsonObject& json, const QString& path, ContactMethod* cm)
{
    //Check if the object is already loaded
    QStringList sha1List;
    QJsonArray as = json[QStringLiteral("sha1s")].toArray();
    for (int i = 0; i < as.size(); ++i) {
        sha1List.append(as[i].toString());
    }

    if (sha1List.isEmpty())
        return {};

    QByteArray sha1 = sha1List[0].toLatin1();

    if (sha1List.size() > 1) {
        sha1 = mashSha1s(sha1List);
    }

    if (m_hPeers.contains(sha1))
        return m_hPeers[sha1];

    //Load from json
    QSharedPointer<Serializable::Peers> p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
    p->read(json, path);
    m_hPeers[sha1] = p;

    //TODO Remove in 2016
    //Some older versions of the file don't store necessary values, fix that
    if (cm && p->peers.isEmpty())
        p->addPeer(cm);

    return p;
}


bool Serializable::Group::warnOfRaceCondition = false;

Serializable::Group::Group(Account* a, const QString& path) : m_pAccount(a), m_Path(path)
{
    Q_ASSERT(!warnOfRaceCondition);
}

Serializable::Group::~Group()
{
    for (auto m : qAsConst(messages))
        delete m.first;
}

const QList< QPair<Media::MimeMessage*, ContactMethod*> >& Serializable::Group::messagesRef() const
{
    return messages;
}

int Serializable::Group::size() const
{
    return messages.size();
}

void Serializable::Group::addMessage(Media::MimeMessage* m, ContactMethod* peer)
{
    // Brute force back the timestamp. While it wastes a few CPU cycles,
    // it avoids having to keep track of it internally.
    auto newBeg = (!begin) ? m->timestamp() : std::min(
        m->timestamp(), begin
    );

    // This should really not happen, ever. If it did, there's either a new
    // race condition that reads the event while it's being created or something
    // went wrong handling time_t. Some bad timezone handling could also set
    // this on fire. However that's a bug elsewhere in the code and should be
    // fixed there.
    if (newBeg != begin && m_pEvent)
        qWarning() << "Trying to modify immutable event variables, this is a bug";

    begin = newBeg;

    end = std::max(
        m->timestamp(), end
    );

    if (m->direction() == Media::Media::Direction::IN)
        m_IncomingCount++;
    else
        m_OutgoingCount++;

    addPeer(peer);

    messages.append({m, peer});
}

void Serializable::Group::reloadAttendees() const
{
    if (m_pParent && m_pEvent) {
        m_pEvent->d_ptr->m_lAttendees.clear();

        for (auto peer : qAsConst(m_pParent->peers)) {
             m_pEvent->d_ptr->m_lAttendees << QPair<ContactMethod*, QString> {
                 peer, ""
             };
        }
    }
}

void Serializable::Group::addPeer(ContactMethod* cm)
{
    if (!cm)
        return;

    if (!m_pParent) {
        if ((m_pParent = SerializableEntityManager::peer(cm))) {
            reloadAttendees();
        }

        return;
    }

    if (m_pParent->peers.contains(cm))
        return;


    m_pParent = m_pParent->join(cm);

    reloadAttendees();
}

void Serializable::Group::read (const QJsonObject &json, const QHash<QString,ContactMethod*> sha1s, const QString& path)
{
    id            = json[QStringLiteral("id")           ].toInt   ();
    nextGroupSha1 = json[QStringLiteral("nextGroupSha1")].toString();
    nextGroupId   = json[QStringLiteral("nextGroupId")  ].toInt   ();
    eventUid      = json[QStringLiteral("eventUid")     ].toString().toLatin1();
    type          = static_cast<Media::MimeMessage::Type>(json[QStringLiteral("type")].toInt());
    m_Path        = path;

    QJsonArray a = json[QStringLiteral("messages")].toArray();
    for (int i = 0; i < a.size(); ++i) {
        QJsonObject o = a[i].toObject();

        if (o.isEmpty())
            continue;

        ContactMethod* cm = nullptr;

        // The ContactMethod isn't part of the MimeMessage, but a group metadata
        if (o.contains(QStringLiteral("authorSha1"))) {
            const QString sha1 = o[QStringLiteral("authorSha1")].toString();
            cm = sha1s[sha1];
        }

        addMessage(Media::MimeMessage::buildExisting(o), cm);
    }
}

void Serializable::Group::write(QJsonObject &json, const QString& path) const
{
    // This is really not supposed to happen anymore
    if (!m_pEvent)
        qWarning() << "Trying to save a text message group without an event" << eventUid;

    Q_ASSERT(eventUid == event()->uid());

    if (!event()->hasAttachment(path)) {
        static QMimeType* t = nullptr;
        if (!t) {
            QMimeDatabase db;
            t = new QMimeType(db.mimeTypeForFile("foo.json"));
        }

        event()->attachFile(new Media::File(
            path, Media::Attachment::BuiltInTypes::TEXT_RECORDING, t
        ));
    }

    json[QStringLiteral("id")            ] = id                     ;
    json[QStringLiteral("nextGroupSha1") ] = nextGroupSha1          ;
    json[QStringLiteral("nextGroupId")   ] = nextGroupId            ;
    json[QStringLiteral("type")          ] = static_cast<int>(type) ;

    // Deleting messages have strange ways to fail, this mitigates one of the
    // two main race
    if (event()->syncState() == Event::SyncState::PLACEHOLDER) {
        qWarning() << "An event cannot be saved if it doesn't exist";
    }
    else {
        Q_ASSERT(event()->collection());
        json[QStringLiteral("eventUid")] = QString(event()->uid());
    }

    QJsonArray a;
    for (const auto& m : qAsConst(messages)) {
        QJsonObject o;
        m.first->write(o);

        if (o.isEmpty())
            continue;

        if (m.second)
            o[QStringLiteral("authorSha1")] = QString(m.second->sha1());

        a.append(o);
    }
    json[QStringLiteral("messages")] = a;
}

Serializable::Peers::~Peers()
{
    for (auto g : qAsConst(groups))
        delete g;
}

void Serializable::Peers::read(const QJsonObject &json, const QString& path)
{
    QJsonArray as = json[QStringLiteral("sha1s")].toArray();
    for (int i = 0; i < as.size(); ++i) {
        sha1s.append(as[i].toString());
    }

    Account* a = nullptr;

    QSet<ContactMethod*> fallback;

    // Note that it is **VERY** important to do this before the groups
    QJsonArray a2 = json[QStringLiteral("peers")].toArray();
    for (int i = 0; i < a2.size(); ++i) {
        QJsonObject o = a2[i].toObject();

        // It should never happen, but there is some corner case where it might
        // anyway, like when some data is deleted, but not some other and the
        // file is resaved. Tracking the problem source is not worth it as the
        // data will be worthless.
        if (o[QStringLiteral("uri")].toString().isEmpty()) {
            qWarning() << "Corrupted chat history entry: Missing URI" << o;
            continue;
        }

        auto cm = PhoneDirectoryModel::instance().fromJson(o);

        m_hSha1[cm->sha1()] = cm;
        peers.insert(cm);

        fallback.insert(cm);

        if ((!a) && cm)
            a = cm->account();
    }

    // It can happen if the accounts were deleted
    if (!a) {
        // Too noisy, it happens with deleted accounts because once the file
        // is saved again, the accountId is no longer there.
        //qWarning() << "Could not find a viable account for existing chat conversation";
        a = AvailableAccountModel::instance().currentDefaultAccount();
    }

    // Getting worst, pick something at "random" (the account order is not really random)
    if ((!a) && AccountModel::instance().size()) {
        a = AccountModel::instance()[0];
    }
    else if (!a) {
        qWarning() << "There is no accounts for existing assets, this wont end well";
    }

    QJsonArray arr = json[QStringLiteral("groups")].toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();

        Group* group = new Group(a, path);
        group->read(o, m_hSha1, path);

        // Work around a bug in older chat conversation where whe authorSha1
        // wasn't set.
        if (!group->m_pParent) {
            group->m_pParent = SerializableEntityManager::peers(fallback);

            Q_ASSERT(fallback.size() == group->m_pParent->peers.size());
            group->reloadAttendees();
        }

        if (!group->size())
            delete group;
        else {
            groups.append(group);
            Q_ASSERT(group->timeRange().first);
        }
    }
}

QJsonArray Serializable::Peers::toSha1Array() const
{
    QJsonArray a2;
    for (const QString& sha1 : qAsConst(sha1s)) {
        a2.append(sha1);
    }

    return a2;
}

void Serializable::Peers::write(QJsonObject &json) const
{

    QJsonArray a;

    const auto sha1s = toSha1Array();

    for (const Group* g : groups) {
        QJsonObject o;
        g->write(o, LocalTextRecordingCollection::directoryPath()+sha1s.first().toString()+".json");

        if (o.isEmpty())
            continue;

        a.append(o);
    }

    if (a.isEmpty())
        return;

    json[QStringLiteral("sha1s")] = sha1s;
    json[QStringLiteral("groups")] = a;

    QJsonArray a3;
    for (const ContactMethod* cm : peers) {
        a3.append(cm->toJson());
    }

    json[QStringLiteral("peers")] = a3;
}

Event* Serializable::Group::buildEvent()
{
    //
}

bool Serializable::Group::hasEvent() const
{
    return m_pEvent && m_pEvent->syncState() != Event::SyncState::PLACEHOLDER;
}

QPair<time_t, time_t> Serializable::Group::timeRange() const
{
//     end = 0;
//     begin = std::numeric_limits<time_t>::max();
//
//     //TODO add something to detect if the cache are still valid
//     for (const auto& p : qAsConst(messages)) {
//         begin = std::min(p.first()->timestamp(), begin);
//     }

    return {begin, end};
}

/**
 * It is important to keep in mind the events are owned by the calendars, not
 * the text recordings or its internal data structures.
 */
QSharedPointer<Event> Serializable::Group::event(bool allowPlaceholders) const
{
    if (m_pEvent)
        return m_pEvent;

    // In the future, this should work most of the time. However existing data
    // and some obscure core paths may still handle unregistered events
    if ((m_pEvent = EventModel::instance().getById(eventUid, allowPlaceholders))) {
        reloadAttendees();
        return m_pEvent;
    }

    // There is a bunch of reason why this can happen, most of them are races.
    // A valid use case would be to cleanup a messy history or someone who
    // import backups from another device.
    if (Q_UNLIKELY(!eventUid.isEmpty()))
        qWarning() << "Trying to import an event that seems to already exists, but was not found" << eventUid;

    // Try to fix the errors of the past and fix a suitable account.
    if (!m_pAccount)
        m_pAccount = AvailableAccountModel::instance().currentDefaultAccount();

    // This is very, very bad
    if (!m_pAccount && AccountModel::instance().size())
        m_pAccount = AccountModel::instance()[0];

    // There is nothing to do, it will probably crash soon in a way or another
    if (!m_pAccount) {
        qWarning() << "Deleting all accounts is not supported and will cause crashes. Please create an account";
        return nullptr;
    }

    // Build an event;
    EventPrivate ev;

    Q_ASSERT(!m_Path.isEmpty());

    static QMimeType* t = nullptr;
    if (!t) {
        QMimeDatabase db;
        t = new QMimeType(db.mimeTypeForFile("foo.json"));
    }

    auto path = new Media::File(
        m_Path, Media::Attachment::BuiltInTypes::TEXT_RECORDING, t
    );

    ev.m_EventCategory  = Event::EventCategory::MESSAGE_GROUP;
    ev.m_Status         = Event::Status::FINAL;
    ev.m_StartTimeStamp = begin;
    ev.m_StopTimeStamp  = end;
    ev.m_RevTimeStamp   = end;

    if (m_pParent) {
        for (auto peer : qAsConst(m_pParent->peers)) {
             ev.m_lAttendees << QPair<ContactMethod*, QString> {
                 peer, ""
             };
        }
    }

    m_pEvent = m_pAccount->calendar()->addEvent(ev);

    m_pEvent->attachFile(path);

    Q_ASSERT(!path->path().isEmpty());

    const_cast<Group*>(this)->eventUid = m_pEvent->uid();


//     Q_ASSERT((!m_pParent) || !m_pEvent->d_ptr->m_lAttendees.size() == m_pParent->peers.size());

    return m_pEvent;
}

