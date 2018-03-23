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

// Ring
#include "contactmethod.h"
#include "account.h"
#include "person.h"
#include "personmodel.h"
#include "accountmodel.h"
#include "eventmodel.h"
#include "libcard/private/event_p.h"
#include "libcard/calendar.h"
#include "availableaccountmodel.h"
#include "phonedirectorymodel.h"

QHash<QByteArray, QWeakPointer<Serializable::Peers>> SerializableEntityManager::m_hPeers;

void Serializable::Peers::addPeer(const ContactMethod* cm)
{
    Serializable::Peer* peer = new Serializable::Peer();
    peer->sha1      = cm->sha1();
    peer->uri       = cm->uri();
    peer->accountId = cm->account() ? cm->account()->id () : QString();
    peer->personUID = cm->contact() ? cm->contact()->uid() : QString();
    peers << peer;
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::peer(const ContactMethod* cm)
{
    const QByteArray sha1 = cm->sha1();
    QSharedPointer<Serializable::Peers> p = m_hPeers[sha1];

    if (!p) {
        p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
        p->sha1s << sha1;

        p->addPeer(cm);

        m_hPeers[sha1] = p;
    }

    return p;
}

static QByteArray mashSha1s(QList<QString> sha1s)
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

QSharedPointer<Serializable::Peers> SerializableEntityManager::peers(QList<const ContactMethod*> cms)
{
    QList<QString> sha1s;

    for(const ContactMethod* cm : cms) {
        const QByteArray sha1 = cm->sha1();
        sha1s << sha1;
    }

    const QByteArray sha1 = ::mashSha1s(sha1s);

    QSharedPointer<Serializable::Peers> p = m_hPeers[sha1];

    if (!p) {
        p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
        p->sha1s = sha1s;
        m_hPeers[sha1] = p;
    }

    return p;
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::fromSha1(const QByteArray& sha1)
{
    return m_hPeers[sha1];
}

QSharedPointer<Serializable::Peers> SerializableEntityManager::fromJson(const QJsonObject& json, const ContactMethod* cm)
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

    if (m_hPeers[sha1])
        return m_hPeers[sha1];

    //Load from json
    QSharedPointer<Serializable::Peers> p = QSharedPointer<Serializable::Peers>(new Serializable::Peers());
    p->read(json);
    m_hPeers[sha1] = p;

    //TODO Remove in 2016
    //Some older versions of the file don't store necessary values, fix that
    if (cm && p->peers.isEmpty())
        p->addPeer(cm);

    return p;
}


Serializable::Group::~Group()
{
    for (auto m : qAsConst(messages))
        delete m;
}

const QList<MimeMessage*>& Serializable::Group::messagesRef() const
{
    return messages;
}

int Serializable::Group::size() const
{
    return messages.size();
}

void Serializable::Group::addMessage(MimeMessage* m)
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

    messages.append(m);
}

void Serializable::Group::read (const QJsonObject &json, const QHash<QString,ContactMethod*> sha1s)
{
    Q_UNUSED(sha1s)
    id            = json[QStringLiteral("id")           ].toInt   ();
    nextGroupSha1 = json[QStringLiteral("nextGroupSha1")].toString();
    nextGroupId   = json[QStringLiteral("nextGroupId")  ].toInt   ();
    eventUid      = json[QStringLiteral("eventUid")     ].toString().toLatin1();
    type          = static_cast<MimeMessage::Type>(json[QStringLiteral("type")].toInt());

    QJsonArray a = json[QStringLiteral("messages")].toArray();
    for (int i = 0; i < a.size(); ++i) {
        QJsonObject o = a[i].toObject();
        addMessage(MimeMessage::buildExisting(o));
    }
}

void Serializable::Group::write(QJsonObject &json) const
{
    json[QStringLiteral("id")            ] = id                    ;
    json[QStringLiteral("nextGroupSha1") ] = nextGroupSha1         ;
    json[QStringLiteral("nextGroupId")   ] = nextGroupId           ;
    json[QStringLiteral("type")          ] = static_cast<int>(type);
    json[QStringLiteral("eventUid")      ] = QString(eventUid)     ;

    QJsonArray a;
    for (const MimeMessage* m : messages) {
        QJsonObject o;
        m->write(o);
        a.append(o);
    }
    json[QStringLiteral("messages")] = a;
}

void Serializable::Peer::read (const QJsonObject &json)
{
    accountId = json[QStringLiteral("accountId")].toString();
    uri       = json[QStringLiteral("uri")      ].toString();
    personUID = json[QStringLiteral("personUID")].toString();
    sha1      = json[QStringLiteral("sha1")     ].toString();

    Account* a     = AccountModel::instance().getById(accountId.toLatin1());
    Person* person = personUID.isEmpty() ?
        nullptr : PersonModel::instance().getPersonByUid(personUID.toLatin1());

    m_pContactMethod = PhoneDirectoryModel::instance().getNumber(uri,person,a);
}

void Serializable::Peer::write(QJsonObject &json) const
{
    json[QStringLiteral("accountId")] = accountId ;
    json[QStringLiteral("uri")      ] = uri       ;
    json[QStringLiteral("personUID")] = personUID ;
    json[QStringLiteral("sha1")     ] = sha1      ;
}

Serializable::Peers::~Peers()
{
    for (auto g : qAsConst(groups))
        delete g;

    for (auto p : qAsConst(peers))
        delete p;
}

void Serializable::Peers::read (const QJsonObject &json)
{
    QJsonArray as = json[QStringLiteral("sha1s")].toArray();
    for (int i = 0; i < as.size(); ++i) {
        sha1s.append(as[i].toString());
    }

    Account* a = nullptr;

    QJsonArray a2 = json[QStringLiteral("peers")].toArray();
    for (int i = 0; i < a2.size(); ++i) {
        QJsonObject o = a2[i].toObject();

        // It should never happen, but there is some corner case where it might
        // anyway, like when some data is deleted, but not some other and the
        // file is resaved. Tracking the problem source is not worth it as the
        // data will be worthless.
        if (o[QStringLiteral("uri")].toString().isEmpty()) {
            qWarning() << "Corrupted chat history entry: Missing URI";
            continue;
        }

        Peer* peer = new Peer();
        peer->read(o);
        m_hSha1[peer->sha1] = peer->m_pContactMethod;
        peers.append(peer);

        if (!a && peer->m_pContactMethod)
            a = peer->m_pContactMethod->account();
    }

    // It can happen if the accounts were deleted
    if (!a) {
        qWarning() << "Could not find a viable account for existing chat conversation";
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
        Group* group = new Group(a);
        group->read(o,m_hSha1);
        groups.append(group);
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
    json[QStringLiteral("sha1s")] = toSha1Array();

    QJsonArray a;
    for (const Group* g : groups) {
        QJsonObject o;
        g->write(o);
        a.append(o);
    }
    json[QStringLiteral("groups")] = a;

    QJsonArray a3;
    for (const Peer* p : peers) {
        QJsonObject o;
        p->write(o);
        a3.append(o);
    }
    json[QStringLiteral("peers")] = a3;
}

Event* Serializable::Group::buildEvent()
{
    //
}

/**
 * It is important to keep in mind the events are owned by the calendars, not
 * the text recordings or its internal data structures.
 */
QSharedPointer<Event> Serializable::Group::event()
{
    if (m_pEvent)
        return m_pEvent;

    // In the future, this should work most of the time. However existing data
    // and some obscure core paths may still handle unregistered events
    auto e = EventModel::instance().getById(eventUid);

    if (e)
        return e;

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

    ev.m_EventCategory  = Event::EventCategory::MESSAGE_GROUP;
    ev.m_StartTimeStamp = begin;
    ev.m_StopTimeStamp  = end;
    ev.m_RevTimeStamp   = end;

    e = m_pAccount->calendar()->addEvent(ev);

    return e;
}

