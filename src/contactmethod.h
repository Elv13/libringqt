/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
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

#include "typedefs.h"
#include <time.h>
#include <picocms/itembase.h>

//Qt
#include <QStringList>
#include <QtCore/QSize>
#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

//Ring
#include "itemdataroles.h"
#include "uri.h"
#include "libcard/event.h"
class Account;
class AccountPrivate;
class Person;
class Call;
class ContactMethodPrivate;
class ContactMethodDirectoryPrivate;
class TemporaryContactMethod;
class NumberCategory;
class TemporaryContactMethodPrivate;
class Certificate;
class UsageStatistics;
class Individual;
class EventAggregate;
class ContactRequest;

namespace Media {
   class TextRecording;
   class TextRecordingPrivate;
}

/**
 * Minimal information to reach somehone and its associated metadata.
 *
 * A ContactMethod has the following primary key:
 *
 *  * An URI
 *
 * And secondary key:
 *
 *  * An account
 *  * A person (contact)
 *
 * There CM are created by the IndividualDirectory. Only 1 instance per
 * primary+secondary keys can exists at any time. The IndividualDirectory
 * ensure that implicitly. Note that these objects are proxies and can be merged
 * and split transparently by the IndividualDirectory. The URI is immutable
 * while the account and contact can go from nullptr to a value (but not the
 * other way around).
 *
 * Note that if multiple account exits, it is possible to have multiple
 * ContactMethod with the same URI, but different accounts.
 *
 * The TemporaryContactMethod class allows to create CM with mutable URI. Once
 * it is converted into a standard ContactMethod, it is no longer mutable.
 *
 * To edit an URI of a ContactMethod, a new one needs to be created everytime.
 * Note that the CM are only freed if they are duplicates. It is not recommended
 * to create too many temporary ones.
 *
 * An "human" or entity can have multiple way to contact them and thus multiple
 * ContactMethod objects. They are linked using the `Individual` object. The
 * individual is the abstract version of the Person just as ContactMethod is the
 * abstract version of an URI. Their job is to gather metadata regardless of
 * what they are in the GUI.
 */
class LIB_EXPORT ContactMethod : public ItemBase
{
   Q_OBJECT
public:
   friend class IndividualDirectory; // factory
   friend class IndividualDirectoryPrivate; // owner
   friend class LocalTextRecordingCollection; // Manage the CM own text recording
   friend class CallPrivate; //TODO remove, this is a legacy of the pre-Event separation of concerns
   friend class AccountPrivate; // An account is a ContactMethod and share some internals
   friend class Account; // update type
   friend class Individual; // manage d_ptr->m_pIndividual
   friend class IndividualPrivate; // access it's own internal properties stored in an opaque ptr
   friend class Media::TextRecording; // update last used
   friend class Media::TextRecordingPrivate; // update last used
   friend class EventModelPrivate; // manage the CM Events, emit signals and so on
   friend class EventModel; // manage the CM Events, emit signals and so on
   friend class IncomingContactRequestManager; // manage the confirmation status
   friend class NotificationStateCollection; // manage the setNotificationPolicy

    // To synchronize the timeline weak pointers
    friend class Person;

   enum class Role {
      Uri          = static_cast<int>(Ring::Role::UserRole) + 1000,
      CategoryIcon     ,
      CategoryName     ,
      IsReachable      ,
      Filter           ,
      CanCall          ,
      CanVideoCall     ,
      CanSendTexts     ,
      TotalCallCount   ,
      TotalMessageCount,
      TotalEventCount  ,
      Type             ,
      CategoryKey      ,
      Account          ,
      RegisteredName   ,
      UserData, // This has to stay the last role, see itemdataroles.h
      //TODO implement all others
   };

   //Properties
   Q_PROPERTY(Account*          account          READ account WRITE setAccount NOTIFY accountChanged  )
   Q_PROPERTY(Person*           person           READ contact           WRITE setPerson NOTIFY contactChanged)
   Q_PROPERTY(int               lastUsed         READ lastUsed          NOTIFY lastUsedChanged        )
   Q_PROPERTY(QString           uri              READ uri               CONSTANT                      )
   Q_PROPERTY(int               callCount        READ callCount         NOTIFY callAdded              )
   Q_PROPERTY(const QList<Call*> calls           READ calls                                           )
   Q_PROPERTY(bool              bookmarked       READ isBookmarked WRITE setBookmarked NOTIFY bookmarkedChanged )
   Q_PROPERTY(bool              isBookmarked     READ isBookmarked      WRITE setBookmarked NOTIFY bookmarkedChanged) //DEPRECATED
   Q_PROPERTY(QString           uid              READ uid                                             )
   Q_PROPERTY(bool              isTracked        READ isTracked         NOTIFY trackedChanged         )
   Q_PROPERTY(bool              isPresent        READ isPresent         NOTIFY presentChanged         )
   Q_PROPERTY(bool              isSelf           READ isSelf            NOTIFY accountChanged         )
   Q_PROPERTY(bool              supportPresence  READ supportPresence                                 )
   Q_PROPERTY(QString           presenceMessage  READ presenceMessage   NOTIFY presenceMessageChanged )
   Q_PROPERTY(uint              weekCount        READ weekCount                                       )
   Q_PROPERTY(uint              trimCount        READ trimCount                                       )
   Q_PROPERTY(bool              haveCalled       READ haveCalled        NOTIFY callAdded              )
   Q_PROPERTY(QString           primaryName      READ primaryName       NOTIFY primaryNameChanged     )
   Q_PROPERTY(bool              isRecording      READ isRecording                                     )
   Q_PROPERTY(bool              hasActiveCall    READ hasActiveCall     NOTIFY hasActiveCallChanged   )
   Q_PROPERTY(bool              hasInitCall      READ hasInitCall       NOTIFY hasInitCallChanged     )
   Q_PROPERTY(bool              hasActiveVideo   READ hasActiveVideo                                  )
   Q_PROPERTY(QVariant          icon             READ icon                                            )
   Q_PROPERTY(int               totalSpentTime   READ totalSpentTime    NOTIFY callAdded              )
   Q_PROPERTY(URI::ProtocolHint protocolHint     READ protocolHint                                    )
   Q_PROPERTY(bool              isReachable      READ isReachable                                     )
   Q_PROPERTY(Certificate*      certificate      READ certificate                                     )
   Q_PROPERTY(QString           registeredName   READ registeredName    NOTIFY registeredNameSet      )
   Q_PROPERTY(QString           bestId           READ bestId                                          )
   Q_PROPERTY(QString           bestName         READ bestName          NOTIFY primaryNameChanged     )
   Q_PROPERTY(Type              type             READ type                                            )
   Q_PROPERTY(ContactRequest*   request          READ request           NOTIFY contactRequestChanged  )
   Q_PROPERTY(Individual*       individual       READ individual        CONSTANT                      )
   Q_PROPERTY(Call*            firstOutgoingCall READ firstOutgoingCall NOTIFY hasActiveCallChanged   )
   Q_PROPERTY(Call*            firstActiveCall   READ firstActiveCall   NOTIFY hasActiveCallChanged   )

   Q_PROPERTY(NotificationPolicy notificationPolicy READ notificationPolicy WRITE setNotificationPolicy NOTIFY changed)


   // Contact requests
   Q_PROPERTY(bool confirmationEnabled READ isConfirmationEnabled WRITE setConfirmationEnabled NOTIFY confirmationChanged)
   Q_PROPERTY(ConfirmationStatus confirmationStatus READ confirmationStatus NOTIFY confirmationChanged)

   // Media availability
   Q_PROPERTY(ContactMethod::MediaAvailailityStatus canSendTexts READ canSendTexts NOTIFY mediaAvailabilityChanged )
   Q_PROPERTY(ContactMethod::MediaAvailailityStatus canCall      READ canCall      NOTIFY mediaAvailabilityChanged )
   Q_PROPERTY(ContactMethod::MediaAvailailityStatus canVideoCall READ canVideoCall NOTIFY mediaAvailabilityChanged )
   Q_PROPERTY(bool                                  isAvailable  READ isAvailable  NOTIFY mediaAvailabilityChanged )

   Q_PROPERTY(Media::TextRecording* textRecording READ textRecording CONSTANT)

   ///@enum Type: Is this temporary, blank, used or unused
   enum class Type {
      BLANK     = 0, /*!< This number represent no number                                  */
      TEMPORARY = 1, /*!< This number is not yet complete                                  */
      USED      = 2, /*!< This number have been called before                              */
      UNUSED    = 3, /*!< This number have never been called, but is in the address book   */
      ACCOUNT   = 4, /*!< This number correspond to the URI of a SIP account               */
   };
   Q_ENUMS(Type)

   /**
    * Track why a media can or cannot be used with this contact method.
    *
    * @note The errors are ordered by criticality
    */
   enum class MediaAvailailityStatus : char {
       AVAILABLE    = 0, /*!< There is no issue                                              */
       NO_CALL      = 1, /*!< The media is unavailable because it requires an active call    */
       UNSUPPORTED  = 2, /*!< The media is unavailable because the account doesn't support it*/
       SETTINGS     = 3, /*!< The media is unavailable because it's disabled on purpose      */
       NO_ACCOUNT   = 4, /*!< No account is available for the URI protocol                   */
       CODECS       = 5, /*!< The media is unavailable because all codecs are unchecked      */
       ACCOUNT_DOWN = 6, /*!< An account is set, but it's not currently available            */
       NETWORK      = 7, /*!< There is a known network issue                                 */
       COUNT__
   };
   Q_ENUM(MediaAvailailityStatus)

   /**
    * How to handle the notifications for this ContactMethod.
    */
   enum class NotificationPolicy : char {
        DEFAULT, /*!< Use the default notification policy                     */
        NOTIFY , /*!< Always post notification except in  do-not-disturb mode */
        FORCE  , /*!< Always post the notifications                           */
        SKIP   , /*!< No not create notifications for this contact method     */
   };
   Q_ENUM(NotificationPolicy)

   /**
    * Ring accounts support contact (and/or firends) requests.
    *
    * Those requests allow to "trust" a RingID for incoming calls and also
    * synchronize the public profile and presence.
    */
   enum class ConfirmationStatus : char {
       NOT_APPLICABLE, /*!< This CM isn't for the Ring protocol                    */
       NO_ACCOUNT    , /*!< This is a Ring CM, but there is no account             */
       UNCONFIRMED   , /*!< The contact request has not been sent since startup    */
       PENDING       , /*!< The contact request has been sent, but is not accepted */
       CONFIRMED     , /*!< The contact request has been confirmed                 */
       DISABLED      , /*!< The user asked not to be asked for this CM again       */
   };
   Q_ENUM(ConfirmationStatus)

   //Getters
   const URI&            uri             () const;
   NumberCategory*       category        () const;
   bool                  isSelf          () const;
   bool                  isTracked       () const;
   bool                  isPresent       () const;
   QString               presenceMessage () const;
   Account*              account         () const;
   Person*               contact         () const;
   time_t                lastUsed        () const;
   ContactMethod::Type   type            () const;
   int                   callCount       () const;
   uint                  weekCount       () const;
   uint                  trimCount       () const;
   bool                  haveCalled      () const;
   const QList<Call*>    calls           () const;
   QHash<QString, QPair<int, time_t>> alternativeNames() const;
   QString               primaryName     () const;
   bool                  isBookmarked    () const;
   bool                  supportPresence () const;
   virtual QVariant      icon            () const;
   int                   totalSpentTime  () const;
   QString               uid             () const;
   URI::ProtocolHint     protocolHint    () const;
   QByteArray            sha1            () const;
   QJsonObject           toJson          () const;
   Media::TextRecording* textRecording   () const;
   bool                  isReachable     () const;
   Certificate*          certificate     () const;
   QString               registeredName  () const;
   QString               bestId          () const;
   bool                  isDuplicate     () const;
   QString               bestName        () const;
   bool                  isRecording     () const;
   bool                  hasActiveCall   () const;
   bool                  hasInitCall     () const;
   bool                  hasActiveVideo  () const;
   UsageStatistics*      usageStatistics () const;
   Call*                 firstOutgoingCall() const;
   Call*                 firstActiveCall () const;
   bool                  hasTextRecordings() const;
   NotificationPolicy    notificationPolicy() const;

   // ContactRequest
   ContactRequest* request() const;
   ConfirmationStatus confirmationStatus() const;
   bool isConfirmationEnabled() const;
   void setConfirmationEnabled(bool enabled);
   Q_INVOKABLE bool sendContactRequest() const;

   /// Opaque pointer to be used as a deduplicated identifier
   ContactMethodPrivate* d() const;

   MediaAvailailityStatus canSendTexts(bool warn = false) const;
   MediaAvailailityStatus canCall() const;
   MediaAvailailityStatus canVideoCall() const;
   bool isAvailable() const;
   QVector<Media::TextRecording*> alternativeTextRecordings() const;

   QSharedPointer<Event>          oldestEvent   () const;
   QSharedPointer<Event>          newestEvent   () const;

   QSharedPointer<EventAggregate> eventAggregate() const;

   Individual* individual() const;

   /*
    * Returns roles associated on ContactMethod based on Call::Roles
    * Returns last call info when querying info on call associated to this contact method
    */
   Q_INVOKABLE QVariant   roleData   (int role) const;
   Q_INVOKABLE QMimeData* mimePayload(        ) const;

   Q_INVOKABLE bool setRoleData(const QVariant &value, int role);

   //Setters
   Q_INVOKABLE void setAccount       (Account*            account       );
   Q_INVOKABLE void setPerson        (Person*             contact       );
   Q_INVOKABLE void setTracked       (bool                track         );
   void             setCategory      (NumberCategory*     cat           );
   void             setBookmarked    (bool                bookmarked    );
   void setNotificationPolicy(NotificationPolicy p);

   //Mutator
   Q_INVOKABLE void addCall(Call* call);
   Q_INVOKABLE void incrementAlternativeName(const QString& name, const time_t lastUsed);

   //Static
   static const ContactMethod* BLANK();

   //Helper
   QString toHash() const;

   /**
    * Convert to the rfc5545 attendee format.
    * @param QString name The `CN` name (default = bestName()).
    */
   QByteArray toAttendee(const QString& name) const;

   //Operator
   bool operator==(ContactMethod* other);
   bool operator==(const ContactMethod* other) const;
   bool operator==(ContactMethod& other);
   bool operator==(const ContactMethod& other) const;

protected:
   //Constructor
   ContactMethod(const URI& uri, NumberCategory* cat, Type st = Type::UNUSED);
   virtual ~ContactMethod();

   //Private setters
   void setPresenceMessage(const QString& message);

   //IndividualDirectory mutator
   bool merge(ContactMethod* other);

   ContactMethodDirectoryPrivate* dir_d_ptr {nullptr};

   //Many phone numbers can have the same "d" if they were merged
   ContactMethodPrivate* d_ptr;

private:
   friend class ContactMethodPrivate;

public Q_SLOTS:
   bool sendOfflineTextMessage(const QMap<QString, QString>& payloads);

Q_SIGNALS:
   ///A new call have used this ContactMethod
   void callAdded             ( Call* call          );
   ///A property associated with this number has changed
   void changed               (                     );
   ///The presence status of this phone number has changed
   void presentChanged        ( bool                );
   ///The presence status message associated with this number
   void presenceMessageChanged( const QString&      );
   ///This number track presence
   void trackedChanged        ( bool                );
   /**
    * The name used to be represent this number has changed
    * It is important for user of this object to track this
    * as the name will change over time as new contact
    * sources are added
    */
   void primaryNameChanged    ( const QString& name );
   /**
    * Two previously independent number have been merged
    * this happen when new information cues prove that number
    * with previously ambiguous data
    */
   void rebased               ( ContactMethod* other  );
   ///The most recent time there was an interaction with this CM changed
   void lastUsedChanged(time_t t);
   ///The person attached to this CM has changed
   void contactChanged(Person* newContact, Person* oldContact);
   /// The number of unread text messages has changed
   void unreadTextMessageCountChanged();
   /// When the registered name is set
   void registeredNameSet(const QString& name);
   /// When the bookmark status changes
   void bookmarkedChanged(bool isBookmarked);
   /// Emitted when the timeline is merged into another
   void timelineMerged();
   /// When a new alternative TextRecording is added
   void textRecordingAdded(Media::TextRecording* t);
   /// When the capacity to send text messages changes.
   void mediaAvailabilityChanged();
   /// When one or more call is in progress.
   void hasActiveCallChanged(bool status);
   /// When one or more call is undergoing initialization or is ringing
   void hasInitCallChanged(bool status);
   /// When an account is set, it should not "change" after that
   void accountChanged(Account* account);
   /// When an event is attached to a ContactMethod
   void eventAdded(QSharedPointer<Event> e);
   /// When an event is detached from a ContactMethod
   void eventDetached(QSharedPointer<Event> e);
   /// When the status of the ContactRequest changes.
   void contactRequestChanged();
   /// When the daemon confirms the ContactMethod to be accepted by the peer
   void confirmationChanged(ConfirmationStatus status);
};

Q_DECLARE_METATYPE(ContactMethod*)
Q_DECLARE_METATYPE(ContactMethod::MediaAvailailityStatus)
Q_DECLARE_METATYPE(ContactMethod::Type)

///@class TemporaryContactMethod: An incomplete phone number
class LIB_EXPORT TemporaryContactMethod : public ContactMethod {
   Q_OBJECT
public:
   explicit TemporaryContactMethod(const ContactMethod* number = nullptr);
   virtual QVariant icon() const override;
   void setUri(const URI& uri);
   void setRegisteredName(const QString& regName);

private:
   TemporaryContactMethodPrivate* d_ptr;
   Q_DECLARE_PRIVATE(TemporaryContactMethod)
};
