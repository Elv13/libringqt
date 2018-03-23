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
#pragma once

// Qt
#include <QtCore/QString>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSharedPointer>

// Ring
class ContactMethod;
class Event;
class Account;
#include <media/mimemessage.h>

/*
 * This set of classes help manage a JSON database used to index all known text
 * recording files.
 */

class SerializableEntityManager;

//BEGIN Those classes are serializable to JSon
/**
 * Those classes map 1:1 to the json stored on the disk. References are then
 * extracted, the conversation reconstructed and placed into a TextMessageNode
 * vector.
 */
namespace Serializable {

class Group;
class Peers;

class Group {
public:
    Group(Account* a) : m_pAccount(a) {}
    ~Group();

    /**Also link upward to avoid duplicating the peers metadata in each group
     *
     * This is intended as a copy-on-write reference, so not modify the
     * original.
     */
    QSharedPointer<Peers> m_pParent {nullptr};

    ///The group ID (necessary to untangle the graph
    int id;
    ///If the conversion add new participants, a new file will be created
    QString nextGroupSha1;
    ///The group type
    MimeMessage::Type type {MimeMessage::Type::CHAT};
    ///This is the group identifier in the file described by `nextGroupSha1`
    int nextGroupId;
    ///The unique identifier of the event associated with this text message group
    QByteArray eventUid;
    ///The timestamp of the first group entry
    time_t begin {0};
    ///The timestamp of the last group entry
    time_t end   {0};

    /// The account that owns this group, ignore the possible cardinality issues
    Account* m_pAccount {nullptr};

    QSharedPointer<Event> event();
    void addMessage(MimeMessage* m, ContactMethod* peer);

    /// Prevent the list from being modified directly.
    const QList<QPair<MimeMessage*, ContactMethod*> >& messagesRef() const;

    int size() const;

    /// Create an event;
    Event* buildEvent();

    /// Keep the peers in sync
    void addPeer(ContactMethod* cm);

    /// Keep the event in sync
    void reloadAttendees();

    void read (const QJsonObject &json, const QHash<QString,ContactMethod*> sha1s);
    void write(QJsonObject       &json) const;

private:
    ///All messages from this chunk (the ContactMethod is the author of incoming messages)
    QList< QPair<MimeMessage*, ContactMethod*> > messages;

    ///Due to complex ownership, give no direct access
    QSharedPointer<Event> m_pEvent;
};

class Peers {
    friend class ::SerializableEntityManager;
public:
    ~Peers();

    ///The sha1(s) of each participants. If there is onlt one, it should match the filename
    QList<QString> sha1s;
    ///Every message groups associated with this ContactMethod (or ContactMethodGroup)
    QList<Group*> groups;
    ///Information about every (non self) peer involved in this group
    QSet<ContactMethod*> peers;

    ///This attribute store if the file has changed
    bool hasChanged;

    ///Keep a cache of the peers sha1
    QHash<QString,ContactMethod*> m_hSha1;

    void read (const QJsonObject &json);
    void write(QJsonObject       &json) const;

    QJsonArray toSha1Array() const;

    void addPeer(ContactMethod* cm);

    /**
     * Return a new set with both the current CMs and the new one;
     */
    QSharedPointer<Peers> join(ContactMethod* cm);

private:
    Peers() : hasChanged(false) {}
};

}
//END Those classes are serializable to JSon

/**
 * This class ensure that only one Serializable::Peer structure exist for each
 * peer [group].
 */
class SerializableEntityManager
{
public:
    static QSharedPointer<Serializable::Peers> peer(ContactMethod* cm);
    static QSharedPointer<Serializable::Peers> peers(const QSet<ContactMethod*>& cms);
    static QSharedPointer<Serializable::Peers> fromSha1(const QByteArray& sha1);
    static QSharedPointer<Serializable::Peers> fromJson(const QJsonObject& obj, ContactMethod* cm = nullptr);
private:
    static QHash<QByteArray, QWeakPointer<Serializable::Peers>> m_hPeers;
};
