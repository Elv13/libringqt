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

//Parent
#include "person.h"

//Qt
#include <QtCore/QDateTime>

//Ring library
#include "contactmethod.h"
#include "accountmodel.h"
#include "certificatemodel.h"
#include "collectioninterface.h"
#include "transitionalpersonbackend.h"
#include "account.h"
#include "private/vcardutils.h"
#include "personmodel.h"
#include "historytimecategorymodel.h"
#include "numbercategorymodel.h"
#include "numbercategory.h"
#include "personcmmodel.h"
#include "addressmodel.h"
#include "globalinstances.h"
#include "interfaces/pixmapmanipulatori.h"
#include "private/person_p.h"
#include "private/contactmethod_p.h"
#include "media/textrecording.h"
#include "private/personstatistics.hpp"
#include "mime.h"

// Std
#include <random>

class AddressPrivate final
{
public:
   ~AddressPrivate() {}

   QString addressLine;
   QString city;
   QString zipCode;
   QString state;
   QString country;
   QString type;
};

Person::Address::Address() : d_ptr(new AddressPrivate())
{

}

Person::Address::~Address()
{
   //delete d_ptr; //FIXME ASAN doesn't like for some reasons, but also report a leak
}

QString Person::Address::addressLine() const
{
   return d_ptr->addressLine;
}

QString Person::Address::city() const
{
   return d_ptr->city;
}

QString Person::Address::zipCode() const
{
   return d_ptr->zipCode;
}

QString Person::Address::state() const
{
   return d_ptr->state;
}

QString Person::Address::country() const
{
   return d_ptr->country;
}

QString Person::Address::type() const
{
   return d_ptr->type;
}

void Person::Address::setAddressLine(const QString& value)
{
   d_ptr->addressLine = value;
}

void Person::Address::setCity(const QString& value)
{
   d_ptr->city = value;
}

void Person::Address::setZipCode(const QString& value)
{
   d_ptr->zipCode = value;
}

void Person::Address::setState(const QString& value)
{
   d_ptr->state = value;
}

void Person::Address::setCountry(const QString& value)
{
   d_ptr->country = value;
}

void Person::Address::setType(const QString& value)
{
   d_ptr->type = value;
}

QString PersonPrivate::filterString()
{
   if (m_CachedFilterString.size())
      return m_CachedFilterString;

   //Also filter by phone numbers, accents are negligible
   foreach(const ContactMethod* n , m_Numbers) {
      m_CachedFilterString += n->uri();
   }

   //Strip non essential characters like accents from the filter string
   foreach(const QChar& char2,(m_FormattedName+'\n'+m_Organization+'\n'+m_Group+'\n'+
      m_Department+'\n'+m_PreferredEmail).toLower().normalized(QString::NormalizationForm_KD) ) {
      if (!char2.combiningClass())
         m_CachedFilterString += char2;
   }

   return m_CachedFilterString;
}

void PersonPrivate::changed()
{
   m_CachedFilterString.clear();
   foreach (Person* c,m_lParents) {
      emit c->changed();
   }
}

void PersonPrivate::presenceChanged( ContactMethod* n )
{
   foreach (Person* c,m_lParents) {
      emit c->presenceChanged(n);
   }
}

void PersonPrivate::statusChanged  ( bool s )
{
   foreach (Person* c,m_lParents) {
      emit c->statusChanged(s);
   }
}

void PersonPrivate::phoneNumbersChanged()
{
   foreach (Person* c,m_lParents) {
      emit c->phoneNumbersChanged();
   }
}

void PersonPrivate::phoneNumbersAboutToChange()
{
   foreach (Person* c,m_lParents) {
      emit c->phoneNumbersAboutToChange();
   }
}

void PersonPrivate::registerContactMethod(ContactMethod* m)
{
   m_HiddenContactMethods << m;

   foreach (Person* c,m_lParents) {
      emit c->relatedContactMethodsAdded(m);
   }

   connect(m, &ContactMethod::lastUsedChanged, this, &PersonPrivate::slotLastUsedTimeChanged);
   connect(m, &ContactMethod::callAdded, this, &PersonPrivate::slotCallAdded);

   if ((!m_LastUsedCM) || m->lastUsed() > m_LastUsedCM->lastUsed())
      slotLastContactMethod(m);
}

PersonPrivate::PersonPrivate(Person* contact) : QObject(nullptr),
   m_Numbers(),m_DisplayPhoto(false),m_Active(true),m_isPlaceHolder(false),
   q_ptr(contact)
{
   moveToThread(QCoreApplication::instance()->thread());
   setParent(contact);
}

PersonPrivate::~PersonPrivate()
{
}

///Constructor
Person::Person(CollectionInterface* parent): ItemBase(nullptr),
   d_ptr(new PersonPrivate(this))
{
   setCollection(parent ? parent : &TransitionalPersonBackend::instance());

   d_ptr->m_isPlaceHolder = false;
   d_ptr->m_lParents << this;
}

Person::Person(const QByteArray& content, Person::Encoding encoding, CollectionInterface* parent)
 : ItemBase(nullptr), d_ptr(new PersonPrivate(this))
{
   setCollection(parent ? parent : &TransitionalPersonBackend::instance());
   d_ptr->m_isPlaceHolder = false;
   d_ptr->m_lParents << this;
   switch (encoding) {
      case Person::Encoding::UID:
         setUid(content);
         break;
      case Person::Encoding::vCard:
         if (!VCardUtils::mapToPerson(this, content)) {
            qDebug() << "Loading person failed";
         }
         break;
   };
}

/**
 * Copy constructor, useful when transferring a contact between collections
 *
 * For example, converting a trust request to GMail contact without forcing
 * a slow vCard conversion.
 *
 * This create a COPY of the person details, using shared attributes between
 * multiple person with multiple collection is currently not supported (but
 * would be easy to enable if the need arise).
 */
Person::Person(const Person& other) noexcept : ItemBase(nullptr),
d_ptr(new PersonPrivate(this))
{
   d_ptr->m_FirstName            = other.d_ptr->m_FirstName           ;
   d_ptr->m_SecondName           = other.d_ptr->m_SecondName          ;
   d_ptr->m_NickName             = other.d_ptr->m_NickName            ;
   d_ptr->m_vPhoto               = other.d_ptr->m_vPhoto              ;
   d_ptr->m_FormattedName        = other.d_ptr->m_FormattedName       ;
   d_ptr->m_PreferredEmail       = other.d_ptr->m_PreferredEmail      ;
   d_ptr->m_Organization         = other.d_ptr->m_Organization        ;
   d_ptr->m_Uid                  = other.d_ptr->m_Uid                 ;
   d_ptr->m_Group                = other.d_ptr->m_Group               ;
   d_ptr->m_Department           = other.d_ptr->m_Department          ;
   d_ptr->m_DisplayPhoto         = other.d_ptr->m_DisplayPhoto        ;
   d_ptr->m_Numbers              = other.d_ptr->m_Numbers             ;
   d_ptr->m_Active               = other.d_ptr->m_Active              ;
   d_ptr->m_isPlaceHolder        = other.d_ptr->m_isPlaceHolder       ;
   d_ptr->m_lAddresses           = other.d_ptr->m_lAddresses          ;
   d_ptr->m_lCustomAttributes    = other.d_ptr->m_lCustomAttributes   ;
   d_ptr->m_LastUsedCM           = other.d_ptr->m_LastUsedCM          ;
   d_ptr->m_HiddenContactMethods = other.d_ptr->m_HiddenContactMethods;
}

///Updates an existing contact from vCard info
void Person::updateFromVCard(const QByteArray& content)
{
   // empty existing contact methods first
   setContactMethods(ContactMethods());
   if (!VCardUtils::mapToPerson(this, content)) {
      qWarning() << "Updating person failed";
   }
}

///Destructor
Person::~Person()
{
   //Unregister itself from the D-Pointer list
   d_ptr->m_lParents.removeAll(this);

   if (!d_ptr->m_lParents.size()) {
      delete d_ptr;
   }
}

///Get the phone number list
Person::ContactMethods Person::phoneNumbers() const
{
   return d_ptr->m_Numbers;
}

/**
 * Over time, more ContactMethods are associated with a Person. For example,
 * if the person move and get a new phone number (and lose the old one) or
 * change country. Other example could be losing the Ring account private
 * key and having to create a new one. There is other accidental instances
 * caused by various race conditions.
 */
Person::ContactMethods Person::relatedContactMethods() const
{
    return d_ptr->m_HiddenContactMethods;
}

///Get the nickname
QString Person::nickName() const
{
   return d_ptr->m_NickName;
}

///Get the firstname
QString Person::firstName() const
{
   return d_ptr->m_FirstName;
}

///Get the second/family name
QString Person::secondName() const
{
   return d_ptr->m_SecondName;
}

///Get the photo
QVariant Person::photo() const
{
   return d_ptr->m_vPhoto;
}

///Get the formatted name
QString Person::formattedName() const
{
   return d_ptr->m_FormattedName;
}

///Get the organisation
QString Person::organization()  const
{
   return d_ptr->m_Organization;
}

///Get the preferred email
QString Person::preferredEmail()  const
{
   return d_ptr->m_PreferredEmail;
}

///Get the unique identifier (used for drag and drop)
QByteArray Person::uid() const
{
   return d_ptr->m_Uid;
}

///Get the group
QString Person::group() const
{
   return d_ptr->m_Group;
}

QString Person::department() const
{
   return d_ptr->m_Department;
}

/// Get the last ContactMethod used with that person.
ContactMethod* Person::lastUsedContactMethod() const
{
    return d_ptr->m_LastUsedCM;
}

UsageStatistics* Person::usageStatistics() const
{
    if (!d_ptr->m_pStats) {
        d_ptr->m_pStats = new PersonStatistics(this);
    }

    return d_ptr->m_pStats;
}

QSharedPointer<QAbstractItemModel> Person::phoneNumbersModel() const
{
    if (!d_ptr->m_pPhoneNumbersModel) {
        auto p = QSharedPointer<QAbstractItemModel>(new PersonCMModel(this));

        // Delete all children will otherwise silently delete the object
        p.data()->setParent(nullptr);

        d_ptr->m_pPhoneNumbersModel = p;
        return p;
    }

    return d_ptr->m_pPhoneNumbersModel;
}

QSharedPointer<QAbstractItemModel> Person::addressesModel() const
{
    if (!d_ptr->m_pAddressModel) {
        auto p = QSharedPointer<QAbstractItemModel>(new AddressModel(this));

        // Delete all children will otherwise silently delete the object
        p.data()->setParent(nullptr);

        d_ptr->m_pAddressModel = p;
        return p;
    }

    return d_ptr->m_pAddressModel;
}

/**
 * Returns a timeline model if and only if a person phone number was contacted.
 *
 * For "self manager" Person objects, it means it is *required* to set the phone
 * numbers right.
 */
QSharedPointer<QAbstractItemModel> Person::timelineModel() const
{
    // See if one of the contact methods already built one
    auto begin(d_ptr->m_Numbers.constBegin()), end(d_ptr->m_Numbers.constEnd());

    auto cmi = std::find_if(begin, end, [](ContactMethod* cm) {
        return cm->d_ptr->m_TimelineModel;
    });

    if (cmi != end)
        return (*cmi)->timelineModel();

    // Too bad, build one
    if (auto cm = d_ptr->m_Numbers.constFirst())
        return cm->timelineModel();

    // This person was never contacted
    return QSharedPointer<QAbstractItemModel>();
}

///Set the phone number (type and number)
void Person::setContactMethods(const ContactMethods& numbers)
{
   //TODO make a diff instead of full reload
   const auto dedup = QSet<ContactMethod*>::fromList(numbers.toList());

   d_ptr->phoneNumbersAboutToChange();
   for (ContactMethod* n : d_ptr->m_Numbers) {
      disconnect(n,SIGNAL(presentChanged(bool)),this,SLOT(slotPresenceChanged()));
      disconnect(n, &ContactMethod::lastUsedChanged, d_ptr, &PersonPrivate::slotLastUsedTimeChanged);
      disconnect(n, &ContactMethod::unreadTextMessageCountChanged, d_ptr, &PersonPrivate::changed);
      disconnect(n, &ContactMethod::callAdded, d_ptr, &PersonPrivate::slotCallAdded);
   }

   d_ptr->m_Numbers = ContactMethods::fromList(dedup.toList());

   for (ContactMethod* n : d_ptr->m_Numbers) {
      connect(n,SIGNAL(presentChanged(bool)),this,SLOT(slotPresenceChanged()));
      connect(n, &ContactMethod::lastUsedChanged, d_ptr, &PersonPrivate::slotLastUsedTimeChanged);
      connect(n, &ContactMethod::unreadTextMessageCountChanged, d_ptr, &PersonPrivate::changed);
      connect(n, &ContactMethod::callAdded, d_ptr, &PersonPrivate::slotCallAdded);
   }

   d_ptr->phoneNumbersChanged();
   d_ptr->changed();

   //Allow incoming calls from those numbers
   const QList<Account*> ringAccounts = AccountModel::instance().getAccountsByProtocol(Account::Protocol::RING);
   QStringList certIds;
   for (ContactMethod* n : d_ptr->m_Numbers) {
      if (n->uri().protocolHint() == URI::ProtocolHint::RING)
         certIds << n->uri().userinfo(); // certid must only contain the hash, no scheme
   }

   foreach(const QString& hash , certIds) {
      Certificate* cert = CertificateModel::instance().getCertificateFromId(hash);
      if (cert) {
         for (Account* a : ringAccounts) {
            if (a->allowIncomingFromContact())
               a->allowCertificate(cert);
         }
      }
   }
}

///Set the nickname
void Person::setNickName(const QString& name)
{
   d_ptr->m_NickName = name;
   d_ptr->changed();
}

///Set the first name
void Person::setFirstName(const QString& name)
{
   d_ptr->m_FirstName = name;
   setObjectName(formattedName());
   d_ptr->changed();
}

///Set the family name
void Person::setFamilyName(const QString& name)
{
   d_ptr->m_SecondName = name;
   setObjectName(formattedName());
   d_ptr->changed();
}

///Set the Photo/Avatar
void Person::setPhoto(const QVariant& photo)
{
   d_ptr->m_vPhoto = photo;
   d_ptr->changed();
}

///Set the formatted name (display name)
void Person::setFormattedName(const QString& name)
{
   d_ptr->m_FormattedName = name;
   d_ptr->changed();
}

///Set the organisation / business
void Person::setOrganization(const QString& name)
{
   d_ptr->m_Organization = name;
   d_ptr->changed();
}

///Set the default email
void Person::setPreferredEmail(const QString& name)
{
   d_ptr->m_PreferredEmail = name;
   d_ptr->changed();
}

///Set UID
void Person::setUid(const QByteArray& id)
{
   d_ptr->m_Uid = id;
   d_ptr->changed();
}

///Set Group
void Person::setGroup(const QString& name)
{
   d_ptr->m_Group = name;
   d_ptr->changed();
}

///Set department
void Person::setDepartment(const QString& name)
{
   d_ptr->m_Department = name;
   d_ptr->changed();
}

///Return if one of the ContactMethod is present
bool Person::isPresent() const
{
   foreach(const ContactMethod* n,d_ptr->m_Numbers) {
      if (n->isPresent())
         return true;
   }
   return false;
}

///Return if one of the ContactMethod is tracked
bool Person::isTracked() const
{
   foreach(const ContactMethod* n,d_ptr->m_Numbers) {
      if (n->isTracked())
         return true;
   }
   return false;
}

bool Person::isPlaceHolder() const
{
    return d_ptr->m_isPlaceHolder;
}

/** Get the last time this person was contacted.
 * This method returns zero when the person was never contacted.
 *  @todo Implement some caching
 */
time_t Person::lastUsedTime() const
{
   if (!d_ptr->m_LastUsedCM)
      return 0;

   return d_ptr->m_LastUsedCM->lastUsed();
}

///Return if one of the ContactMethod support presence
bool Person::supportPresence() const
{
   foreach(const ContactMethod* n,d_ptr->m_Numbers) {
      if (n->supportPresence())
         return true;
   }
   return false;
}

///Return true if there is a change one if the account can be used to reach that person
bool Person::isReachable() const
{
   if (!d_ptr->m_Numbers.size())
      return false;

   foreach (const ContactMethod* n, d_ptr->m_Numbers) {
      if (n->isReachable())
         return true;
   }
   return false;
}

bool Person::hasBeenCalled() const
{
   foreach( ContactMethod* cm, phoneNumbers()) {
      if (cm->callCount())
         return true;
   }

   return false;
}

/**
 * Return if one of the contact method has a recording
 *
 * @todo Implement AUDIO, VIDEO and FILE, The information can be obtained by \
 * foreach looping the contact methods calls, but this is overly expensive. \
 * some ContactMethod level caching need to be implemented and connected to new\
 * recording signals.
 */
bool Person::hasRecording(Media::Media::Type type, Media::Media::Direction direction) const
{
   Q_UNUSED(direction) //TODO implement

   switch (type) {
      case Media::Media::Type::AUDIO:
      case Media::Media::Type::VIDEO:
         return false; //TODO implement
      case Media::Media::Type::TEXT:
         foreach( ContactMethod* cm, phoneNumbers()) {
            if (cm->textRecording() && !cm->textRecording()->isEmpty())
               return true;
         }

         return false;
      case Media::Media::Type::FILE:
      case Media::Media::Type::COUNT__:
         break;
   }

   return false;
}

///Recomputing the filter string is heavy, cache it
QString Person::filterString() const
{
   return d_ptr->filterString();
}

///Get the role value
QVariant Person::roleData(int role) const
{
   switch (role) {
      case Qt::DisplayRole:
      case Qt::EditRole:
      case static_cast<int>(Ring::Role::Name):
      case static_cast<int>(Person::Role::FormattedName):
         return QVariant(formattedName());
      case static_cast<int>(Ring::Role::Number):
         {
            auto cm = lastUsedContactMethod();
            return cm ? cm->bestId() : QString();
         }
      case Qt::DecorationRole:
         return GlobalInstances::pixmapManipulator().decorationRole(this);
      case static_cast<int>(Person::Role::Organization):
         return QVariant(organization());
      case static_cast<int>(Person::Role::Group):
         return QVariant(group());
      case static_cast<int>(Person::Role::Department):
         return QVariant(department());
      case static_cast<int>(Person::Role::PreferredEmail):
         return QVariant(preferredEmail());
      case static_cast<int>(Ring::Role::FormattedLastUsed):
      case static_cast<int>(Person::Role::FormattedLastUsed):
         return QVariant(HistoryTimeCategoryModel::timeToHistoryCategory(lastUsedTime()));
      case static_cast<int>(Ring::Role::IndexedLastUsed):
      case static_cast<int>(Person::Role::IndexedLastUsed):
         return QVariant(static_cast<int>(HistoryTimeCategoryModel::timeToHistoryConst(lastUsedTime())));
      case static_cast<int>(Ring::Role::Object):
      case static_cast<int>(Person::Role::Object):
         return QVariant::fromValue(const_cast<Person*>(this));
      case static_cast<int>(Ring::Role::ObjectType):
         return QVariant::fromValue(Ring::ObjectType::Person);
      case static_cast<int>(Ring::Role::LastUsed):
      case static_cast<int>(Person::Role::DatedLastUsed):
         return QVariant(QDateTime::fromTime_t( lastUsedTime()));
      case static_cast<int>(Person::Role::Filter):
         return filterString();
      case static_cast<int>(Ring::Role::IsPresent):
         return isPresent();
      case static_cast<int>(Person::Role::LastName):
          return secondName();
      case static_cast<int>(Person::Role::PrimaryName):
          return firstName();
      case static_cast<int>(Person::Role::NickName):
          return nickName();
      case static_cast<int>(Person::Role::IdOfLastCMUsed):
         {
            auto cm = lastUsedContactMethod();
            return cm ? cm->bestId() : QString();
         }
      case static_cast<int>(Ring::Role::UnreadTextMessageCount):
         {
            int unread = 0;
            for (int i = 0; i < d_ptr->m_Numbers.size(); ++i) {
               if (auto rec = d_ptr->m_Numbers.at(i)->textRecording())
                  unread += rec->unreadInstantTextMessagingModel()->rowCount();
            }
            return unread;
         }
         break;
      default:
         break;
   }

   return QVariant();
}

QMimeData* Person::mimePayload() const
{
   return RingMimes::payload(nullptr, nullptr, this);
}

///Callback when one of the phone number presence change
void Person::slotPresenceChanged()
{
   d_ptr->changed();
}

///Create a placeholder contact, it will eventually be replaced when the real one is loaded
PersonPlaceHolder::PersonPlaceHolder(const QByteArray& uid):d_ptr(nullptr)
{
   setUid(uid);
   Person::d_ptr->m_isPlaceHolder = true;
}

/**
 * Sometime, items will use contacts before they are loaded.
 *
 * Once loaded, those pointers need to be upgraded to the real contact.
 */
bool PersonPlaceHolder::merge(Person* contact)
{
   if ((!contact) || ((*contact) == this))
      return false;

   PersonPrivate* currentD = Person::d_ptr;
   replaceDPointer(contact);
   currentD->m_lParents.removeAll(this);

   if (!currentD->m_lParents.size())
      delete currentD;
   return true;
}

void Person::replaceDPointer(Person* c)
{

   if (d_ptr->m_LastUsedCM && lastUsedTime() > c->lastUsedTime()) {
      c->d_ptr->m_LastUsedCM = d_ptr->m_LastUsedCM;
      emit c->lastUsedTimeChanged(d_ptr->m_LastUsedCM->lastUsed());
   }

   this->d_ptr = c->d_ptr;
   d_ptr->m_lParents << this;
   emit changed();
   emit rebased(c);
}

bool Person::operator==(const Person* other) const
{
   return other && this->d_ptr == other->d_ptr;
}

bool Person::operator==(const Person& other) const
{
   return this->d_ptr == other.d_ptr;
}

///Add a new address to this contact
void Person::addAddress(const Person::Address& addr)
{
   d_ptr->m_lAddresses << addr;
}

void Person::addPhoneNumber(ContactMethod* cm)
{
    if ((!cm) || cm->type() == ContactMethod::Type::BLANK)
        return;

    if (Q_UNLIKELY(d_ptr->m_Numbers.indexOf(cm) != -1)) {
        qWarning() << this << "already has the phone number" << cm;
        return;
    }

    if (Q_UNLIKELY(cm->contact() && cm->contact()->d_ptr != d_ptr)) {
        qWarning() << "Adding a phone number to" << this << "already owned by" << cm->contact();
    }

    d_ptr->m_Numbers << cm;

    d_ptr->phoneNumbersChanged();

    if (d_ptr->m_HiddenContactMethods.indexOf(cm) != -1) {
        d_ptr->m_HiddenContactMethods.removeAll(cm);
        foreach (Person* c, d_ptr->m_lParents)
            emit c->relatedContactMethodsRemoved(cm);
    }
}

/// Returns the addresses associated with the person.
QList<Person::Address> Person::addresses() const
{
    return d_ptr->m_lAddresses;
}

/// Anything that didn't fit in the Person fields
QMultiMap<QByteArray, QByteArray> Person::otherFields() const
{
    return d_ptr->m_lCustomAttributes;
}

/// Get the custom fields for a specific key
QList<QByteArray> Person::getCustomFields(const QByteArray& name) const
{
    return d_ptr->m_lCustomAttributes.values("X-RINGACCOUNTID");
}

///Add custom fields for contact profiles
void Person::addCustomField(const QByteArray& key, const QByteArray& value)
{
   d_ptr->m_lCustomAttributes.insert(key, value);
}

///Remove all custom fields corresponding to the key
///@return The number of elements removed
int Person::removeAllCustomFields(const QByteArray& key)
{
    return d_ptr->m_lCustomAttributes.remove(key);
}

const QByteArray Person::toVCard(QList<Account*> accounts) const
{
   //serializing here
   VCardUtils maker;
   maker.startVCard("2.1");
   maker.addProperty(VCardUtils::Property::UID, uid());
   maker.addProperty(VCardUtils::Property::NAME, (secondName()
                                                   + VCardUtils::Delimiter::SEPARATOR_TOKEN
                                                   + firstName()));
   maker.addProperty(VCardUtils::Property::FORMATTED_NAME, formattedName());
   maker.addProperty(VCardUtils::Property::ORGANIZATION, organization());

   maker.addEmail("PREF", preferredEmail());

   foreach (ContactMethod* phone , phoneNumbers()) {
      QString uri = phone->uri();
      // in the case of a RingID, we want to make sure that the uri contains "ring:" so that the user
      // can tell it is a RING number and not some other hash
      if (phone->uri().protocolHint() == URI::ProtocolHint::RING)
         uri = phone->uri().full();
      maker.addContactMethod(phone->category()->name(), uri);
   }

   foreach (const Address& addr , d_ptr->m_lAddresses) {
      maker.addAddress(addr);
   }

   for (auto i = d_ptr->m_lCustomAttributes.constBegin(); i != d_ptr->m_lCustomAttributes.constEnd(); i++) {
      maker.addProperty(i.key(), i.value());
   }

   foreach (Account* acc , accounts) {
      maker.addProperty(VCardUtils::Property::X_RINGACCOUNT, acc->id());
   }

   maker.addPhoto(GlobalInstances::pixmapManipulator().toByteArray(photo()));
   return maker.endVCard();
}

void PersonPrivate::slotLastUsedTimeChanged(::time_t t)
{
   Q_UNUSED(t)

   auto cm = qobject_cast<ContactMethod*>(sender());

   // This function should always be called by a contactmethod signal. Otherwise
   // the timestamp comes from an unknown source and cannot be traced.
   Q_ASSERT(cm);

   slotLastContactMethod(cm);
}

void PersonPrivate::slotLastContactMethod(ContactMethod* cm)
{
   if (cm && q_ptr->lastUsedTime() > cm->lastUsed())
      return;

   m_LastUsedCM = cm;

   foreach (Person* c, m_lParents) {
      emit c->lastUsedTimeChanged(cm ? cm->lastUsed() : 0);
   }
}

void PersonPrivate::slotCallAdded(Call *call)
{
    foreach (Person* c,m_lParents) {
        emit c->callAdded(call);
    }
}

/**
 * ensureUid ensures an unique Id.
 */
void
Person::ensureUid()
{
    static std::random_device rdev;
    static std::seed_seq seq {rdev(), rdev()};
    static std::mt19937_64 rand {seq};
    static std::uniform_int_distribution<uint64_t> id_generator;

    while (d_ptr->m_Uid.isEmpty()
        or (PersonModel::instance().getPersonByUid(d_ptr->m_Uid)
            && PersonModel::instance().getPersonByUid(d_ptr->m_Uid) != this)) {
        d_ptr->m_Uid = std::to_string(id_generator(rand)).c_str();
    }
}
