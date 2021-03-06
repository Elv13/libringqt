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
#include "contactmethod.h"

//Qt
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>

//Ring daemon
#include "dbus/configurationmanager.h"

//Ring
#include "individualdirectory.h"
#include "person.h"
#include "session.h"
#include "account.h"
#include "media/recordingmodel.h"
#include "private/person_p.h"
#include "private/contactmethod_p.h"
#include "call.h"
#include "private/notificationstatemodel_p.h"
#include "availableaccountmodel.h"
#include "dbus/presencemanager.h"
#include "numbercategorymodel.h"
#include "bookmarkmodel.h"
#include "private/numbercategorymodel_p.h"
#include "collections/numbercategory.h"
#include "certificate.h"
#include "accountmodel.h"
#include "individual.h"
#include "certificatemodel.h"
#include "eventmodel.h"
#include "libcard/eventaggregate.h"
#include "libcard/private/eventmodel_p.h"
#include "media/textrecording.h"
#include "mime.h"
#include "usagestatistics.h"
#include "globalinstances.h"
#include "callmodel.h"
#include "pendingcontactrequestmodel.h"
#include "interfaces/pixmapmanipulatori.h"

//Private
#include "private/individualdirectory_p.h"
#include "private/textrecording_p.h"

class UsageStatisticsPrivate
{
public:
    int          m_TotalSeconds  { 0   };  ///< cummulated usage in number of seconds
    int          m_LastWeekCount { 0   }; ///< XXX: not documented, not clear
    int          m_LastTrimCount { 0   }; ///< XXX: not documented, not clear
    time_t       m_LastUsed      { 0   };        ///< last usage time
    bool         m_HaveCalled    {false};    ///< has object been called? (used for call type object)
    QList<Call*> m_lCalls               ;
    QList<Call*> m_lActiveCalls         ;
    QList<Call*> m_lInitCalls           ;

    int m_lEventType[enum_class_size<Event::EventCategory>()] {0};

    // Mutators
    void setHaveCalled();

    /// \brief Update usage using a time range.
    ///
    /// All values are updated using given <tt>[start, stop]</tt> time range.
    /// \a start and \a stop are given in seconds.
    ///
    /// \param start starting time of usage
    /// \param stop ending time of usage, must be greater than \a start
    /// \param c The type of event
    void update(time_t start, time_t stop, Event::EventCategory c);

    /// \brief Use this method to update lastUsed time by a new time only if sooner.
    ///
    /// \return \a true if the update has been effective.
    bool setLastUsed(time_t new_time);

    void append(UsageStatistics* other);
};

void ContactMethodPrivate::callAdded(Call* call)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->callAdded(call);
}

void ContactMethodPrivate::changed()
{
   foreach (ContactMethod* n, m_lParents)
      emit n->changed();
}

void ContactMethodPrivate::mediaAvailabilityChanged()
{
    const bool a = q_ptr->hasActiveCall();

    for (auto n : qAsConst(m_lParents)) {
        emit n->mediaAvailabilityChanged();
        emit n->hasActiveCallChanged(a);
    }
}

void ContactMethodPrivate::presentChanged(bool s)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->presentChanged(s);
}

void ContactMethodPrivate::presenceMessageChanged(const QString& status)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->presenceMessageChanged(status);
}

void ContactMethodPrivate::trackedChanged(bool t)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->trackedChanged(t);
}

void ContactMethodPrivate::primaryNameChanged(const QString& name)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->primaryNameChanged(name);
}

void ContactMethodPrivate::rebased(ContactMethod* other)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->rebased(other);
}

void ContactMethodPrivate::registeredNameSet(const QString& name)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->registeredNameSet(name);
}

void ContactMethodPrivate::bookmarkedChanged(bool value)
{
   foreach (ContactMethod* n, m_lParents)
      emit n->bookmarkedChanged(value);
}

void ContactMethodPrivate::accountChanged()
{
    foreach (ContactMethod* n, m_lParents)
      emit n->accountChanged(m_pAccount);
}

void ContactMethodPrivate::contactChanged(Person* p1,Person* p2)
{
    foreach (ContactMethod* n, m_lParents)
      emit n->contactChanged(p1, p2);
}

const ContactMethod* ContactMethod::BLANK()
{
    static auto instance = []{
        auto instance = new ContactMethod(QString(), NumberCategoryModel::other());
        instance->d_ptr->m_Type = ContactMethod::Type::BLANK;
        return instance;
    }();
    return instance;
}

ContactMethodPrivate::ContactMethodPrivate(const URI& uri, NumberCategory* cat, ContactMethod::Type st, ContactMethod* q) :
   m_Uri(uri),m_pCategory(cat), m_Type(st),m_pAccount(nullptr),
   m_pUsageStats(new UsageStatistics(q)), m_pOriginal(q), q_ptr(q)
{}

ContactMethodPrivate::~ContactMethodPrivate()
{
    delete m_pUsageStats;
}


ContactMethodPrivate* ContactMethod::d() const
{
    return d_ptr;
}

///Constructor
ContactMethod::ContactMethod(const URI& number, NumberCategory* cat, Type st) : ItemBase(Session::instance()->individualDirectory()),
d_ptr(new ContactMethodPrivate(number,cat,st,this))
{
   setObjectName(d_ptr->m_Uri);
   d_ptr->m_hasType = cat != NumberCategoryModel::other();
   if (d_ptr->m_hasType) {
      Session::instance()->numberCategoryModel()->d_ptr->registerNumber(this);
   }
   d_ptr->m_lParents << this;
}

ContactMethod::~ContactMethod()
{

   d_ptr->m_lParents.remove(this);

   if (d_ptr->m_lParents.isEmpty())
      delete d_ptr;
}

///Return if this number presence is being tracked
bool ContactMethod::isTracked() const
{
    if (isSelf() && account()->protocol() == Account::Protocol::RING)
        return true;

    //If the number doesn't support it, ignore the flag
    return supportPresence() && d_ptr->m_Tracked;
}

///Is this number present
bool ContactMethod::isPresent() const
{
    return d_ptr->m_Tracked && d_ptr->m_Present;
}

///Is this CM correspond to its account "self"
bool ContactMethod::isSelf() const
{
    // Use the d_ptr just in case it was deduplicated
    return account() && account()->contactMethod()->d_ptr == d_ptr;
}

///This number presence status string
QString ContactMethod::presenceMessage() const
{
   return d_ptr->m_PresentMessage;
}

///Return the number
const URI& ContactMethod::uri() const {
   // Set the Scheme now that it is know to avoid prorating ::NONE
    if (d_ptr->m_pAccount && d_ptr->m_Uri.schemeType() == URI::SchemeType::NONE) {
        switch(d_ptr->m_pAccount->protocol()) {
            case Account::Protocol::RING:
                d_ptr->m_Uri.setSchemeType(URI::SchemeType::RING);
                break;
            case Account::Protocol::SIP:
                d_ptr->m_Uri.setSchemeType(d_ptr->m_pAccount->isTlsEnabled() ?
                    URI::SchemeType::SIPS :
                    URI::SchemeType::SIP
                );
                break;
            case Account::Protocol::COUNT__:
                Q_ASSERT(false);
        }
    }

    return d_ptr->m_Uri;
}

///Return the phone number type
NumberCategory* ContactMethod::category() const {
   return d_ptr->m_pCategory ;
}

///Return this number associated account, if any
Account* ContactMethod::account() const
{
   return d_ptr->m_pAccount;
}

///Return this number associated contact, if any
Person* ContactMethod::contact() const
{
   return d_ptr->m_pPerson;
}

///Return when this number was last used
time_t ContactMethod::lastUsed() const
{
   return d_ptr->m_pUsageStats->lastUsed();
}

UsageStatistics* ContactMethod::usageStatistics() const
{
   return d_ptr->m_pUsageStats;
}

ContactRequest* ContactMethod::request() const
{
    if (!account())
        return nullptr;

    return account()->pendingContactRequestModel()->findContactRequestFrom(this);
}

ContactMethod::ConfirmationStatus ContactMethod::confirmationStatus() const
{
    // Do not set m_ConfirmationStatus since it's status would be lost when
    // isConfirmationEnabled is toggled.
    if (!d_ptr->m_IsConfirmationEnabled)
        return ConfirmationStatus::DISABLED;

    if (d_ptr->m_ConfirmationStatus == ConfirmationStatus::NOT_APPLICABLE) {
       if (account() && account()->protocol() == Account::Protocol::RING) {
          d_ptr->m_ConfirmationStatus = ConfirmationStatus::UNCONFIRMED;
           emit const_cast<ContactMethod*>(this)->confirmationChanged(confirmationStatus());
       }
       else if ((!account()) && uri().protocolHint() == URI::ProtocolHint::RING) {
           d_ptr->m_ConfirmationStatus = ConfirmationStatus::NO_ACCOUNT;
           emit const_cast<ContactMethod*>(this)->confirmationChanged(confirmationStatus());
       }
    }

    return d_ptr->m_ConfirmationStatus;
}

bool ContactMethod::isConfirmationEnabled() const
{
    return d_ptr->m_IsConfirmationEnabled;
}

void ContactMethod::setConfirmationEnabled(bool enabled)
{
    d_ptr->m_IsConfirmationEnabled = enabled;
    emit confirmationChanged(confirmationStatus());
}

bool ContactMethod::sendContactRequest() const
{
    if (!account())
        return false;

    if (account()->protocol() != Account::Protocol::RING)
        return false;

    return account()->sendContactRequest(this);
}

///Set this number default account
void ContactMethod::setAccount(Account* account)
{
   if (account == d_ptr->m_pAccount)
      return;

   //Account cannot be changed for the Account own contact method
   if (this->account() && type() == ContactMethod::Type::ACCOUNT)
      return;

   //Add the statistics
   if (account && !d_ptr->m_pAccount && account->usageStatistics())
       account->usageStatistics()->d_ptr->append(d_ptr->m_pUsageStats);

   d_ptr->m_pAccount = account;

   //The sha1 is no longer valid
   d_ptr->m_Sha1.clear();

   if (d_ptr->m_pAccount)
      connect (d_ptr->m_pAccount,SIGNAL(destroyed(QObject*)), d_ptr,SLOT(slotAccountDestroyed(QObject*)));

   //Track Ring contacts by default
   if (this->protocolHint() == URI::ProtocolHint::RING || this->protocolHint() == URI::ProtocolHint::RING_USERNAME)
       setTracked(true);

   if (account && account->protocol() == Account::Protocol::RING
    && (d_ptr->m_ConfirmationStatus == ConfirmationStatus::NOT_APPLICABLE
    || d_ptr->m_ConfirmationStatus == ConfirmationStatus::NO_ACCOUNT))
      d_ptr->m_ConfirmationStatus = ConfirmationStatus::UNCONFIRMED;

   d_ptr->changed();
   d_ptr->accountChanged();
}

///Set this number contact
void ContactMethod::setPerson(Person* contact)
{

   Person* old = d_ptr->m_pPerson;
   if (d_ptr->m_pPerson == contact)
      return;

   // This *will* have bad side effects. Better just call
   // `IndividualDirectory::getNumber()` to get a new ContactMethod.
   if (d_ptr->m_pPerson && contact && contact->uid() != d_ptr->m_pPerson->uid()) {
      qWarning() << "WARNING: There's already a contact, this is a bug"
         << contact << d_ptr->m_pPerson;

      d_ptr->m_pPerson->individual()->removePhoneNumber(this);

   }

   d_ptr->m_pPerson = contact;

   //The sha1 is no longer valid
   d_ptr->m_Sha1.clear();

   if (contact && d_ptr->m_Type != ContactMethod::Type::TEMPORARY) {
      contact->individual()->registerContactMethod(this);
      Session::instance()->individualDirectory()->d_ptr->indexNumber(this,d_ptr->m_hNames.keys()+QStringList(contact->formattedName()));
      d_ptr->m_PrimaryName_cache = contact->formattedName();
      d_ptr->primaryNameChanged(d_ptr->m_PrimaryName_cache);

      connect(contact, &Person::formattedNameChanged, d_ptr,
         &ContactMethodPrivate::slotNameChanged);

      connect(contact,SIGNAL(rebased(Person*)), d_ptr,SLOT(slotContactRebased(Person*)));
   }
   d_ptr->changed();

   d_ptr->contactChanged(contact, old);
}

void ContactMethod::setCategory(NumberCategory* cat)
{
   if (cat == d_ptr->m_pCategory) return;
   if (d_ptr->m_hasType)
      Session::instance()->numberCategoryModel()->d_ptr->unregisterNumber(this);
   d_ptr->m_hasType = cat != NumberCategoryModel::other();
   d_ptr->m_pCategory = cat;
   if (d_ptr->m_hasType)
      Session::instance()->numberCategoryModel()->d_ptr->registerNumber(this);
   d_ptr->changed();
}

void ContactMethod::setBookmarked(bool bookmarked )
{
   d_ptr->m_IsBookmark = bookmarked;

   if (bookmarked)
      Session::instance()->bookmarkModel()->addBookmark(this);
   else
      Session::instance()->bookmarkModel()->removeBookmark(this);

   changed();
   bookmarkedChanged(bookmarked);
}

///Force an Uid on this number (instead of hash)
void ContactMethodPrivate::setUid(const QString& uri)
{
   m_Uid = uri;
}

///Attempt to change the number type
bool ContactMethodPrivate::setType(ContactMethod::Type t)
{
   if (m_Type == ContactMethod::Type::BLANK)
      return false;

   if (t == m_Type)
      return true;

   if (q_ptr->account() && t == ContactMethod::Type::ACCOUNT) {
      // Avoid the possible ambiguity since the correct value is currently know
      switch(q_ptr->account()->protocol()) {
         case Account::Protocol::SIP:
            m_Uri.setSchemeType(URI::SchemeType::SIP);
            break;
         case Account::Protocol::RING:
            m_Uri.setSchemeType(URI::SchemeType::RING);
            break;
         case Account::Protocol::COUNT__:
             Q_ASSERT(false);
      }

      if (q_ptr->account()->supportPresenceSubscribe()) {
         m_Tracked = true; //The daemon will init the tracker itself
         trackedChanged(true);
      }
      m_Type = t;
      return true;
   }
   return false;
}

///Update the last time used only if t is more recent than m_LastUsed
void ContactMethodPrivate::setLastUsed(time_t t)
{
    if (m_pUsageStats->d_ptr->setLastUsed(t))
       foreach (ContactMethod* n, m_lParents)
          emit n->lastUsedChanged(t);
}

///Set if this number is tracking presence information
void ContactMethod::setTracked(bool track)
{
   if (track != d_ptr->m_Tracked) { //Subscribe only once
      //You can't subscribe without account
      if (track && !d_ptr->m_pAccount) return;
      d_ptr->m_Tracked = track;
      PresenceManager::instance().subscribeBuddy(d_ptr->m_pAccount->id(),
                                                 uri().format(URI::Section::CHEVRONS |
                                                              URI::Section::SCHEME |
                                                              URI::Section::USER_INFO |
                                                              URI::Section::HOSTNAME),
                                                 track);
      d_ptr->changed();
      d_ptr->trackedChanged(track);
   }
}

/// Set the registered name
void ContactMethodPrivate::setRegisteredName(const QString& registeredName)
{
   if (registeredName.isEmpty() || registeredName == m_RegisteredName)
      return;

   // If this happens, better create another ContactMethod manually to avoid
   // all the corner cases. Doing this, for example, allows to keep track of
   // the name in the private index of unique names kept by the
   // IndividualDirectory
   if (!m_RegisteredName.isEmpty()) {
      qWarning() << "A registered name is already set for this ContactMethod"
         << m_RegisteredName << registeredName;
      return;
   }

   m_RegisteredName = registeredName;
   primaryNameChanged(q_ptr->primaryName());
   registeredNameSet(registeredName);
   changed();
}

///Allow phonedirectorymodel to change presence status
void ContactMethodPrivate::setPresent(bool present)
{
    if (q_ptr->isSelf()) {
        //TODO
    }
    else if (m_Present != present) {
        m_Present = present;
        presentChanged(present);
    }
}

void ContactMethod::setPresenceMessage(const QString& message)
{
   if (d_ptr->m_PresentMessage != message) {
      d_ptr->m_PresentMessage = message;
      d_ptr->presenceMessageChanged(message);
   }
}

///Return the current type of the number
ContactMethod::Type ContactMethod::type() const
{
   return d_ptr->m_Type;
}

///Return the number of calls from this number
int ContactMethod::callCount() const
{
   return d_ptr->m_pUsageStats->d_ptr->m_lEventType[(int)Event::EventCategory::CALL];
}

uint ContactMethod::weekCount() const
{
   return d_ptr->m_pUsageStats->lastWeekCount();
}

uint ContactMethod::trimCount() const
{
   return d_ptr->m_pUsageStats->lastTrimCount();
}

bool ContactMethod::haveCalled() const
{
   return d_ptr->m_pUsageStats->hasBeenCalled();
}

///Best bet for this person real name
QString ContactMethod::primaryName() const
{
   if (type() == ContactMethod::Type::TEMPORARY) {
      return registeredName().isEmpty() ? URI(uri()) : registeredName();
   }

   //Compute the primary name
   if (d_ptr->m_PrimaryName_cache.isEmpty()) {
      QString ret;
      if (d_ptr->m_hNames.size() == 1)
         ret =  d_ptr->m_hNames.constBegin().key();
      else {
         QString toReturn = registeredName().isEmpty() ? URI(uri()) : registeredName();
         QPair<int, time_t> max = {0, 0};

         for (QHash<QString,QPair<int, time_t>>::const_iterator i = d_ptr->m_hNames.constBegin(); i != d_ptr->m_hNames.constEnd(); ++i) {
             if (this->protocolHint() == URI::ProtocolHint::RING &&
                     i.value().second > max.second) {
                 max.second = i.value().second;
                 toReturn = i.key  ();
             }
             else if (i.value().first > max.first) {
                 max.first = i.value().first;
                 toReturn = i.key  ();
             }
         }
         ret = toReturn;
      }
      d_ptr->m_PrimaryName_cache = ret;
      d_ptr->primaryNameChanged(d_ptr->m_PrimaryName_cache);
   }
   //Fallback: Use the URI
   if (d_ptr->m_PrimaryName_cache.isEmpty()) {
      return registeredName().isEmpty() ? URI(uri()) : registeredName();
   }

   //Return the cached primaryname
   return d_ptr->m_PrimaryName_cache;
}

/// Returns the best possible name for the contact method in order of priority:
/// 1. Formatted name of associated Person
/// 2. Registered name (for RING type)
/// 3. "primary name" (could be a SIP display name received during a call or uri as fallback)
QString ContactMethod::bestName() const
{
    QString name;
    if (contact() and !contact()->formattedName().isEmpty())
        name = contact()->formattedName();
    else if (not registeredName().isEmpty())
        name = registeredName();
    else
        name = primaryName();

    return name;
}

/// Returns the registered username otherwise returns an empty QString
QString ContactMethod::registeredName() const
{
   return d_ptr->m_RegisteredName;
}

/// Returns if this contact method is currently involved in a recording
bool ContactMethod::isRecording() const
{
   for (auto c : qAsConst(d_ptr->m_pUsageStats->d_ptr->m_lActiveCalls)) {
      //TODO if video recording is ever supported, extend the if
      if (c->isRecording(Media::Media::Type::AUDIO, Media::Media::Direction::OUT))
         return true;
   }
   return false;
}

/// Returns if this contact method currently has ongoing calls
bool ContactMethod::hasActiveCall() const
{
   return !d_ptr->m_pUsageStats->d_ptr->m_lActiveCalls.isEmpty();
}

/// Returns if this contact method currently has ringing or initializing calls
bool ContactMethod::hasInitCall() const
{
    return !d_ptr->m_pUsageStats->d_ptr->m_lInitCalls.isEmpty();
}

/// Returns if this contact method currently has active video streams
bool ContactMethod::hasActiveVideo() const
{
   for (auto c : qAsConst(d_ptr->m_pUsageStats->d_ptr->m_lActiveCalls)) {
      if (c->videoRenderer())
         return true;
   }
   return false;
}

/// Returns the registered name if available, otherwise returns the uri
QString ContactMethod::bestId() const
{
   return registeredName().isEmpty() ? URI(uri()) : registeredName();
}

/**
 * Return if this ContactMethod pointer is the `master` one or a deduplicated
 * proxy.
 *
 * Contact methods can be merged over time to avoid both the memory overhead
 * and diverging statistics / presence / messaging. As explained in `merge()`,
 * the old pointers are kept alive as they are proxies to the real object
 * (d_ptr). This accessor helps some algorithm detect if they can safely get
 * rid of this CM as a "better" one already exists (assuming the track all CMs).
 */
bool ContactMethod::isDuplicate() const
{
   return d_ptr->m_pOriginal != this;
}

///Is this number bookmarked
bool ContactMethod::isBookmarked() const
{
   return d_ptr->m_IsBookmark;
}

///If this number could (theoretically) support presence status
bool ContactMethod::supportPresence() const
{
   //Without an account, presence is impossible
   if (!d_ptr->m_pAccount)
      return false;
   //The account also have to support it
   if (!d_ptr->m_pAccount->supportPresenceSubscribe())
       return false;

   //In the end, it all come down to this, is the number tracked
   return true;
}

///Proxy accessor to the category icon
QVariant ContactMethod::icon() const
{
   return category()->icon(isTracked(),isPresent());
}

///The number of seconds spent with the URI (from history)
int ContactMethod::totalSpentTime() const
{
    return d_ptr->m_pUsageStats->totalSeconds();
}

///Return this number unique identifier (hash)
QString ContactMethod::uid() const
{
   return d_ptr->m_Uid.isEmpty()?toHash():d_ptr->m_Uid;
}

///Return the URI protocol hint
URI::ProtocolHint ContactMethod::protocolHint() const
{
   return d_ptr->m_Uri.protocolHint();
}

///Create a SHA1 hash identifying this contact method
QByteArray ContactMethod::sha1() const
{
   if (d_ptr->m_Sha1.isEmpty()) {
      QCryptographicHash hash(QCryptographicHash::Sha1);
      hash.addData(toHash().toLatin1());

      //Create a reproducible key for this file
      d_ptr->m_Sha1 = hash.result().toHex();
   }
   return d_ptr->m_Sha1;
}

QJsonObject ContactMethod::toJson() const
{
    QJsonObject o;

    o[QStringLiteral("uri" )] = uri ();
    o[QStringLiteral("sha1")] = QString(sha1());

    if (auto a = account())
        o[QStringLiteral("accountId")] = QString(a->id());

    if (auto p = contact())
        o[QStringLiteral("personUID")] = QString(p->uid());

    return o;
}

///Return all calls from this number
const QList<Call*> ContactMethod::calls() const
{
   return d_ptr->m_pUsageStats->d_ptr->m_lCalls;
}

Call* ContactMethod::firstOutgoingCall() const
{
    const auto calls = d_ptr->m_pUsageStats->d_ptr->m_lActiveCalls;

    for (Call* c : calls)
        if (c->direction() == Call::Direction::OUTGOING)
            return c;

    return nullptr;
}

Call* ContactMethod::firstActiveCall() const
{
    return Session::instance()->callModel()->firstActiveCall(const_cast<ContactMethod*>(this));
}

QHash<QString,QPair<int, time_t>> ContactMethod::alternativeNames() const
{
   return d_ptr->m_hNames;
}

QVariant ContactMethod::roleData(int role) const
{
   QVariant cat;

   auto lastCall = d_ptr->m_pUsageStats->d_ptr->m_lCalls.isEmpty()
      ? nullptr : d_ptr->m_pUsageStats->d_ptr->m_lCalls.constLast();

   switch (role) {
      case static_cast<int>(Ring::Role::Name):
      case static_cast<int>(Call::Role::Name):
         cat = bestName();
         break;
      case Qt::ToolTipRole:
         cat = presenceMessage();
         break;
      case static_cast<int>(Ring::Role::URI):
      case static_cast<int>(Role::Uri):
         cat = uri();
         break;
      case static_cast<int>(Role::RegisteredName):
          return registeredName();
      case Qt::DisplayRole:
      case Qt::EditRole:
      case static_cast<int>(Ring::Role::Number):
      case static_cast<int>(Call::Role::Number):
         cat = bestId();
         break;
      case Qt::DecorationRole:
         return GlobalInstances::pixmapManipulator().decorationRole(this);
      case static_cast<int>(Call::Role::Direction):
         cat = cat = !lastCall ? QVariant() : QVariant::fromValue(lastCall->direction());
         break;
      case static_cast<int>(Ring::Role::LastUsed):
      case static_cast<int>(Call::Role::Date):
         cat = d_ptr->m_pUsageStats->lastUsed() <= 0 ? QVariant() : QDateTime::fromTime_t(d_ptr->m_pUsageStats->lastUsed());
         break;
      case static_cast<int>(Ring::Role::Length):
      case static_cast<int>(Call::Role::Length):
         cat = cat = !lastCall ? QVariant() : lastCall->length();
         break;
      case static_cast<int>(Ring::Role::FormattedLastUsed):
      case static_cast<int>(Call::Role::FormattedDate):
      case static_cast<int>(Call::Role::FuzzyDate):
         cat = HistoryTimeCategoryModel::timeToHistoryCategory(d_ptr->m_pUsageStats->lastUsed());
         break;
      case static_cast<int>(Ring::Role::IndexedLastUsed):
         return QVariant(static_cast<int>(HistoryTimeCategoryModel::timeToHistoryConst(d_ptr->m_pUsageStats->lastUsed())));
      case static_cast<int>(Call::Role::HasAVRecording):
         cat = cat = !lastCall ? QVariant() : lastCall->isAVRecording();
         break;
      case static_cast<int>(Call::Role::ContactMethod):
      case static_cast<int>(Ring::Role::Object):
         cat = QVariant::fromValue(const_cast<ContactMethod*>(this));
         break;
      case static_cast<int>(Ring::Role::ObjectType):
         cat = QVariant::fromValue(Ring::ObjectType::ContactMethod);
         break;
      case static_cast<int>(Call::Role::IsBookmark):
      case static_cast<int>(Ring::Role::IsBookmarked):
         cat = d_ptr->m_IsBookmark;
         break;
      case static_cast<int>(Call::Role::Filter):
         cat = uri()+primaryName();
         break;
      case static_cast<int>(Ring::Role::IsPresent):
      case static_cast<int>(Call::Role::IsPresent):
         cat = isPresent();
         break;
      case static_cast<int>(Call::Role::Photo):
         if (contact())
            cat = contact()->photo();
         break;
      case static_cast<int>(Role::CategoryIcon):
         if (category())
            cat = d_ptr->m_pCategory->icon(isTracked(), isPresent());
         break;
      case static_cast<int>(Role::CategoryName):
         if (category())
            cat = d_ptr->m_pCategory->name();
         break;
      case static_cast<int>(Role::IsReachable):
          return isReachable();
      case static_cast<int>(Role::CanCall):
          return canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
      case static_cast<int>(Role::CanVideoCall):
          return canVideoCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
      case static_cast<int>(Role::CanSendTexts):
          return canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
      case static_cast<int>(Role::TotalCallCount):
          return callCount();
      case static_cast<int>(Role::TotalEventCount):
          return Session::instance()->eventModel()->d_ptr->events(this).size();
      case static_cast<int>(Role::TotalMessageCount):
          if (auto rec = textRecording())
            cat = rec->sentCount() + rec->receivedCount();
         else
            cat = 0;
         break;
      case static_cast<int>(Role::Filter):
          return QStringLiteral("%1//%2//%3//%4//%5//%6")
            .arg(bestName())
            .arg(primaryName())
            .arg(uri())
            .arg(account() ? account()->alias() : QString())
            .arg(contact() ? contact()->formattedName() : QString())
            .arg(registeredName());
      case static_cast<int>(Call::Role::LifeCycleState):
         return QVariant::fromValue(Call::LifeCycleState::FINISHED);
      case static_cast<int>(Ring::Role::UnreadTextMessageCount):
         if (hasTextRecordings())
            cat = textRecording()->unreadCount();
         else
            cat = 0;
         break;
      case static_cast<int>(Ring::Role::IsRecording):
         cat = isRecording();
         break;
      case static_cast<int>(Ring::Role::HasActiveCall):
         cat = hasActiveCall();
         break;
      case static_cast<int>(Ring::Role::HasActiveVideo):
         cat = hasActiveVideo();
         break;
      case static_cast<int>(ContactMethod::Role::Type):
         return QVariant::fromValue(type());
      case static_cast<int>(ContactMethod::Role::CategoryKey):
         return d_ptr->m_pCategory->key();
      case static_cast<int>(ContactMethod::Role::Account):
         return QVariant::fromValue(account());
   }
   return cat;
}

/// Keep a single method to set the roles instead of copy/pasting in 5 models
bool ContactMethod::setRoleData(const QVariant &value, int role)
{
    // Those values are immutable once the CM becomes "real"
    if (type() == ContactMethod::Type::TEMPORARY) {
        switch(role) {
            case static_cast<int>(Ring::Role::URI):
            case static_cast<int>(Role::Uri):
                qobject_cast<TemporaryContactMethod*>(this)->setUri(value.toString());
                return true;
        }
    }

    switch(role) {
        case static_cast<int>(Ring::Role::IsBookmarked):
            setBookmarked(value.toBool());
            return true;
        case static_cast<int>(Role::CategoryKey):
            if (auto cat = Session::instance()->numberCategoryModel()->forKey(value.toInt())) {
                setCategory(cat);
                return true;
            }
            break;
        case static_cast<int>(Role::Account):
            if (type() == ContactMethod::Type::ACCOUNT)
                return false;

            if (value.canConvert<Account*>()) {
                setAccount(qvariant_cast<Account*>(value));
                return true;
            }
            else if (value.canConvert<QModelIndex>()) {
                const auto idx = qvariant_cast<QModelIndex>(value);
                if (idx.model() == Session::instance()->accountModel()) {
                    setAccount(Session::instance()->accountModel()->getAccountByModelIndex(idx));
                    return true;
                }
            }
            break;

    };

    return false;
}

Individual* ContactMethod::individual() const
{
    Q_ASSERT(type() != ContactMethod::Type::BLANK);
    if (contact())
        return contact()->individual();

    if (!d_ptr->m_pIndividual) {
        auto p = Individual::getIndividual(const_cast<ContactMethod*>(this));
        d_ptr->m_pIndividual = p;
        return p;
    }

    return d_ptr->m_pIndividual;
}

QSharedPointer<EventAggregate> ContactMethod::eventAggregate() const
{
    if (!d_ptr->m_pEventAggregate) {
        d_ptr->m_pEventAggregate = EventAggregate::build(
            const_cast<ContactMethod*>(this)
        );
    }

    return d_ptr->m_pEventAggregate;
}

/**
 * The oldest event this contact method attended.
 *
 * It exists to prevent an O(N) lookup everytime there is a need to build
 * the list of events.
 *
 * Once the list is required, it is cached, but that's only done when there is
 * a need for it.
 */
QSharedPointer<Event> ContactMethod::oldestEvent() const
{
    return Session::instance()->eventModel()->oldest(this);
}

/**
 * Get the newest event attended by this CM.
 *
 * @see oldestEvent
 */
QSharedPointer<Event> ContactMethod::newestEvent() const
{
    return Session::instance()->eventModel()->newest(this);
}

QMimeData* ContactMethod::mimePayload() const
{
   return RingMimes::payload(nullptr, this, nullptr);
}

///Add a call to the call list, notify listener
void ContactMethod::addCall(Call* call)
{
   if (!call) return;

   d_ptr->m_Type = ContactMethod::Type::USED;
   d_ptr->m_pUsageStats->d_ptr->m_lCalls << call;

   // call setLastUsed first so that we emit lastUsedChanged()
   auto time = call->startTimeStamp();
   d_ptr->setLastUsed(time);

   //Update the contact method statistics
   d_ptr->addTimeRange(call->startTimeStamp(), call->stopTimeStamp(), Event::EventCategory::CALL);
   if (d_ptr->m_pAccount)
      d_ptr->m_pAccount->usageStatistics()->d_ptr->update(call->startTimeStamp(), call->stopTimeStamp(), Event::EventCategory::CALL);

   if (call->direction() == Call::Direction::OUTGOING) {
      d_ptr->m_pUsageStats->d_ptr->setHaveCalled();
      if (d_ptr->m_pAccount)
         d_ptr->m_pAccount->usageStatistics()->d_ptr->setHaveCalled();
   }

   if (d_ptr->m_pAccount)
       d_ptr->m_pAccount->usageStatistics()->d_ptr->setLastUsed(time);

   d_ptr->callAdded(call);
   d_ptr->changed();
}

///Generate an unique representation of this number
QString ContactMethod::toHash() const
{
   QString uristr;

   switch(uri().protocolHint()) {
      case URI::ProtocolHint::RING         :
         //There is no point in keeping the full URI, a Ring hash is unique
         uristr = uri().userinfo();
         break;
      case URI::ProtocolHint::RING_USERNAME:
      case URI::ProtocolHint::SIP_OTHER:
      case URI::ProtocolHint::IP       :
      case URI::ProtocolHint::SIP_HOST :
         //Some URI have port number in them. They have to be stripped prior to the hash creation
         uristr = uri().format(
            URI::Section::CHEVRONS  |
            URI::Section::SCHEME    |
            URI::Section::USER_INFO |
            URI::Section::HOSTNAME
         );
         break;
   }

   return QStringLiteral("%1///%2///%3")
      .arg(
         uristr
      )
      .arg(
         account()?account()->id():QString()
      )
      .arg(
         contact()?contact()->uid():QString()
      );
}

QByteArray ContactMethod::toAttendee(const QString& name) const
{
    const auto hash = toHash();

    QByteArray ret;

    QTextStream s(&ret);

    s << "ATTENDEE";

    s << ";CN=" << (name.isEmpty() ? bestName() : name);

    if (account())
        s << ";X_RING_ACCOUNT=" << account()->id();

    if (contact())
        s << ";CONTACT=" << contact()->uid();

    if (account())
        s << ";X_RING_HASH=" << hash;

    s << ':' << uri() << '\n';

    //TODO make sure the line is no more than 75 chars

    return ret;
}

///Increment name counter and update indexes
void ContactMethod::incrementAlternativeName(const QString& name, const time_t lastUsed)
{
   const bool needReIndexing = !d_ptr->m_hNames[name].first;
   if (d_ptr->m_hNames[name].second < lastUsed)
      d_ptr->m_hNames[name].second = lastUsed;
   d_ptr->m_hNames[name].first++;

   if (needReIndexing && d_ptr->m_Type != ContactMethod::Type::TEMPORARY) {
      Session::instance()->individualDirectory()->d_ptr->indexNumber(this,d_ptr->m_hNames.keys()+(d_ptr->m_pPerson?(QStringList(d_ptr->m_pPerson->formattedName())):QStringList()));
      //Invalid m_PrimaryName_cache
      if (!d_ptr->m_pPerson) {
         d_ptr->m_PrimaryName_cache.clear();
         d_ptr->primaryNameChanged(primaryName());
      }
   }

   d_ptr->changed();
}

void ContactMethodPrivate::slotAccountDestroyed(QObject* o)
{
   if (o == m_pAccount) {
      m_pAccount = nullptr;
      accountChanged();
   }
}

/**
 * When the ContactMethod contact is merged with another one, the phone number
 * data might be replaced, like the preferred name.
 */
void ContactMethodPrivate::slotContactRebased(Person* other)
{
   m_PrimaryName_cache = other->formattedName();
   primaryNameChanged(m_PrimaryName_cache);
   q_ptr->setPerson(other);

   changed();
   rebased(q_ptr);
}


void ContactMethodPrivate::slotNameChanged()
{
   if (!q_ptr->contact())
      m_PrimaryName_cache.clear();
   else
      m_PrimaryName_cache = q_ptr->contact()->formattedName();

   primaryNameChanged(q_ptr->primaryName());

   changed();
}

/**
 * Merge two phone number to share the same data. This avoid having to change
 * pointers all over the place. The ContactMethod objects remain intact, the
 * IndividualDirectory will replace the old references, but existing ones will
 * keep working.
 */
bool ContactMethod::merge(ContactMethod* other)
{
   if ((!other) || other == this || other->d_ptr == d_ptr)
      return false;

   //This is invalid, those are different numbers
   if (account() && other->account() && account() != other->account())
      return false;

   if (d_ptr->m_Type == ContactMethod::Type::TEMPORARY)
      return false;

   if (other->d_ptr->m_Type == ContactMethod::Type::TEMPORARY)
      return false;

   //TODO Check if the merge is valid

   //TODO Merge the alternative names

   if (d_ptr->m_Tracked)
      other->d_ptr->m_Tracked = true;

   if (d_ptr->m_Present)
      other->d_ptr->m_Present = true;

   // Merge the events
   Session::instance()->eventModel()->d_ptr->mergeEvents(other, this);

   if (contact() && !other->contact())
      other->setPerson(contact());

   if ((!registeredName().isEmpty()) && other->registeredName().isEmpty())
      other->d_ptr->setRegisteredName(registeredName());

   // Avoid losing chat data
   if (d_ptr->m_pTextRecording && d_ptr->m_pTextRecording->size() > 0 &&
    d_ptr->m_pTextRecording != other->d_ptr->m_pTextRecording) {
       other->d_ptr->addAlternativeTextRecording(d_ptr->m_pTextRecording);
   }

   const QString oldName = primaryName();

   ContactMethodPrivate* currentD = d_ptr;

   //Replace the D-Pointer
   for (auto p : qAsConst(currentD->m_lParents)) {
      d_ptr->m_lParents << p;
      p->d_ptr = other->d_ptr;
   }

   //In case the URI is different, take the longest and most precise
   //TODO keep a log of all URI used
   if (currentD->m_Uri.size() > other->d_ptr->m_Uri.size()) {
      other->d_ptr->m_lOtherURIs << other->d_ptr->m_Uri;
      other->d_ptr->m_Uri = currentD->m_Uri;
   }
   else
      other->d_ptr->m_lOtherURIs << currentD->m_Uri;

   d_ptr->changed();
   d_ptr->rebased(other);

   if (oldName != primaryName())
      d_ptr->primaryNameChanged(primaryName());

   delete currentD;

   return true;
}

bool ContactMethod::operator==(ContactMethod* other)
{
   return other && this->d_ptr== other->d_ptr;
}

bool ContactMethod::operator==(const ContactMethod* other) const
{
   return other && this->d_ptr== other->d_ptr;
}

bool ContactMethod::operator==(ContactMethod& other)
{
   return this->d_ptr== other.d_ptr;
}

bool ContactMethod::operator==(const ContactMethod& other) const
{
   return this->d_ptr== other.d_ptr;
}

Media::TextRecording* ContactMethod::textRecording() const
{
    if (!d_ptr->m_pTextRecording) {
        d_ptr->m_pTextRecording = Session::instance()->recordingModel()->createTextRecording(this);

        for (auto n : qAsConst(d_ptr->m_lParents))
            emit n->textRecordingAdded(d_ptr->m_pTextRecording);
    }

    return d_ptr->m_pTextRecording;
}

bool ContactMethod::hasTextRecordings() const
{
    return d_ptr->m_pTextRecording ||
        Session::instance()->recordingModel()->hasTextRecordings(this);
}

bool ContactMethod::isReachable() const
{
   auto m = Session::instance()->accountModel();

   const bool hasSip   = m->isSipSupported  ();
   const bool hasIP2IP = m->isIP2IPSupported();
   const bool hasRing  = m->isRingSupported ();

   switch (protocolHint()) {
      case URI::ProtocolHint::SIP_HOST :
      case URI::ProtocolHint::IP       :
         if (hasIP2IP)
            return true;
         //no break
         [[clang::fallthrough]];
      case URI::ProtocolHint::SIP_OTHER:
         if (hasSip)
            return true;
         break;
      case URI::ProtocolHint::RING:
      case URI::ProtocolHint::RING_USERNAME:
         if (hasRing)
            return true;
         break;
   }

   return false;
}

Certificate* ContactMethod::certificate() const
{
    if (!d_ptr->m_pCertificate && protocolHint() == URI::ProtocolHint::RING)
        d_ptr->m_pCertificate = CertificateModel::instance().getCertificateFromId(uri().userinfo(), account());

    if (d_ptr->m_pCertificate && !d_ptr->m_pCertificate->contactMethod())
        d_ptr->m_pCertificate->setContactMethod(const_cast<ContactMethod*>(this));

    return d_ptr->m_pCertificate;
}

void ContactMethodPrivate::setCertificate(Certificate* certificate)
{
    m_pCertificate = certificate;
    if (!certificate->contactMethod())
      certificate->setContactMethod(q_ptr);
}

void ContactMethodPrivate::setTextRecording(Media::TextRecording* r)
{
   m_pTextRecording = r;
}

/**
 * This list will have entries in 2 case:
 *
 *  * A message was sent between the time the CM was created from a name and
 *   name service lookup finished (or timeout). In that case, the deduplication
 *   will register an alternative TextRecording. Note that in the future, they
 *   could be merged into the primary one to avoid the issue persisting across
 *   restart
 *  * When a contact was added after the first communication. In this case,
 *    there will be a hashed recording based on both the SHA1 of the CM and
 *    the SHA1 of the CM+Person. They could also be merged so the issue
 *    doesn't persist across restart.
 *
 */
QVector<Media::TextRecording*> ContactMethod::alternativeTextRecordings() const
{
    return d_ptr->m_lAltTR;
}

void ContactMethodPrivate::addAlternativeTextRecording(Media::TextRecording* recording)
{
    Q_ASSERT(recording != m_pTextRecording);

    m_lAltTR << recording;
    foreach (ContactMethod* n, m_lParents)
        emit n->textRecordingAdded(recording);
}

/**
 * Detect every case where the capacity of a contact method to send (valid)
 * text messages is affected.
 */
ContactMethod::MediaAvailailityStatus ContactMethod::canSendTexts(bool warn) const
{
    auto selectedAccount = account() ? account() :  Session::instance()->availableAccountModel()->currentDefaultAccount(this);

    // Texts might still fail, but there is no reliable way to know, assume the
    // best.
    if (hasActiveCall())
        return ContactMethod::MediaAvailailityStatus::AVAILABLE;

    if (!selectedAccount) {
        if (warn)
            qWarning() << "Failed to send an offline text message. No account "
                "available for this contactmethod";
        return ContactMethod::MediaAvailailityStatus::NO_ACCOUNT;
    }

    if (selectedAccount->registrationState() != Account::RegistrationState::READY) {
        if (warn)
            qWarning() << "Failed to send an offline text message. The account"
            " isn't registered";
        return ContactMethod::MediaAvailailityStatus::ACCOUNT_DOWN;
    }

    if (selectedAccount->protocol() != Account::Protocol::RING) {
        if (warn)
            qWarning() << "Failed to send an offline message because a SIP account"
                " was used";
        return ContactMethod::MediaAvailailityStatus::NO_CALL;
    }

    return ContactMethod::MediaAvailailityStatus::AVAILABLE;
}

ContactMethod::MediaAvailailityStatus ContactMethod::canCall() const
{
    if (hasActiveCall())
        return ContactMethod::MediaAvailailityStatus::AVAILABLE;

    auto selectedAccount = account();

    if (!selectedAccount)
        if (!(selectedAccount =  Session::instance()->availableAccountModel()->currentDefaultAccount(this)))
            return ContactMethod::MediaAvailailityStatus::NO_ACCOUNT;

    if (!selectedAccount->canCall())
        return ContactMethod::MediaAvailailityStatus::ACCOUNT_DOWN;

    return ContactMethod::MediaAvailailityStatus::AVAILABLE; //TODO find other accounts
}

ContactMethod::MediaAvailailityStatus ContactMethod::canVideoCall() const
{
    const auto callStatus = canCall();

    if (callStatus != ContactMethod::MediaAvailailityStatus::AVAILABLE)
        return callStatus;

    auto selectedAccount = account();

    if (!selectedAccount) {
        if (!(selectedAccount =  Session::instance()->availableAccountModel()->currentDefaultAccount(this)))
            return ContactMethod::MediaAvailailityStatus::NO_ACCOUNT;
    }

    if (!selectedAccount->canVideoCall())
        return ContactMethod::MediaAvailailityStatus::CODECS;

    return ContactMethod::MediaAvailailityStatus::AVAILABLE; //TODO find other accounts
}

/// Detect if the CM is generally available to do useful things.
bool ContactMethod::isAvailable() const
{
    // warning: This is also used for the account own CM, it cannot choose other
    // accounts.
    return canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE ||
        canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
}

bool ContactMethod::sendOfflineTextMessage(const QMap<QString,QString>& payloads)
{
    auto selectedAccount = account() ? account() :  Session::instance()->availableAccountModel()->currentDefaultAccount(this);

    if (canSendTexts(true) != ContactMethod::MediaAvailailityStatus::AVAILABLE)
        return false;

    // This is too easy to cause accidentally. Better just fix it an hope for
    // the best. The problem is that when no scheme is present, SIP used to be
    // assumed. While this is no longer safe, changing the code is more risky
    // than this band-aid.
    if (uri().schemeType() != URI::SchemeType::RING) {
        d_ptr->m_Uri.setSchemeType(URI::SchemeType::RING);

        qWarning() << "An URI has a scheme type unsupported by its account, "
            "a fix was applied" << uri();
    }

    const uint64_t id = ConfigurationManager::instance().sendTextMessage(
        selectedAccount->id(),
        uri().format(URI::Section::SCHEME|URI::Section::USER_INFO|URI::Section::HOSTNAME),
        payloads
    );

    textRecording()->d_ptr->insertNewMessage(payloads, this, Media::Media::Direction::OUT, id);

    return true;
}

/**
 * Track the CM active calls.
 *
 * Previous attempts at building a timeline tried to track CMs, Calls and
 * Persons. While this worked with enough effort, it was hard to maintain,
 * grew in complexity over time and was always rather fragile as new events
 * were added.
 *
 * Tracking active calls (and text messages) in the CM allows it to unify the
 * timeline events into a single object.
 */
void ContactMethodPrivate::addActiveCall(Call* c)
{
    const int wasEmpty = m_pUsageStats->d_ptr->m_lActiveCalls.isEmpty();

    m_pUsageStats->d_ptr->m_lActiveCalls << c;

    changed();

    removeInitCall(c);

    if (wasEmpty != m_pUsageStats->d_ptr->m_lActiveCalls.isEmpty())
        mediaAvailabilityChanged();
}

void ContactMethodPrivate::removeActiveCall(Call* c)
{
    const int wasEmpty = m_pUsageStats->d_ptr->m_lActiveCalls.isEmpty();

    m_pUsageStats->d_ptr->m_lActiveCalls.removeAll(c);

    changed();

    if (wasEmpty != m_pUsageStats->d_ptr->m_lActiveCalls.isEmpty())
        mediaAvailabilityChanged();
}

void ContactMethodPrivate::addInitCall(Call* c)
{
    m_pUsageStats->d_ptr->m_lInitCalls << c;

    changed();

    foreach (ContactMethod* n, m_lParents)
      emit n->hasInitCallChanged(q_ptr->hasInitCall());
}

void ContactMethodPrivate::removeInitCall(Call* c)
{
    m_pUsageStats->d_ptr->m_lInitCalls.removeAll(c);

    changed();

    foreach (ContactMethod* n, m_lParents)
      emit n->hasInitCallChanged(q_ptr->hasInitCall());
}

void ContactMethod::setNotificationPolicy(NotificationPolicy p)
{
    d_ptr->m_NotificationPolicy = p;

    NotificationStateModel::instance()->setState(this, p);

    d_ptr->changed();
}

ContactMethod::NotificationPolicy ContactMethod::notificationPolicy() const
{
    return d_ptr->m_NotificationPolicy;
}

/************************************************************************************
 *                                                                                  *
 *                             Temporary phone number                               *
 *                                                                                  *
 ***********************************************************************************/

///Constructor
TemporaryContactMethod::TemporaryContactMethod(const ContactMethod* number) :
   ContactMethod(QString(),NumberCategoryModel::other(),ContactMethod::Type::TEMPORARY),d_ptr(nullptr)
{
   if (number) {
      setPerson(number->contact());
      setAccount(number->account());
   }
}

void TemporaryContactMethod::setUri(const URI& u)
{
   if (ContactMethod::d_ptr->m_Type != ContactMethod::Type::TEMPORARY) {
      qWarning() << "Trying to edit an uri after saving" << uri() << u;
      return;
   }

   if (u != ContactMethod::d_ptr->m_Uri)
      ContactMethod::d_ptr->m_RegisteredName.clear();

   ContactMethod::d_ptr->m_Uri = u;
   ContactMethod::d_ptr->m_Uri.resetChecks();

   //The sha1 is no longer valid
   ContactMethod::d_ptr->m_Sha1.clear();
   ContactMethod::d_ptr->changed();
}

void TemporaryContactMethod::setRegisteredName(const QString& regName)
{
   ContactMethod::d_ptr->m_PrimaryName_cache.clear();
   ContactMethod::d_ptr->m_RegisteredName = regName;
   ContactMethod::d_ptr->primaryNameChanged(primaryName());
}

QVariant TemporaryContactMethod::icon() const
{
   return QVariant(); //TODO use the pixmapmanipulator to get a better icon
}

/************************************************************************************
 *                                                                                  *
 *                                  Statistics                                      *
 *                                                                                  *
 ***********************************************************************************/

UsageStatistics::UsageStatistics(QObject* parent) : QObject(nullptr), d_ptr(new UsageStatisticsPrivate)
{
    Q_UNUSED(parent); //Done manually
}

UsageStatistics::~UsageStatistics()
{
    delete d_ptr;
}

int UsageStatistics::totalSeconds() const
{
    return d_ptr->m_TotalSeconds;
}

int UsageStatistics::lastWeekCount() const
{
    return d_ptr->m_LastWeekCount;
}

int UsageStatistics::lastTrimCount() const
{
    return d_ptr->m_LastTrimCount;
}

time_t UsageStatistics::lastUsed() const
{
    return d_ptr->m_LastUsed;
}

bool UsageStatistics::hasBeenCalled() const
{
    return d_ptr->m_HaveCalled;
}

//Mutators

void UsageStatisticsPrivate::setHaveCalled()
{
    m_HaveCalled = true;
}

/// \brief Update usage using a time range.
///
/// All values are updated using given <tt>[start, stop]</tt> time range.
/// \a start and \a stop are given in seconds.
///
/// \param start starting time of usage
/// \param stop ending time of usage, must be greater than \a start
void UsageStatisticsPrivate::update(time_t start, time_t stop, Event::EventCategory c)
{
    // Remove garbage events, the bugs are elsewhere and they will wreck the
    // stat if they are used.
    if ((!start) || (!stop))
        return;

    if (start > stop)
        return;

    setLastUsed(start);
    Q_ASSERT(stop >= start);
    m_TotalSeconds += stop - start;

    m_lEventType[(int)c]++;

    time_t now;
    ::time(&now);
    if (now - 3600*24*7 < stop)
        ++m_LastWeekCount;
    if (now - 3600*24*7*15 < stop)
        ++m_LastTrimCount;
}

void ContactMethodPrivate::addTimeRange(time_t start, time_t end, Event::EventCategory c)
{
    if (end > m_pUsageStats->lastUsed()) {
        setLastUsed(end);
    }
    m_pUsageStats->d_ptr->update(start, end, c);

}

/// \brief Use this method to update lastUsed time by a new time only if sooner.
///
/// \return \a true if the update has been effective.
bool UsageStatisticsPrivate::setLastUsed(time_t new_time)
{
    if (new_time > m_LastUsed) {
        m_LastUsed = new_time;
        return true;
    }

    return false;
}

void UsageStatisticsPrivate::append(UsageStatistics* rhs)
{
    m_LastWeekCount += rhs->d_ptr->m_LastWeekCount;
    m_LastTrimCount += rhs->d_ptr->m_LastTrimCount;
    setLastUsed(rhs->d_ptr->m_LastUsed);
}

Q_DECLARE_METATYPE(QList<Call*>)
