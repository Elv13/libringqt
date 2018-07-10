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
#include "mimemessage.h"


// Qt
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QRegExp>
#include <QtCore/QUrl>
#include <QtCore/QRegularExpression>

// Ring
#include "mime.h"
#include "libcard/matrixutils.h"
#include "account_const.h"

namespace Media {

class MimeMessagePrivate
{
public:

    // Constants
    static const QRegularExpression m_linkRegex;

    // Attributes
    time_t                       m_TimeStamp;
    QList<MimeMessage::Payload*> m_lPayloads;
    QString                      authorSha1;
    Media::Direction             m_Direction { Media::Direction::OUT       };
    MimeMessage::Type            m_Type      { MimeMessage::Type::CHAT     };
    MimeMessage::State           m_Status    { MimeMessage::State::UNKNOWN };
    uint64_t                     m_Id        {               0             };


    //Cache the most common payload to avoid lookup
    QString     m_PlainText;
    QString     m_HTML;
    QString     m_FormattedHtml;
    QList<QUrl> m_LinkList;
    bool        m_HasText     {false};
    bool        m_HasSnapshot {false};
    bool        m_HasUri      {false};

    // Incoming and outgoing messages don't have the same lifecycle
    static const Matrix2D<MimeMessage::State, MimeMessage::Actions, MimeMessage::State> m_mStateMapIn;
    static const Matrix2D<MimeMessage::State, MimeMessage::Actions, MimeMessage::State> m_mStateMapOut;
    static const MimeMessage::Actions m_mDaemonStateMap[(int)MimeMessage::State::COUNT__][5];

};

#define ST MimeMessage::State::
#define ACT MimeMessage::Actions::
#define ROW MimeMessagePrivate::DaemonActionRow

/*
 * Outgoing messages are (usually) confirmed by the peer and it's state cannot
 * be affected by some user actions like "mark as unread" as it would make no
 * sense.
 */
const Matrix2D<MimeMessage::State, MimeMessage::Actions, MimeMessage::State> MimeMessagePrivate::m_mStateMapOut ={{
   /*                      SEND,        CONFIRM,       ERROR,        READ,       UNREAD,     DISCARD       NOTHING    */
   { ST UNKNOWN    , {{ ST SENDING , ST UNREAD   , ST ERROR    , ST READ     , ST ERROR, ST DISCARDED, ST UNKNOWN    }}},
   { ST SENDING    , {{ ST ERROR   , ST SENT     , ST FAILURE  , ST ERROR    , ST ERROR, ST DISCARDED, ST SENDING    }}},
   { ST SENT       , {{ ST ERROR   , ST ERROR    , ST ERROR    , ST READ     , ST ERROR, ST DISCARDED, ST SENT       }}},
   { ST READ       , {{ ST ERROR   , ST ERROR    , ST ERROR    , ST READ     , ST ERROR, ST DISCARDED, ST READ       }}},
   { ST FAILURE    , {{ ST ERROR   , ST ERROR    , ST ERROR    , ST READ     , ST ERROR, ST DISCARDED, ST FAILURE    }}},
   { ST UNREAD     , {{ ST ERROR   , ST ERROR    , ST ERROR    , ST READ     , ST ERROR, ST DISCARDED, ST UNREAD     }}},
   { ST DISCARDED  , {{ ST ERROR   , ST DISCARDED, ST DISCARDED, ST DISCARDED, ST ERROR, ST DISCARDED, ST DISCARDED  }}},
   { ST ERROR      , {{ ST ERROR   , ST ERROR    , ST ERROR    , ST ERROR    , ST ERROR, ST DISCARDED, ST ERROR      }}},
}};

/*
 * Those actions are purely local and don't involve sending anything to the peer.
 */
const Matrix2D<MimeMessage::State, MimeMessage::Actions, MimeMessage::State> MimeMessagePrivate::m_mStateMapIn ={{
   /*                      SEND,   CONFIRM,         ERROR,        READ,       UNREAD,     DISCARD       NOTHING     */
   { ST UNKNOWN    , {{ ST ERROR, ST ERROR    , ST ERROR    , ST READ     , ST UNREAD, ST DISCARDED, ST UNKNOWN   }}},
   { ST SENDING    , {{ ST ERROR, ST ERROR    , ST ERROR    , ST ERROR    , ST UNREAD, ST DISCARDED, ST SENDING   }}},
   { ST SENT       , {{ ST ERROR, ST ERROR    , ST ERROR    , ST READ     , ST UNREAD, ST DISCARDED, ST SENT      }}},
   { ST READ       , {{ ST ERROR, ST ERROR    , ST ERROR    , ST READ     , ST UNREAD, ST DISCARDED, ST READ      }}},
   { ST FAILURE    , {{ ST ERROR, ST ERROR    , ST ERROR    , ST READ     , ST ERROR , ST DISCARDED, ST FAILURE   }}},
   { ST UNREAD     , {{ ST ERROR, ST ERROR    , ST ERROR    , ST READ     , ST UNREAD, ST DISCARDED, ST UNREAD    }}},
   { ST DISCARDED  , {{ ST ERROR, ST DISCARDED, ST DISCARDED, ST DISCARDED, ST ERROR , ST DISCARDED, ST DISCARDED }}},
   { ST ERROR      , {{ ST ERROR, ST ERROR    , ST ERROR    , ST ERROR    , ST ERROR , ST DISCARDED, ST ERROR     }}},
}};

/*
 * Map a daemon state change to a local state change actions.
 */
const MimeMessage::Actions MimeMessagePrivate::m_mDaemonStateMap[(int)MimeMessage::State::COUNT__][5] = {
   /*                    UNKNOWN,    SENDING,      SENT,        READ,       FAILURE   */
   /* UNKNOWN    */ { ACT NOTHING, ACT SEND    , ACT ERROR  , ACT ERROR  , ACT ERROR  },
   /* SENDING    */ { ACT ERROR  , ACT NOTHING , ACT CONFIRM, ACT READ   , ACT ERROR  },
   /* SENT       */ { ACT ERROR  , ACT ERROR   , ACT NOTHING, ACT READ   , ACT ERROR  },
   /* READ       */ { ACT ERROR  , ACT ERROR   , ACT ERROR  , ACT NOTHING, ACT ERROR  },
   /* FAILURE    */ { ACT ERROR  , ACT SEND    , ACT ERROR  , ACT ERROR  , ACT NOTHING},
   /* UNREAD     */ { ACT ERROR  , ACT ERROR   , ACT ERROR  , ACT ERROR  , ACT ERROR  },
   /* DISCARDED  */ { ACT ERROR  , ACT SEND    , ACT NOTHING, ACT NOTHING, ACT ERROR  },
   /* ERROR      */ { ACT ERROR  , ACT ERROR   , ACT ERROR  , ACT ERROR  , ACT ERROR  },
};

#undef ROW
#undef ACT
#undef ST

MimeMessage::MimeMessage() : d_ptr(new MimeMessagePrivate)
{
}

MimeMessage* MimeMessage::buildFromSnapshot(const QString& path)
{
    auto ret = new MimeMessage();

    time_t currentTime;
    ::time(&currentTime);

    ret->d_ptr->m_HasSnapshot = true;
    ret->d_ptr->m_Direction   = Media::Direction::IN;
    ret->d_ptr->m_Type        = MimeMessage::Type::SNAPSHOT;
    ret->d_ptr->m_Id          = currentTime^qrand();
    ret->d_ptr->m_TimeStamp   = currentTime;
    ret->d_ptr->m_Status      = MimeMessage::State::READ;

    auto p = new MimeMessage::Payload();
    p->mimeType = RingMimes::SNAPSHOT;
    p->payload  = path;
    ret->d_ptr->m_lPayloads << p;

    return ret;
}

MimeMessage* MimeMessage::buildNew(const QMap<QString,QString>& message, Media::Direction direction, uint64_t id)
{
    auto m = new MimeMessage();

    //Create the message
    time_t currentTime;
    ::time(&currentTime);

    m->d_ptr->m_TimeStamp = currentTime;
    m->d_ptr->m_Direction = direction;
    m->d_ptr->m_Type      = MimeMessage::Type::CHAT;
    m->d_ptr->m_Id        = id;

    if (direction == Media::Direction::IN)
        m->d_ptr->m_Status = MimeMessage::State::UNREAD;
    else if (id)
        m->d_ptr->m_Status = MimeMessage::State::SENDING;

    static const int profileSize = QString(RingMimes::PROFILE_VCF).size();

    //TODO C++17
    for (auto i = message.constBegin(); i != message.constEnd(); i++) {
        if (i.key().left(profileSize) == RingMimes::PROFILE_VCF) {
            delete m;
            return nullptr;
        }

        // This one is currently useless
        if (i.value() == QLatin1String("application/resource-lists+xml"))
            continue;

        auto p = new MimeMessage::Payload();
        p->mimeType = i.key();
        p->payload  = i.value();
        m->d_ptr->m_lPayloads << p;

        if (p->mimeType == QLatin1String("text/plain")) {
            m->d_ptr->m_PlainText = p->payload;
            m->d_ptr->m_HasText   = true;
        }
        else if (p->mimeType == QLatin1String("text/html")) {
            m->d_ptr->m_HTML    = p->payload;
            m->d_ptr->m_HasText = true;
        }
    }

    return m;
}

MimeMessage* MimeMessage::buildExisting(const QJsonObject& json)
{
    auto m = new MimeMessage();

    const auto dir = static_cast<Media::Direction>(
        json[QStringLiteral("direction")].toInt()
    );

    const auto t = static_cast<MimeMessage::Type>(
        json[QStringLiteral("type")].toInt()
    );

    // In theory, this should use `deliveryStatus` for outgoing messages, but
    // it is unclear how realistic it can be. Should unsent messages be resent
    // in future sessions? That would be a bit awkward as the message could be
    // weeks old and totally irrelevant. So until this is sorted out, only
    // the isRead flag for incoming messages is used.
    State st = State::UNKNOWN;

    Q_ASSERT((int) dir < 2 && (int) dir >= 0);
    switch(dir) {
        case Media::Direction::IN:
            st = json[QStringLiteral("isRead")].toBool() ?
                State::READ : State::UNREAD;
            break;
        case Media::Direction::OUT:
            st = State::UNKNOWN;
            break;
        case Media::Direction::COUNT__:
        case Media::Direction::BOTH:
            break;
    }

    m->d_ptr->m_TimeStamp = json[QStringLiteral("timestamp") ].toInt();
    m->d_ptr->authorSha1  = json[QStringLiteral("authorSha1")].toString();
    m->d_ptr->m_Direction = dir;
    m->d_ptr->m_Type      = t;
    m->d_ptr->m_Id        = json[QStringLiteral("id")].toVariant().value<uint64_t>();
    m->d_ptr->m_Status    = st;

    if (dir == Media::Direction::IN)
        Q_ASSERT(st != State::UNKNOWN);

    QJsonArray a = json[QStringLiteral("payloads")].toArray();
    for (int i = 0; i < a.size(); ++i) {
        QJsonObject o = a[i].toObject();
        Payload* p = new Payload();
        p->read(o);
        m->d_ptr->m_lPayloads << p;

        if (p->mimeType == QLatin1String("text/plain")) {
            m->d_ptr->m_PlainText = p->payload;
            m->d_ptr->m_HasText   = true;
        }
        else if (p->mimeType == QLatin1String("text/html")) {
            m->d_ptr->m_HTML    = p->payload;
            m->d_ptr->m_HasText = true;
        }
    }

    //Load older conversation from a time when only 1 mime/payload pair was supported
    if (!json[QStringLiteral("payload")   ].toString().isEmpty()) {
        Payload* p  = new Payload();
        p->payload  = json[QStringLiteral("payload")  ].toString();
        p->mimeType = json[QStringLiteral("mimeType") ].toString();
        m->d_ptr->m_lPayloads << p;
        m->d_ptr->m_PlainText = p->payload;
        m->d_ptr->m_HasText   = true;
    }

    return m;
}

MimeMessage::~MimeMessage()
{
    for (auto p : qAsConst(d_ptr->m_lPayloads))
        delete p;

    delete d_ptr;
}

bool MimeMessage::performAction(const MimeMessage::Actions action)
{
    const auto st = d_ptr->m_Status;

    switch(d_ptr->m_Direction) {
        case Media::Direction::IN:
            d_ptr->m_Status = d_ptr->m_mStateMapIn[d_ptr->m_Status][action];
            break;
        case Media::Direction::OUT:
            d_ptr->m_Status = d_ptr->m_mStateMapOut[d_ptr->m_Status][action];
            break;
        case Media::Direction::BOTH:
            Q_ASSERT(false);
            [[clang::fallthrough]];
        case Media::Direction::COUNT__:
            break;
    }

    return d_ptr->m_Status != st;
}

bool MimeMessage::performAction(const DRing::Account::MessageStates daemonState)
{
    Q_ASSERT(d_ptr->m_Direction == Media::Direction::OUT);

    Q_ASSERT((int)d_ptr->m_Status < (int)MimeMessage::State::COUNT__);
    Q_ASSERT((int)daemonState < 5 && (int)daemonState >= 0);

    // Can't use a safe matrix here...
    const auto action = d_ptr->m_mDaemonStateMap[(int)d_ptr->m_Status][(int)daemonState];

    return performAction(action);
}

void MimeMessage::write(QJsonObject &json) const
{
    QJsonArray a;

    static auto toIgnore = QStringLiteral("x-ring/ring.profile.vcard");

    int validPayloads = 0;

    for (const Payload* p : qAsConst(d_ptr->m_lPayloads)) {
        // The vCards are saved elsewhere
        if (p->mimeType.left(toIgnore.size()) == toIgnore)
            return;

        QJsonObject o;
        p->write(o);
        a.append(o);
        validPayloads++;
    }

    if (!validPayloads)
        return;

    json[QStringLiteral("payloads")] = a;
    json[QStringLiteral("timestamp")     ] = static_cast<int>(d_ptr->m_TimeStamp);
    json[QStringLiteral("authorSha1")    ] = d_ptr->authorSha1;
    json[QStringLiteral("direction")     ] = static_cast<int>(d_ptr->m_Direction);
    json[QStringLiteral("type")          ] = static_cast<int>(d_ptr->m_Type);
    json[QStringLiteral("isRead")        ] = status() == State::READ;
    json[QStringLiteral("id")            ] = QString::number(d_ptr->m_Id);
    json[QStringLiteral("deliveryStatus")] = static_cast<int>(d_ptr->m_Status);
}

const QRegularExpression MimeMessagePrivate::m_linkRegex = QRegularExpression(QStringLiteral("((?>(?>https|http|ftp|ring):|www\\.)(?>[^\\s,.);!>]|[,.);!>](?!\\s|$))+)"),
                                                                                 QRegularExpression::CaseInsensitiveOption);

const QString& MimeMessage::getFormattedHtml()
{
    if (d_ptr->m_FormattedHtml.isEmpty()) {
        QString re;
        auto p = 0;
        auto it = d_ptr->m_linkRegex.globalMatch(d_ptr->m_PlainText);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            auto start = match.capturedStart();

            auto url = QUrl::fromUserInput(match.capturedRef().toString());

            if (start > p)
                re.append(d_ptr->m_PlainText.mid(p, start - p).toHtmlEscaped().replace(QLatin1Char('\n'),
                                                                                QStringLiteral("<br/>")));
            re.append(QStringLiteral("<a href=\"%1\">%2</a>")
                      .arg(QString::fromLatin1(url.toEncoded()).toHtmlEscaped(),
                           match.capturedRef().toString().toHtmlEscaped()));
            d_ptr->m_LinkList.append(url);
            p = match.capturedEnd();
        }
        if (p < d_ptr->m_PlainText.size())
            re.append(d_ptr->m_PlainText.mid(p, d_ptr->m_PlainText.size() - p).toHtmlEscaped());

        d_ptr->m_FormattedHtml = QStringLiteral("<body>%1</body>").arg(re);
    }
    return d_ptr->m_FormattedHtml;
}

time_t MimeMessage::timestamp() const
{
    return d_ptr->m_TimeStamp;
}

QString MimeMessage::plainText() const
{
    return d_ptr->m_PlainText;
}

QString MimeMessage::html() const
{
    return d_ptr->m_HTML;
}

QList<QUrl> MimeMessage::linkList() const
{
    return d_ptr->m_LinkList;
}

Media::Direction MimeMessage::direction() const
{
    return d_ptr->m_Direction;
}

uint64_t MimeMessage::id() const
{
    return d_ptr->m_Id;
}

MimeMessage::State MimeMessage::status() const
{
    return d_ptr->m_Status;
}

bool MimeMessage::hasText() const
{
    return d_ptr->m_HasText;
}

MimeMessage::Type MimeMessage::type() const
{
    return d_ptr->m_Type;
}

QList<MimeMessage::Payload*> MimeMessage::payloads() const
{
    return d_ptr->m_lPayloads;
}

void MimeMessage::Payload::read(const QJsonObject &json)
{
   payload  = json[QStringLiteral("payload") ].toString();
   mimeType = json[QStringLiteral("mimeType")].toString();
}

void MimeMessage::Payload::write(QJsonObject& json) const
{
   json[QStringLiteral("payload") ] = payload ;
   json[QStringLiteral("mimeType")] = mimeType;
}

} //Media::
