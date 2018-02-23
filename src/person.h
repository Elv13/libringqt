/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <time.h>
#include <itembase.h>
#include <media/media.h>

//Ring
#include "itemdataroles.h"
class ContactMethod;
class PersonPrivate;
class AddressPrivate;
class Account;
class CollectionInterface;
class PersonPlaceHolderPrivate;
class UsageStatistics;
class Address;
class Individual;

#include "typedefs.h"

///Person: Abstract version of a contact
class LIB_EXPORT Person : public ItemBase
{
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
   Q_OBJECT
   #pragma GCC diagnostic pop

   // To check if a sibling phone numbers already build a timeline model
   friend class ContactMethod;

public:

   enum class Role {
      Organization      = static_cast<int>(Ring::Role::UserRole) + 100,
      Group             ,
      Department        ,
      PreferredEmail    ,
      FormattedName     ,
      FormattedLastUsed ,
      IndexedLastUsed   ,
      DatedLastUsed     ,
      IdOfLastCMUsed    ,
      LastName          ,
      PrimaryName       ,
      NickName          ,
      Filter            , //All roles, all at once
      DropState         = static_cast<int>(Ring::Role::DropState), //State for drag and drop
   };

   ///@enum Encoding How to decode the person content payload
   enum class Encoding {
      UID  , /*!< The bytearray only has an unique identifier      */
      vCard, /*!< The bytearray contain a RFC 6868 compliant vCard */
   };

   typedef QVector<ContactMethod*> ContactMethods;

   //Properties
   Q_PROPERTY( QString               nickName       READ nickName       WRITE setNickName       )
   Q_PROPERTY( QString               firstName      READ firstName      WRITE setFirstName      )
   Q_PROPERTY( QString               secondName     READ secondName     WRITE setFamilyName     )
   Q_PROPERTY( QString               formattedName  READ formattedName  WRITE setFormattedName  )
   Q_PROPERTY( QString               organization   READ organization   WRITE setOrganization   )
   Q_PROPERTY( QByteArray            uid            READ uid            WRITE setUid            )
   Q_PROPERTY( QString               preferredEmail READ preferredEmail WRITE setPreferredEmail )
   Q_PROPERTY( QVariant              photo          READ photo          WRITE setPhoto          NOTIFY photoChanged)
   Q_PROPERTY( QString               group          READ group          WRITE setGroup          )
   Q_PROPERTY( QString               department     READ department     WRITE setDepartment     )
   Q_PROPERTY( time_t                lastUsedTime   READ lastUsedTime                           )
   Q_PROPERTY( bool                  hasBeenCalled  READ hasBeenCalled                          )
   Q_PROPERTY( bool                  isProfile      READ isProfile                              )
   Q_PROPERTY( bool                  isPresent      READ isPresent      NOTIFY presenceChanged  )
   Q_PROPERTY( bool                  isTracked      READ isTracked      NOTIFY trackedChanged   )

   Q_PROPERTY( ContactMethod*        lastUsedContactMethod READ lastUsedContactMethod)

   Q_PROPERTY( QSharedPointer<Individual> individual READ individual CONSTANT)
   Q_PROPERTY( QSharedPointer<QAbstractItemModel> addressesModel    READ addressesModel CONSTANT)
   Q_PROPERTY( QSharedPointer<QAbstractItemModel> timelineModel     READ timelineModel CONSTANT)

   //Mutator
   Q_INVOKABLE void addAddress(Address* addr);
   Q_INVOKABLE void addCustomField(const QByteArray& key, const QByteArray& value);
   Q_INVOKABLE bool removeCustomField(const QByteArray& key, const QByteArray& value);
   Q_INVOKABLE int  removeAllCustomFields(const QByteArray& key);
   Q_INVOKABLE const QByteArray toVCard(QList<Account*> accounts = {}) const;
   Q_INVOKABLE QList<QByteArray> getCustomFields(const QByteArray& name) const;
   Q_INVOKABLE bool hasCustomField(const QByteArray& name) const;

protected:
   //The D-Pointer can be shared if a PlaceHolderPerson is merged with a real one
   PersonPrivate* d_ptr;
   Q_DECLARE_PRIVATE(Person)

   void replaceDPointer(Person* other);

public:
   //Constructors & Destructors
   explicit Person(CollectionInterface* parent = nullptr);
   Person(const QByteArray& content, Person::Encoding encoding = Encoding::UID, CollectionInterface* parent = nullptr);
   Person(const Person& other) noexcept;
   virtual ~Person();

   //Getters
   QString nickName         () const;
   QString firstName        () const;
   QString secondName       () const;
   QString formattedName    () const;
   QString organization     () const;
   QByteArray uid           () const;
   QString preferredEmail   () const;
   QVariant photo           () const;
   QString group            () const;
   QString department       () const;
   bool    isProfile        () const;
   UsageStatistics* usageStatistics () const;
   time_t lastUsedTime              () const;
   ContactMethod* lastUsedContactMethod() const;
   QList<Address*> addresses        () const;
   QMultiMap<QByteArray, QByteArray> otherFields() const;

   QSharedPointer<Individual>  individual() const;
   QSharedPointer<QAbstractItemModel> addressesModel() const;
   QSharedPointer<QAbstractItemModel> timelineModel() const;

   Q_INVOKABLE QVariant   roleData   (int role) const;
   Q_INVOKABLE QMimeData* mimePayload(        ) const;

   //Cache
   QString filterString            () const;

   //Number related getters (proxies)
   bool isPresent                  () const;
   bool isTracked                  () const;
   bool supportPresence            () const;
   bool isReachable                () const;
   bool isPlaceHolder              () const;
   bool hasBeenCalled              () const;
   bool canCall                    () const;
   bool canVideoCall               () const;
   bool canSendTexts               () const;
   bool hasRecording(Media::Media::Type type, Media::Media::Direction direction) const;

   //Setters
   void setFormattedName  ( const QString&    name   );
   void setNickName       ( const QString&    name   );
   void setFirstName      ( const QString&    name   );
   void setFamilyName     ( const QString&    name   );
   void setOrganization   ( const QString&    name   );
   void setPreferredEmail ( const QString&    name   );
   void setGroup          ( const QString&    name   );
   void setDepartment     ( const QString&    name   );
   void setUid            ( const QByteArray& id     );
   void setPhoto          ( const QVariant&   photo  );
   void ensureUid         (                          );

   //Updates an existing contact from vCard info
   void updateFromVCard(const QByteArray& content);

   //Operator
   bool operator==(const Person* other) const;
   bool operator==(const Person& other) const;

Q_SIGNALS:
   ///The presence status of a contact method changed
   void presenceChanged           ( ContactMethod* );
   ///The presence status of a contact method changed
   void trackedChanged            ( ContactMethod* );
   ///The person presence status changed
   void statusChanged             ( bool           );
   ///The person properties changed
   void changed                   (                );
   ///The address themselves have changed
   void addressesChanged          (                );
   ///The address themselvesd are about to change
   void addressesAboutToChange    (                );
   ///The person data were merged from another source
   void rebased                   ( Person* other  );
   ///The last time there was an interaction with this person changed
   void lastUsedTimeChanged       ( long long      ) const;
   ///A new call used a ContactMethod associated with this Person
   void callAdded                 ( Call* call     );
   ///When the photo changed
   void photoChanged();
};

class LIB_EXPORT PersonPlaceHolder : public Person {
   Q_OBJECT
public:
   explicit PersonPlaceHolder(const QByteArray& uid);
   bool merge(Person* contact);
private:
   PersonPlaceHolderPrivate* d_ptr;
   Q_DECLARE_PRIVATE(PersonPlaceHolder)
};

Q_DECLARE_METATYPE(Person*)
