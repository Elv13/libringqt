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
#include "phonedirectorymodel.h"
#include "numbercategorymodel.h"
#include "numbercategory.h"
#include "individual.h"
#include "addressmodel.h"
#include "globalinstances.h"
#include "interfaces/pixmapmanipulatori.h"
#include "private/person_p.h"
#include "private/contactmethod_p.h"
#include "media/textrecording.h"
#include "private/personstatistics.hpp"
#include "mime.h"
#include "address.h"

// Std
#include <random>

QString PersonPrivate::filterString()
{
    if (m_CachedFilterString.size())
        return m_CachedFilterString;

    //Also filter by phone numbers, accents are negligible
    q_ptr->individual()->forAllNumbers([this](ContactMethod* cm) {
        m_CachedFilterString += cm->uri();
        m_CachedFilterString += cm->registeredName();
    }, false);

    //Strip non essential characters like accents from the filter string
    foreach(const QChar& char2,(m_FormattedName+'\n'+m_Organization+'\n'+m_Group+'\n'+
        m_Department+'\n'+m_PreferredEmail).toLower().normalized(QString::NormalizationForm_KD) ) {
        if (!char2.combiningClass())
            m_CachedFilterString += char2;
    }

    return m_CachedFilterString;
}

void PersonPrivate::photoChanged()
{
    for (Person* c : qAsConst(m_lParents))
        emit c->photoChanged();
}

void PersonPrivate::changed()
{
    m_CachedFilterString.clear();

    for (Person* c : qAsConst(m_lParents))
        emit c->changed();
}

void PersonPrivate::presenceChanged( ContactMethod* n )
{
    for (Person* c : qAsConst(m_lParents))
        emit c->presenceChanged(n);
}

void PersonPrivate::trackedChanged( ContactMethod* n )
{
    for (Person* c : qAsConst(m_lParents))
        emit c->trackedChanged(n);
}

void PersonPrivate::statusChanged  ( bool s )
{
    for (Person* c : qAsConst(m_lParents))
        emit c->statusChanged(s);
}

// void PersonPrivate::phoneNumbersChanged()
// {
//     for (Person* c : qAsConst(m_lParents))
//         emit c->phoneNumbersChanged();
// }

// void PersonPrivate::phoneNumbersAboutToChange()
// {
//     for (Person* c : qAsConst(m_lParents))
//         emit c->phoneNumbersAboutToChange();
// }

PersonPrivate::PersonPrivate(Person* contact) : QObject(nullptr),
   q_ptr(contact)
{
   moveToThread(QCoreApplication::instance()->thread());
   setParent(contact);
}

PersonPrivate::~PersonPrivate()
{
    while (!m_lAddresses.isEmpty())
        delete m_lAddresses.takeFirst();
}

///Constructor
Person::Person(CollectionInterface* parent): ItemBase(&PersonModel::instance()),
   d_ptr(new PersonPrivate(this))
{
   setCollection(parent ? parent : &TransitionalPersonBackend::instance());

   d_ptr->m_isPlaceHolder = false;
   d_ptr->m_lParents << this;
}

Person::Person(const QByteArray& content, Person::Encoding encoding, CollectionInterface* parent)
 : ItemBase(&PersonModel::instance()), d_ptr(new PersonPrivate(this))
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
Person::Person(const Person& other) noexcept : ItemBase(&PersonModel::instance()),
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
   d_ptr->m_pIndividual          = other.d_ptr->m_pIndividual         ;
   d_ptr->m_isPlaceHolder        = other.d_ptr->m_isPlaceHolder       ;
   d_ptr->m_lAddresses           = other.d_ptr->m_lAddresses          ;
   d_ptr->m_lCustomAttributes    = other.d_ptr->m_lCustomAttributes   ;
}

///Updates an existing contact from vCard info
void Person::updateFromVCard(const QByteArray& content)
{
    // Make sure nothing leaks form the old one
    d_ptr->m_pIndividual = nullptr;

   if (!VCardUtils::mapToPerson(this, content)) {
      qWarning() << "Updating person failed";
   }
}

///Destructor
Person::~Person()
{
   //Unregister itself from the D-Pointer list
   d_ptr->m_lParents.removeAll(this);

   if (d_ptr->m_lParents.isEmpty()) {
      d_ptr->setParent(nullptr);
      delete d_ptr;
   }
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

UsageStatistics* Person::usageStatistics() const
{
    if (!d_ptr->m_pStats) {
        d_ptr->m_pStats = new PersonStatistics(this);
    }

    return d_ptr->m_pStats;
}

QSharedPointer<Individual> Person::individual() const
{
    if (!d_ptr->m_pIndividual)
        d_ptr->m_pIndividual = Individual::getIndividual(const_cast<Person*>(this));

    return d_ptr->m_pIndividual;
}

ContactMethod* Person::lastUsedContactMethod() const
{
    return individual()->lastUsedContactMethod();
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
    return individual()->timelineModel();
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
   d_ptr->photoChanged();
}

///Set the formatted name (display name)
void Person::setFormattedName(const QString& name)
{
   d_ptr->m_FormattedName = name;
   setObjectName(name);
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
    return individual()->hasProperty<&ContactMethod::isPresent>();
}

/** Return if this Person is currently used as a profile.
 *
 * @warning This is not cached and if the PhoneDirectoryModel fails to
 * deduplicate things properly it can take quite long.
 */
bool Person::isProfile() const
{
    if (individual()->phoneNumbers().isEmpty() && individual()->hasHiddenContactMethods())
        return false;

    return individual()->matchExpression([this](ContactMethod* cm) {
        return cm->isSelf() && cm->contact()->d_ptr == d_ptr;
    });
}

///Return if one of the ContactMethod is tracked
bool Person::isTracked() const
{
    return individual()->hasProperty<&ContactMethod::isTracked>();
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
   return individual()->lastUsedTime();
}

///Return if one of the ContactMethod support presence
bool Person::supportPresence() const
{
    return individual()->hasProperty<&ContactMethod::supportPresence>();
}

///Return true if there is a change one if the account can be used to reach that person
bool Person::isReachable() const
{
    return individual()->hasProperty<&ContactMethod::isReachable>();
}

bool Person::hasBeenCalled() const
{
    return individual()->matchExpression([this](ContactMethod* cm) {
        return cm->callCount();
    });
}

bool Person::canCall() const
{
    return individual()->matchExpression([this](ContactMethod* cm) {
        return cm->canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
}

bool Person::canVideoCall() const
{
    return individual()->matchExpression([this](ContactMethod* cm) {
        return cm->canVideoCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
}

bool Person::canSendTexts() const
{
    return individual()->matchExpression([this](ContactMethod* cm) {
        return cm->canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
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
        case Media::Media::Type::TEXT: {
            const auto pn = individual()->phoneNumbers();
            for (ContactMethod* cm : qAsConst(pn)) {
                if (cm->textRecording() && !cm->textRecording()->isEmpty())
                    return true;
            }

            return false;
        }
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
                individual()->forAllNumbers([&unread](ContactMethod* cm) {
                    if (auto rec = cm->textRecording())
                        unread += rec->unreadCount();
                });
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
void PersonPrivate::slotPresenceChanged()
{
    presenceChanged(qobject_cast<ContactMethod*>(sender()));
    changed();
}

///Callback when one of the phone number presence change
void PersonPrivate::slotTrackedChanged()
{
    trackedChanged(qobject_cast<ContactMethod*>(sender()));
    changed();
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

    if (individual()->lastUsedContactMethod() && lastUsedTime() > c->lastUsedTime()) {
        emit c->lastUsedTimeChanged(individual()->lastUsedContactMethod()->lastUsed());
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
void Person::addAddress(Address* addr)
{
    emit addressesAboutToChange();
    d_ptr->m_lAddresses << addr;
    emit addressesChanged();
}

/// Returns the addresses associated with the person.
QList<Address*> Person::addresses() const
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
    return d_ptr->m_lCustomAttributes.values(name);
}

/// Return if the person has a (or more) custom field called `name`
bool Person::hasCustomField(const QByteArray& name) const
{
    return d_ptr->m_lCustomAttributes.contains(name);
}

///Add custom fields for contact profiles
void Person::addCustomField(const QByteArray& key, const QByteArray& value)
{
    d_ptr->m_lCustomAttributes.insert(key, value);
}

/// Remove a specific instance of a custom field
bool Person::removeCustomField(const QByteArray& key, const QByteArray& value)
{
    return d_ptr->m_lCustomAttributes.remove(key, value);
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
    maker.startVCard(QStringLiteral("2.1"));
    maker.addProperty(VCardUtils::Property::UID, uid());
    maker.addProperty(VCardUtils::Property::NAME, (secondName()
                                                    + VCardUtils::Delimiter::SEPARATOR_TOKEN
                                                    + firstName()));
    maker.addProperty(VCardUtils::Property::FORMATTED_NAME, formattedName());
    maker.addProperty(VCardUtils::Property::ORGANIZATION, organization());

    maker.addEmail(QStringLiteral("PREF"), preferredEmail());

    const auto pn = individual()->phoneNumbers();

    for (ContactMethod* phone : qAsConst(pn)) {
        QString uri = phone->uri();
        // in the case of a RingID, we want to make sure that the uri contains "ring:" so that the user
        // can tell it is a RING number and not some other hash
        if (phone->uri().protocolHint() == URI::ProtocolHint::RING)
            uri = phone->uri().full();
        maker.addContactMethod(phone->category()->name(), uri);
    }

    for (Address* addr : qAsConst(d_ptr->m_lAddresses)) {
        maker.addAddress(addr);
    }

    for (auto i = d_ptr->m_lCustomAttributes.constBegin(); i != d_ptr->m_lCustomAttributes.constEnd(); i++) {
        maker.addProperty(i.key(), i.value());
    }

    for (Account* acc : qAsConst(accounts)) {
        maker.addProperty(VCardUtils::Property::X_RINGACCOUNT, acc->id());
    }

    maker.addPhoto(GlobalInstances::pixmapManipulator().toByteArray(photo()));
    return maker.endVCard();
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
