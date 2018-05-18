/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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

// Ring
#include <media/media.h>
#include <typedefs.h>
class ContactMethod;

namespace DRing {namespace Account {
    enum class MessageStates;
}}

namespace Serializable {
    class Group;
}
namespace Media {

class MimeMessagePrivate;

class TextRecordingPrivate;
class TextRecording;

class LIB_EXPORT MimeMessage
{
    friend class Serializable::Group; // existing object factory
    friend class TextRecordingPrivate; // new object factory
    friend class TextRecording; //::setAllRead perform action

public:

    class Payload {
    public:
        QString payload;
        QString mimeType;

        void read (const QJsonObject &json);
        void write(QJsonObject       &json) const;
    };

    enum class Type {
        CHAT    , /*!< Normal message between the peer                                           */
        STATUS  , /*!< "Room status" message, such as new participants or participants that left */
        SNAPSHOT, /*!< An image or clip extracted from the video                                 */
    };

    /** Used to keep the state in sync with daemon or user events.
     *
     * This class doesn't manage its own state, it is fully dependent on the
     * factory to do it.
     *
     * All this does is ensure the state changes are valid.
     */
    enum class Actions {
        SEND,
        CONFIRM,
        ERROR,
        READ,
        UNREAD,
        DISCARD,
        NOTHING,
        COUNT__
    };

    /** Possible messages states
    * WARNING the order is in sync with both the daemon and the json files
    */
    enum class State : unsigned int {
        UNKNOWN = 0, /*!< The information is unavailable                                                */
        SENDING    , /*!< An outgoing message is yet to be sent                                         */
        SENT       , /*!< The message has been sent, but not confirmed                                  */
        READ       , /*!< The message has been read (by the peer if outgoing, but the user is incoming) */
        FAILURE    , /*!< Sending the message failed                                                    */
        UNREAD     , /*!< The (incoming) message hasn't been marked as read by the user                 */
        DISCARDED  , /*!< The user don't want to see this message anymore               (incoming only) */
        ERROR      , /*!< An impossible state transition was attempted, all hope is lost                */
        COUNT__    ,
    };
    Q_ENUMS(Status)

    // Getter
    time_t timestamp() const;
    QString plainText() const;
    QString html() const;
    QList<QUrl> linkList() const;
    Media::Direction direction() const;
    uint64_t id() const;
    Type type() const;
    State status() const;
    bool hasText() const;
    QList<Payload*> payloads() const;

    const QString& getFormattedHtml(); //FIXME remove

private:

    explicit MimeMessage();
    virtual ~MimeMessage();

    static MimeMessage* buildFromSnapshot(const QString& path);
    static MimeMessage* buildExisting(const QJsonObject &json);
    static MimeMessage* buildNew(
        const QMap<QString,QString>& message,
        Media::Direction direction,
        uint64_t id
    );

    // Mutator
    bool performAction(const Actions action);
    bool performAction(const DRing::Account::MessageStates daemonState);

    void write(QJsonObject       &json) const;

    MimeMessagePrivate* d_ptr;
    Q_DECLARE_PRIVATE(MimeMessage);
};

} // Media::

Q_DECLARE_METATYPE(Media::MimeMessage::State)
