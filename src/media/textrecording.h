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

#include "media/recording.h"
#include "media/media.h"
#include "itemdataroles.h"

//Qt
class QJsonObject;
class QAbstractItemModel;

//Ring
class IMConversationManagerPrivate;
class LocalTextRecordingEditor;
class ContactMethod;
class IndividualTimelineModel;
class IndividualTimelineModelPrivate;
class MimeMessage;

namespace Media {

class TextRecordingPrivate;
class Text;

class LIB_EXPORT TextRecording : public Recording
{
   Q_OBJECT

   friend class ::IMConversationManagerPrivate;
   friend class ::LocalTextRecordingEditor;
   friend class Text;
   friend class Video;
   friend class ::ContactMethod;
   friend class ::IndividualTimelineModel;
   friend class ::IndividualTimelineModelPrivate;

public:

    Q_PROPERTY(QAbstractItemModel* instantMessagingModel           READ instantMessagingModel           CONSTANT)
    Q_PROPERTY(QAbstractItemModel* instantTextMessagingModel       READ instantTextMessagingModel       CONSTANT)
    Q_PROPERTY(QAbstractItemModel* unreadInstantTextMessagingModel READ unreadInstantTextMessagingModel CONSTANT)
    Q_PROPERTY(bool                isEmpty                         READ isEmpty       NOTIFY messageInserted    )
    Q_PROPERTY(int                 size                            READ size          NOTIFY messageInserted    )
    Q_PROPERTY(int                 unreadCount                     READ unreadCount   NOTIFY messageStateChanged)
    Q_PROPERTY(int                 sendingCount                    READ sendingCount  NOTIFY messageStateChanged)
    Q_PROPERTY(int                 sentCount                       READ sentCount     NOTIFY messageStateChanged)
    Q_PROPERTY(int                 receivedCount                   READ receivedCount NOTIFY messageStateChanged)
    Q_PROPERTY(int                 unknownCount                    READ unknownCount  NOTIFY messageStateChanged)

   enum class Role {
      Direction            = static_cast<int>(Ring::Role::UserRole) + 1,
      AuthorDisplayname    ,
      AuthorUri            ,
      AuthorPresenceStatus ,
      Timestamp            ,
      IsRead               ,
      FormattedDate        ,
      IsStatus             ,
      HTML                 ,
      HasText              ,
      ContactMethod        ,
      DeliveryStatus       ,
      FormattedHtml        ,
      LinkList             ,
      Id                   ,
   };

   /** Loading a full text recording takes a lot of memory that's unlikely to
    * ever be used. To keep this to a minimum,  the full text recording should
    * be lazy-loaded and freed once inactive. This structure holds the minimum
    * metadata that should be kept even when the TextRecording isn't loaded.
    *
    * Its attributes are accessed through the TextRecording object even when
    * unloaded.
    */
   struct Metadata {
      uint   messageCount {0};
      uint   unreadCount  {0};
      time_t lastUsed     {0};
   };

   //Constructor
   explicit TextRecording(const Recording::Status status);
   virtual ~TextRecording();
   static TextRecording* fromJson(const QList<QJsonObject>& items, const ContactMethod* cm = nullptr, CollectionInterface* backend = nullptr);
   static TextRecording* fromPath(const QString& path, const Metadata& metadata, CollectionInterface* backend = nullptr);

   //Getter
   QAbstractItemModel* instantMessagingModel    (                         ) const;
   QAbstractItemModel* instantTextMessagingModel(                         ) const;
   QAbstractItemModel* unreadInstantTextMessagingModel(                   ) const;
   bool                isEmpty                  (                         ) const;
   int                 count                    (                         ) const;
   int                 size                     (                         ) const;
   bool                hasMimeType              ( const QString& mimeType ) const;
   QStringList         mimeTypes                (                         ) const;
   MimeMessage*        messageAt                ( int row                 ) const;
   QVariant            roleData                 ( int row, int role       ) const;
   virtual QVariant    roleData                 ( int role                ) const override;
   QStringList         paths                    (                         ) const;
   int                 unreadCount              (                         ) const;
   int                 sendingCount             (                         ) const;
   int                 sentCount                (                         ) const;
   int                 receivedCount            (                         ) const;
   int                 unknownCount             (                         ) const;
   time_t              lastUsed                 (                         ) const;
   QVector<ContactMethod*> peers                (                         ) const;

   //Helper
   void setAllRead();
   void setAsRead(MimeMessage* m) const;
   void setAsUnread(MimeMessage* m) const;

protected:
    virtual void discard() override;
    virtual void consume() override;

Q_SIGNALS:
   void aboutToInsertMessage(const QMap<QString,QString>& message, ContactMethod* cm, Media::Media::Direction direction);
   void messageInserted(const QMap<QString,QString>& message, ContactMethod* cm, Media::Media::Direction direction);
   void unreadCountChange(int count);
   void messageStateChanged();
   void aboutToClear();
   void cleared();

private:
   TextRecordingPrivate* d_ptr;
   Q_DECLARE_PRIVATE(TextRecording)
};

}

Q_DECLARE_METATYPE(Media::TextRecording*)
