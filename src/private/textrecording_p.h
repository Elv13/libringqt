/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
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

//Qt
#include <QtCore/QAbstractListModel>

//Daemon
#include <account_const.h>

//Ring
#include "media/media.h"
#include "media/textrecording.h"
#include "libcard/matrixutils.h"
#include "media/mimemessage.h"
#include "textrecordingcache.h"

struct TextMessageNode;
class InstantMessagingModel;
class ContactMethod;

namespace Media {
   class TextRecording;
}

namespace Media {

/**
 * The Media::Recording private class. This is where the reconstructed
 * conversation is stored. This class is also used as backend for the
 * IM Model. The messages themselves are added by the Media::Text.
 */
class TextRecordingPrivate : public QObject
{
    Q_OBJECT
public:
    explicit TextRecordingPrivate(TextRecording* r);

    //Attributes
    QAbstractItemModel*         m_pImModel           ;
    QVector<::TextMessageNode*> m_lNodes             ;
    Serializable::Group*        m_pCurrentGroup      ;
    QList<QSharedPointer<Serializable::Peers>> m_lAssociatedPeers;
    QHash<QString,bool>         m_hMimeTypes         ;
    int                         m_UnreadCount        ;
    QStringList                 m_lMimeTypes         ;
    QAbstractItemModel*         m_pTextMessagesModel {nullptr};
    QAbstractItemModel*         m_pUnreadTextMessagesModel {nullptr};
    QHash<uint64_t, TextMessageNode*> m_hPendingMessages;
    time_t                      m_LastUsed {0};

    //WARNING the order is in sync with both the daemon and the json files
    Matrix1D<MimeMessage::State, int> m_mMessageCounter = {{
        {MimeMessage::State::UNKNOWN  , 0},
        {MimeMessage::State::SENDING  , 0},
        {MimeMessage::State::SENT     , 0},
        {MimeMessage::State::READ     , 0},
        {MimeMessage::State::FAILURE  , 0},
        {MimeMessage::State::UNREAD   , 0},
        {MimeMessage::State::DISCARDED, 0},
        {MimeMessage::State::ERROR    , 0},
    }};

    //Helper
    void insertNewMessage(const QMap<QString,QString>& message, ContactMethod* cm, Media::Media::Direction direction, uint64_t id = 0);
    void insertNewSnapshot(Call* call, const QString& path);
    void initGroup(MimeMessage::Type t, ContactMethod* cm = nullptr);
    QHash<QByteArray,QByteArray> toJsons() const;
    void accountMessageStatusChanged(const uint64_t id, DRing::Account::MessageStates status);
    bool updateMessageStatus(MimeMessage* m, DRing::Account::MessageStates status);
    bool performMessageAction(MimeMessage* m, MimeMessage::Actions a);
    bool performMessageAction(MimeMessage* m, DRing::Account::MessageStates a);

    QList<Serializable::Group*> allGroups() const;

    void clear();

Q_SIGNALS:
    void messageAdded(::TextMessageNode* m);

private:
    TextRecording* q_ptr;
};

} //Media::

/**
 * This is the structure used internally to create the text conversation
 * frontend. It will be stored as a vector by the IM Model but also implement
 * a chained list for convenience
 */
struct TextMessageNode
{
    explicit TextMessageNode(Media::TextRecording* rec) : m_pRecording(rec) {}

    Media::MimeMessage*   m_pMessage      {nullptr};
    ContactMethod*        m_pContactMethod{nullptr}; //TODO remove
    ContactMethod*        m_pCM           {nullptr};
    TextMessageNode*      m_pNext         {nullptr};
    Serializable::Group*  m_pGroup        {nullptr};
    Media::TextRecording* m_pRecording    {nullptr};
    int                   m_row           {  -1   };
    QByteArray            m_AuthorSha1    {       };

    QVariant roleData(int role) const;
    QVariant snapshotRoleData(int role) const;
    bool setRoleData(int role, const QVariant& value);
};
