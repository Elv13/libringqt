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
#include "phonedirectorymodel.h"

//Qt
#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>

//DRing
#include <account_const.h>

//Ring
#include "contactmethod.h"
#include "call.h"
#include "uri.h"
#include "account.h"
#include "person.h"
#include "accountmodel.h"
#include "numbercategory.h"
#include "numbercategorymodel.h"
#include "collectioninterface.h"
#include "dbus/presencemanager.h"
#include "globalinstances.h"
#include "private/contactmethod_p.h"
#include "interfaces/pixmapmanipulatori.h"
#include "personmodel.h"
#include "dbus/configurationmanager.h"
#include "media/recordingmodel.h"
#include "media/textrecording.h"

//Private
#include "private/phonedirectorymodel_p.h"

class ContactMethodDirectoryPrivate
{
public:
   int m_Index          {-1};
   int m_PopularityIndex{-1};
};

PhoneDirectoryModelPrivate::PhoneDirectoryModelPrivate(PhoneDirectoryModel* parent) : QObject(parent), q_ptr(parent),
m_CallWithAccount(false),m_pPopularModel(nullptr)
{
    connect(&NameDirectory::instance(), &NameDirectory::registeredNameFound, this, &PhoneDirectoryModelPrivate::slotRegisteredNameFound);
}

PhoneDirectoryModel::PhoneDirectoryModel(QObject* parent) :
   QAbstractTableModel(parent?parent:QCoreApplication::instance()), d_ptr(new PhoneDirectoryModelPrivate(this))
{
   setObjectName(QStringLiteral("PhoneDirectoryModel"));
   connect(&PresenceManager::instance(),SIGNAL(newBuddyNotification(QString,QString,bool,QString)),d_ptr.data(),
           SLOT(slotNewBuddySubscription(QString,QString,bool,QString)));
}

PhoneDirectoryModel::~PhoneDirectoryModel()
{
   QList<NumberWrapper*> vals = d_ptr->m_hNumbersByNames.values();
   //Used by indexes
   d_ptr->m_hNumbersByNames.clear();
   d_ptr->m_lSortedNames.clear();
   while (vals.size()) {
      NumberWrapper* w = vals[0];
      for (int i = 0; i < w->numbers.size(); i++) {
         if (w->numbers[i]->dir_d_ptr) {
            delete w->numbers[i]->dir_d_ptr;
            w->numbers[i]->dir_d_ptr = nullptr;
         }
      }
      vals.removeAt(0);
      delete w;
   }

   //Used by auto completion
   vals = d_ptr->m_hSortedNumbers.values();
   d_ptr->m_hSortedNumbers.clear();
   d_ptr->m_hDirectory.clear();
   while (vals.size()) {
      NumberWrapper* w = vals[0];
      vals.removeAt(0);
      for (int i = 0; i < w->numbers.size(); i++) {
         if (w->numbers[i]->dir_d_ptr) {
            delete w->numbers[i]->dir_d_ptr;
            w->numbers[i]->dir_d_ptr = nullptr;
         }
      }
      delete w;
   }
}

PhoneDirectoryModel& PhoneDirectoryModel::instance()
{
   static auto instance = new PhoneDirectoryModel;
   return *instance;
}

/// To keep in sync with ContactMethod::roleData
QHash<int,QByteArray> PhoneDirectoryModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        QHash<int, QByteArray>::const_iterator i;
        for (i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); ++i)
            roles[i.key()] = i.value();

        roles[ static_cast<int>(Call::Role::Name) ] = "name";
        roles[ static_cast<int>(Call::Role::Number) ] = "number";
        roles[ static_cast<int>(Call::Role::Direction) ] = "direction";
        roles[ static_cast<int>(Call::Role::Date) ] = "date";
        roles[ static_cast<int>(Call::Role::Length) ] = "length";
        roles[ static_cast<int>(Call::Role::FormattedDate) ] = "formattedDate";
        roles[ static_cast<int>(Call::Role::FuzzyDate) ] = "fuzzyDate";
        roles[ static_cast<int>(Call::Role::HasAVRecording) ] = "hasAVRecording";
        roles[ static_cast<int>(Call::Role::ContactMethod) ] = "contactMethod";
        roles[ static_cast<int>(Call::Role::IsBookmark) ] = "isBookmark";
        roles[ static_cast<int>(Call::Role::Filter) ] = "filter";
        roles[ static_cast<int>(Call::Role::IsPresent) ] = "isPresent";
        roles[ static_cast<int>(Call::Role::Photo) ] = "photo";
        roles[ static_cast<int>(Call::Role::LifeCycleState) ] = "lifeCycleState";
        roles[ static_cast<int>(Ring::Role::UnreadTextMessageCount) ] = "unreadTextMessageCount";
        roles[ static_cast<int>(Ring::Role::IsRecording) ] = "isRecording";
        roles[ static_cast<int>(Ring::Role::HasActiveCall) ] = "hasActiveCall";
        roles[ static_cast<int>(Ring::Role::HasActiveVideo) ] = "hasActiveVideo";
        roles[ static_cast<int>(ContactMethod::Role::Object) ] = "object";
        roles[ static_cast<int>(ContactMethod::Role::Uri) ] = "uri";
        roles[ static_cast<int>(ContactMethod::Role::CategoryIcon) ] = "categoryIcon";
        roles[ static_cast<int>(ContactMethod::Role::CategoryName) ] = "categoryName";
        roles[ static_cast<int>(ContactMethod::Role::IsReachable) ] = "isReachable";

    }

    return roles;
}

QVariant PhoneDirectoryModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid() || index.row() >= d_ptr->m_lNumbers.size()) return QVariant();
   const ContactMethod* number = d_ptr->m_lNumbers[index.row()];
   switch (static_cast<PhoneDirectoryModelPrivate::Columns>(index.column())) {
      case PhoneDirectoryModelPrivate::Columns::URI:
         switch (role) {
            case Qt::DisplayRole:
               return number->uri();
            case Qt::DecorationRole :
               return GlobalInstances::pixmapManipulator().decorationRole(number);
            case (int) Role::Object:
               return QVariant::fromValue(const_cast<ContactMethod*>(number));
            case Qt::ToolTipRole:
                return number->isDuplicate() ? QStringLiteral("Is duplicate") : QString();
            default:
                return number->roleData(role);
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::TYPE:
         switch (role) {
            case Qt::DisplayRole:
               return number->category()->name();
            case Qt::DecorationRole:
               return number->icon();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::CONTACT:
         switch (role) {
            case Qt::DisplayRole:
               return number->contact()?number->contact()->formattedName():QVariant();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::IS_SELF:
         switch (role) {
            case Qt::DisplayRole:
               return number->isSelf();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::ACCOUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->account()?number->account()->id():QVariant();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::STATE:
         switch (role) {
            case Qt::DisplayRole:
               return (int)number->type();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::CALL_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->callCount();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::LAST_USED:
         switch (role) {
            case Qt::DisplayRole:
               return (int)number->lastUsed();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::NAME_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->alternativeNames().size();
               break;
            case Qt::ToolTipRole: {
               QString out = QStringLiteral("<table>");
               QHashIterator<QString, QPair<int, time_t>> iter(number->alternativeNames());
               while (iter.hasNext()) {
                  iter.next();
                  out += QStringLiteral("<tr><td>%1</td><td>%2</td></tr>").arg(iter.value().first).arg(iter.key());
               }
               out += QLatin1String("</table>");
               return out;
            }
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::TOTAL_SECONDS:
         switch (role) {
            case Qt::DisplayRole:
               return number->totalSpentTime();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::WEEK_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->weekCount();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::TRIM_COUNT:
         switch (role) {
            case Qt::DisplayRole:
               return number->trimCount();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::HAVE_CALLED:
         switch (role) {
            case Qt::DisplayRole:
               return number->haveCalled();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::POPULARITY_INDEX:
         switch (role) {
            case Qt::DisplayRole:
               return number->dir_d_ptr->m_PopularityIndex;
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::BOOKMARED:
         switch (role) {
            case Qt::CheckStateRole:
               return number->isBookmarked()?Qt::Checked:Qt::Unchecked;
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::HAS_CERTIFICATE:
         switch (role) {
            case Qt::CheckStateRole:
               return number->certificate()?Qt::Checked:Qt::Unchecked;
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::TRACKED:
         switch (role) {
            case Qt::CheckStateRole:
               if (number->account() && number->account()->supportPresenceSubscribe())
                  return number->isTracked()?Qt::Checked:Qt::Unchecked;
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::PRESENT:
         switch (role) {
            case Qt::CheckStateRole:
               return number->isPresent()?Qt::Checked:Qt::Unchecked;
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::PRESENCE_MESSAGE:
         switch (role) {
            case Qt::DisplayRole: {
               if ((index.column() == static_cast<int>(PhoneDirectoryModelPrivate::Columns::TRACKED)
                  || static_cast<int>(PhoneDirectoryModelPrivate::Columns::PRESENT))
                  && number->account() && (!number->account()->supportPresenceSubscribe())) {
                  return tr("This account does not support presence tracking");
               }
               else if (!number->account())
                  return tr("No associated account");
               else
                  return number->presenceMessage();
            }
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::UID:
         switch (role) {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
               return number->uid();
         }
         break;
      case PhoneDirectoryModelPrivate::Columns::REGISTERED_NAME:
         switch (role) {
            case Qt::DisplayRole:
            case Qt::ToolTipRole:
               return number->registeredName();
         }
         break;
   }
   return QVariant();
}

int PhoneDirectoryModel::rowCount(const QModelIndex& parent ) const
{
   return parent.isValid() ? 0 : d_ptr->m_lNumbers.size();
}

int PhoneDirectoryModel::columnCount(const QModelIndex& parent ) const
{
   return parent.isValid() ? 0 : 21;
}

Qt::ItemFlags PhoneDirectoryModel::flags(const QModelIndex& index ) const
{
   const ContactMethod* number = d_ptr->m_lNumbers[index.row()];

   // Mark the "old" duplicate as disabled. They are now zombies acting as
   // proxies to the real contact methods.
   if (number->isDuplicate())
      return Qt::NoItemFlags;

   const bool enabled = !((index.column() == static_cast<int>(PhoneDirectoryModelPrivate::Columns::TRACKED)
      || static_cast<int>(PhoneDirectoryModelPrivate::Columns::PRESENT))
      && number->account() && (!number->account()->supportPresenceSubscribe()));

   return Qt::ItemIsEnabled
      | Qt::ItemIsSelectable
      | (index.column() == static_cast<int>(PhoneDirectoryModelPrivate::Columns::TRACKED)&&enabled?Qt::ItemIsUserCheckable:Qt::NoItemFlags);
}

///This model is read and for debug purpose
bool PhoneDirectoryModel::setData(const QModelIndex& index, const QVariant &value, int role )
{
   ContactMethod* number = d_ptr->m_lNumbers[index.row()];
   if (static_cast<PhoneDirectoryModelPrivate::Columns>(index.column())==PhoneDirectoryModelPrivate::Columns::TRACKED) {
      if (role == Qt::CheckStateRole && number) {
         number->setTracked(value.toBool());
      }
   }
   return false;
}

QVariant PhoneDirectoryModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
   if (orientation == Qt::Vertical)
      return {};

   static const QString headers[] = {
      tr("URI"),
      tr("Type"),
      tr("Person"),
      tr("Is self"),
      tr("Account"),
      tr("State"),
      tr("Call count"),
      tr("Week count"),
      tr("Trimester count"),
      tr("Have Called"),
      tr("Last used"),
      tr("Name_count"),
      tr("Total (in seconds)"),
      tr("Popularity_index"),
      tr("Bookmarked"),
      tr("Tracked"),
      tr("Has certificate"),
      tr("Present"),
      tr("Presence message"),
      tr("Uid"),
      tr("Registered name")
   };

   if (role == Qt::DisplayRole) return headers[section];
   return QVariant();
}

/**
 * This helper method make sure that number without an account get registered
 * correctly with their alternate URIs. In case there is an obvious duplication,
 * it will try to merge both numbers.
 */
void PhoneDirectoryModelPrivate::setAccount(ContactMethod* number, Account* account ) {
   const URI& strippedUri = number->uri();
   const bool hasAtSign = strippedUri.hasHostname();
   number->setAccount(account);

   if (!hasAtSign) {
      const QString extendedUri = strippedUri+'@'+account->hostname();
      NumberWrapper* wrap = m_hDirectory.value(extendedUri);

      //Let make sure none is created in the future for nothing
      if (!wrap) {
         //It won't be a duplicate as none exist for this URI
         wrap = new NumberWrapper(extendedUri);
         m_hDirectory    [extendedUri] = wrap;
         m_hSortedNumbers[extendedUri] = wrap;
         wrap->numbers << number;

      }
      else {
         //After all this, it is possible the number is now a duplicate
         foreach(ContactMethod* n, wrap->numbers) {
            if (n != number && n->account() && n->account() == number->account()) {
               number->merge(n);
            }
         }
      }
      wrap->numbers << number;

   }

   // for RingIDs, once we set an account, we should perform (another) name lookup, in case the
   // account has a different name server set from the default
   if (number->uri().protocolHint() == URI::ProtocolHint::RING)
      NameDirectory::instance().lookupAddress(number->account(), QString(), number->uri().userinfo());
}

///Add new information to existing numbers and try to merge
ContactMethod* PhoneDirectoryModelPrivate::fillDetails(NumberWrapper* wrap, const URI& strippedUri, Account* account, Person* contact, const QString& type)
{
    //TODO pick the best URI
    //TODO the account hostname change corner case
    //TODO search for account that has the same hostname as the URI

    if (!wrap)
        return nullptr;

    for (auto number : qAsConst(wrap->numbers)) {

        //BEGIN Check if contact can be set


        //Check if the contact is compatible
        bool hasCompatiblePerson = (contact && (
            (!number->contact())
        || (
                (number->contact()->uid() == contact->uid())
            && number->contact() != contact
        )
        )) || !contact;

        // Corner case: If the number is part of an account and it has been
        // moved from a profile to another *AND* `contact` is a placeholder,
        // assume nobody don't care and they are fine if the historical profile
        // is ignored.
        if ((!hasCompatiblePerson)
          && account == number->account()
          && number->isSelf()
          && contact
          && contact->isPlaceHolder()
          && account->contactMethod()->contact()) {
            contact = account->contactMethod()->contact();
            hasCompatiblePerson = true;
        }

        if (!hasCompatiblePerson)
            continue;

        //Check if the URI match
        const bool hasCompatibleURI = number->uri() == strippedUri || (number->uri().hasHostname()?(
            /* Has hostname */
                strippedUri == number->uri()
                /* Something with an hostname can be used with IP2IP */
            || (account && account->isIp2ip())
        ) : ( /* Has no hostname */
                number->account() && number->uri()+'@'+number->account()->hostname() == strippedUri
        ));

        if (!hasCompatibleURI)
            continue;

        //Check if the account is compatible
        const bool hasCompatibleAccount = ((!account)
            || (!number->account())
            /* Direct match, this is always valid */
            || (account == number->account())
            /* IP2IP is a special case */ //TODO support DHT here
            || (
                    account->isIp2ip()
                    && strippedUri.hasHostname()
                ));

        if (!hasCompatibleAccount)
            continue;

        //TODO the Display name could be used to influence the choice
        //It would need to ignore all possible translated values of unknown
        //and only be available when another information match

        //If everything match, complete the candidate
        auto cat = number->category() ? number->category() :
            NumberCategoryModel::instance().getCategory(type);

        if (contact)
            number->setPerson   ( contact         );

        if (cat)
            number->setCategory ( cat             );

        if (account)
            setAccount          ( number, account );

        return number;
    }

    return nullptr;
}

/// Detect some common invalid entries
bool PhoneDirectoryModel::ensureValidity(const URI& uri, Account* a)
{
    if (Q_UNLIKELY(uri.isEmpty())) {
        qWarning() << "Trying to create a contact method with an empty URI" << a;
        return false;
    }

    if (a && uri.schemeType() != URI::SchemeType::NONE) {
        switch(uri.schemeType()) {
            case URI::SchemeType::NONE:
            case URI::SchemeType::COUNT__:
                break;
            case URI::SchemeType::SIP:
            case URI::SchemeType::SIPS:
                if (Q_UNLIKELY(a->protocol() == Account::Protocol::RING)) {
                    qWarning() << "Trying to create a SIP contact method with a RING account" << uri;
                    return false;
                }
                break;
            case URI::SchemeType::RING:
                if (Q_UNLIKELY(a->protocol() == Account::Protocol::SIP)) {
                    qWarning() << "Trying to create a RING contact method with a SIP account" << uri;
                    return false;
                }
        }
    }

    return true;
}

/**
 * This version of getNumber() try to get a phone number with a contact from an URI and account
 * It will also try to attach an account to existing numbers. This is not 100% reliable, but
 * it is correct often enough to do it.
 */
ContactMethod* PhoneDirectoryModel::getNumber(const URI& uri, Account* account, const QString& type)
{
   return getNumber(uri,nullptr,account,type);
}

///Return/create a number when no information is available
ContactMethod* PhoneDirectoryModel::getNumber(const URI& uri, const QString& type)
{
   if (const auto wrap = d_ptr->m_hDirectory.value(uri)) {
      ContactMethod* nb = wrap->numbers[0];
      if ((nb->category() == NumberCategoryModel::other()) && (!type.isEmpty())) {
         nb->setCategory(NumberCategoryModel::instance().getCategory(type));
      }
      return nb;
   }

   //Too bad, lets create one
   ContactMethod* number = new ContactMethod(uri, NumberCategoryModel::instance().getCategory(type));
   number->dir_d_ptr = new ContactMethodDirectoryPrivate;
   number->dir_d_ptr->m_Index = d_ptr->m_lNumbers.size();

   beginInsertRows({}, d_ptr->m_lNumbers.size(), d_ptr->m_lNumbers.size());
   d_ptr->m_lNumbers << number;
   endInsertRows();

   connect(number,SIGNAL(callAdded(Call*)),d_ptr.data(),SLOT(slotCallAdded(Call*)));
   connect(number,SIGNAL(changed()),d_ptr.data(),SLOT(slotChanged()));
   connect(number,&ContactMethod::lastUsedChanged,d_ptr.data(), &PhoneDirectoryModelPrivate::slotLastUsedChanged);
   connect(number,&ContactMethod::contactChanged ,d_ptr.data(), &PhoneDirectoryModelPrivate::slotContactChanged);
   connect(number,&ContactMethod::rebased ,d_ptr.data(), &PhoneDirectoryModelPrivate::slotContactMethodMerged);

   const QString hn = number->uri().hostname();

   auto wrap = new NumberWrapper(uri);
   d_ptr->m_hDirectory[uri] = wrap;
   d_ptr->m_hSortedNumbers[uri] = wrap;
   wrap->numbers << number;

   // perform a username lookup for new CM with RingID
   if (number->uri().protocolHint() == URI::ProtocolHint::RING)
      NameDirectory::instance().lookupAddress(number->account(), QString(), number->uri().userinfo());

   return number;
}

/** An URI can take many forms and it's impossible to predict how and when each
 * variation will be used. Any change in the daemon can cause new ones to
 * be created.
 *
 * To mitigate the consequences of this, register a couple aliases in the
 * directory.
 *
 */
void PhoneDirectoryModelPrivate::registerAlternateNames(ContactMethod* number, Account* account,  const URI& uri, const URI& extendedUri)
{
    // The hostname can be empty for IP2IP accounts
    if (!account)
        return;

    const auto userInfo = uri.userinfo();
    const auto hostname = account->hostname();

    if (userInfo.isEmpty())
        return;

    NumberWrapper* wrap = m_hDirectory.value(extendedUri);

    // Also add its alternative URI, it should be safe to do
    if (hostname.isEmpty()) {

        // Also check if it hasn't been created by setAccount
        if ((!wrap) && (!m_hDirectory.value(extendedUri))) {
            wrap = new NumberWrapper(extendedUri);
            m_hDirectory    [extendedUri] = wrap;
            m_hSortedNumbers[extendedUri] = wrap;
        }

        if (wrap)
            wrap->numbers << number;
        else
            qWarning() << "PhoneDirectoryModel: code path should not be reached, wrap is nullptr";

        // The part below can't be true, no need to process it
        return;
    }

    Q_ASSERT(number->account() == account);

    // Add the minimal URI too. This is useful in case getNumber() is called
    // with the userInfo and no account.
    auto wrap3 = m_hDirectory.value(userInfo);

    if (!wrap3) {
        wrap3 = new NumberWrapper(userInfo);
        m_hDirectory    [userInfo] = wrap3;
        m_hSortedNumbers[userInfo] = wrap3;
    }

    wrap3->numbers << number;
}

///Create a number when a more information is available duplicated ones
ContactMethod* PhoneDirectoryModel::getNumber(const URI& uri, Person* contact, Account* account, const QString& type)
{
   //One cause of duplicate is when something like ring:foo happen on SIP accounts.
   ensureValidity(uri, account);

   //See if the number is already loaded using 3 common URI equivalent variants
   NumberWrapper* wrap  = d_ptr->m_hDirectory.value(uri);
   NumberWrapper* wrap2 = nullptr;
   NumberWrapper* wrap3 = nullptr;

   //Check if the URI is complete or short
   const bool hasAtSign = uri.hasHostname();

    // Append the account hostname, this should always reach the same destination
    // as long as the server implementation isn't buggy.
    const auto extendedUri = (hasAtSign || !account) ? uri : URI(QStringLiteral("%1@%2")
       .arg(uri)
       .arg(account->hostname()));

   //Try to see if there is a better candidate with a suffix (LAN only)
   if ( !hasAtSign && account ) {

      // Attempt an early candidate using the extended URI. The later
      // `confirmedCandidate2` will attempt a similar, but less likely case.
      if ((wrap2 = d_ptr->m_hDirectory.value(extendedUri))) {
         if (auto cm = d_ptr->fillDetails(wrap2, extendedUri, account, contact, type))
            return cm;
      }
   }

   //Check
   ContactMethod* confirmedCandidate = d_ptr->fillDetails(wrap,uri,account,contact,type);

   //URIs can be represented in multiple way, check if a more verbose version
   //already exist
   ContactMethod* confirmedCandidate2 = nullptr;

   //Try to use a ContactMethod with a contact when possible, work only after the
   //contact are loaded
   if (confirmedCandidate && confirmedCandidate->contact())
      confirmedCandidate2 = d_ptr->fillDetails(wrap2,uri,account,contact,type);

   ContactMethod* confirmedCandidate3 = nullptr;

   //Else, try to see if the hostname correspond to the account and flush it
   //This have to be done after the parent if as the above give "better"
   //results. It cannot be merged with wrap2 as this check only work if the
   //candidate has an account.
   if (hasAtSign && account && uri.hostname() == account->hostname()) {
     if ((wrap3 = d_ptr->m_hDirectory.value(uri.userinfo()))) {
         foreach(ContactMethod* number, wrap3->numbers) {
            if (number->account() == account) {
               if (contact && ((!number->contact()) || (contact->uid() == number->contact()->uid())))
                  number->setPerson(contact); //TODO Check all cases from fillDetails()
               //TODO add alternate URI
               confirmedCandidate3 = number;
               break;
            }
         }
     }
   }

   //If multiple ContactMethod are confirmed, then they are the same, merge them
   if (confirmedCandidate3 && (confirmedCandidate || confirmedCandidate2)) {
      confirmedCandidate3->merge(confirmedCandidate?confirmedCandidate:confirmedCandidate2);
   }
   else if (confirmedCandidate && confirmedCandidate2) {
      if (confirmedCandidate->contact() && !confirmedCandidate2->contact())
         confirmedCandidate2->merge(confirmedCandidate);
      else if (confirmedCandidate2->contact() && !confirmedCandidate->contact())
         confirmedCandidate->merge(confirmedCandidate2);
   }

   //Empirical testing resulted in this as the best return order
   //The merge may have failed either in the "if" above or in the merging code
   if (confirmedCandidate2)
      return confirmedCandidate2;
   if (confirmedCandidate)
      return confirmedCandidate;
   if (confirmedCandidate3)
      return confirmedCandidate3;

   //No better candidates were found than the original assumption, use it
   if (wrap) {
      foreach(ContactMethod* number, wrap->numbers) {
         if (((!account) || number->account() == account) && ((!contact) || ((*contact) == number->contact()) || (!number->contact()))) {
            //Assume this is valid until a smarter solution is implemented to merge both
            //For a short time, a placeholder contact and a contact can coexist, drop the placeholder
            if (contact && (!number->contact() || (contact->uid() == number->contact()->uid())))
               number->setPerson(contact);

            return number;
         }
      }
   }

   //Create the number
   ContactMethod* number = new ContactMethod(uri,NumberCategoryModel::instance().getCategory(type));
   number->dir_d_ptr = new ContactMethodDirectoryPrivate;

   number->setAccount(account);
   number->dir_d_ptr->m_Index = d_ptr->m_lNumbers.size();
   if (contact)
      number->setPerson(contact);

   connect(number,SIGNAL(callAdded(Call*)),d_ptr.data(),SLOT(slotCallAdded(Call*)));
   connect(number,SIGNAL(changed()),d_ptr.data(),SLOT(slotChanged()));
   connect(number,&ContactMethod::lastUsedChanged,d_ptr.data(), &PhoneDirectoryModelPrivate::slotLastUsedChanged);
   connect(number,&ContactMethod::contactChanged ,d_ptr.data(), &PhoneDirectoryModelPrivate::slotContactChanged );
   connect(number,&ContactMethod::rebased ,d_ptr.data(), &PhoneDirectoryModelPrivate::slotContactMethodMerged);

   if (!wrap) {
      wrap = new NumberWrapper(uri);
      d_ptr->m_hDirectory    [uri] = wrap;
      d_ptr->m_hSortedNumbers[uri] = wrap;

      //Also add its alternative URI, it should be safe to do
      d_ptr->registerAlternateNames(number, account, uri, extendedUri);
   }

   wrap->numbers << number;

   beginInsertRows({}, d_ptr->m_lNumbers.size(), d_ptr->m_lNumbers.size());
   d_ptr->m_lNumbers << number;
   endInsertRows();

   // perform a username lookup for new CM with RingID
   if (number->uri().protocolHint() == URI::ProtocolHint::RING)
      NameDirectory::instance().lookupAddress(number->account(), QString(), number->uri().userinfo());

   return number;
}

ContactMethod* PhoneDirectoryModel::fromTemporary(const TemporaryContactMethod* number)
{
   return getNumber(number->uri(),number->contact(),number->account());
}

ContactMethod* PhoneDirectoryModel::fromHash(const QString& hash)
{
   const QStringList fields = hash.split(QStringLiteral("///"));
   if (fields.size() == 3) {
      const QString uri = fields[0];
      const QByteArray acc = fields[1].toLatin1();
      Account* account = acc.isEmpty() ? nullptr : AccountModel::instance().getById(acc);
      Person* contact = PersonModel::instance().getPersonByUid(fields[2].toUtf8());
      return getNumber(uri,contact,account);
   }
   else if (fields.size() == 1) {
      //FIXME Remove someday, handle version v1.0 to v1.2.3 bookmark format
      return getNumber(fields[0]);
   }
   qDebug() << "Invalid hash" << hash;
   return nullptr;
}


/** Filter the existing CMs for an URI without flooding the model with merge
 * candidates.
 *
 * This should help reduce the number of accidental duplicates once its
 * usage spread over the code. It also reduce the boilerplate code.
 **/
ContactMethod* PhoneDirectoryModel::getExistingNumberIf(const URI& uri, const std::function<bool(const ContactMethod*)>& pred) const
{
   //See if the number is already loaded
   const NumberWrapper* w = d_ptr->m_hDirectory.value(uri);

   if (!w)
      return nullptr;

   const auto iter = std::find_if(std::begin(w->numbers), std::end(w->numbers), pred);

   return (iter != std::end(w->numbers)) ? *iter : nullptr;
}

QVector<ContactMethod*> PhoneDirectoryModel::getNumbersByPopularity() const
{
   return d_ptr->m_lPopularityIndex;
}

void PhoneDirectoryModelPrivate::slotCallAdded(Call* call)
{
   Q_UNUSED(call)

   if (call->state() == Call::State::FAILURE)
      return; //don't update popularity for failed calls

   ContactMethod* number = qobject_cast<ContactMethod*>(sender());
   if (number) {
      int currentIndex = number->dir_d_ptr->m_PopularityIndex;

      //The number is already in the top 10 and just passed the "index-1" one
      if (currentIndex > 0 && m_lPopularityIndex[currentIndex-1]->callCount() < number->callCount()) {
         do {
            ContactMethod* tmp = m_lPopularityIndex[currentIndex-1];
            m_lPopularityIndex[currentIndex-1] = number;
            m_lPopularityIndex[currentIndex  ] = tmp   ;
            tmp->dir_d_ptr->m_PopularityIndex++;
            currentIndex--;
         } while (currentIndex && m_lPopularityIndex[currentIndex-1]->callCount() < number->callCount());
         number->dir_d_ptr->m_PopularityIndex = currentIndex;

         if (m_pPopularModel)
            m_pPopularModel->reload();
      }
      //The top 10 is not complete, a call count of "1" is enough to make it
      else if (m_lPopularityIndex.size() < 10 && currentIndex == -1) {
         m_lPopularityIndex << number;
         if (m_pPopularModel)
            m_pPopularModel->addRow();
         number->dir_d_ptr->m_PopularityIndex = m_lPopularityIndex.size()-1;

      }
      //The top 10 is full, but this number just made it to the top 10
      else if (currentIndex == -1 && m_lPopularityIndex.size() >= 10 && m_lPopularityIndex[9] != number && m_lPopularityIndex[9]->callCount() < number->callCount()) {
         ContactMethod* tmp = m_lPopularityIndex[9];
         tmp->dir_d_ptr->m_PopularityIndex = -1;
         m_lPopularityIndex[9]     = number;
         number->dir_d_ptr->m_PopularityIndex = 9;
         emit tmp->changed();
         emit number->changed();
         if (m_pPopularModel)
            m_pPopularModel->reload();
      }

      //Now check for new peer names
      if (!call->peerName().isEmpty()) {
         number->incrementAlternativeName(call->peerName(), call->startTimeStamp());
      }
   }
}

void PhoneDirectoryModelPrivate::slotChanged()
{
   ContactMethod* number = qobject_cast<ContactMethod*>(sender());
   if (number) {
      const int idx = number->dir_d_ptr->m_Index;
#ifndef NDEBUG
      if (idx<0)
         qDebug() << "Invalid slotChanged() index!" << idx;
#endif
      emit q_ptr->dataChanged(q_ptr->index(idx,0),q_ptr->index(idx,static_cast<int>(Columns::REGISTERED_NAME)));
   }
}

/// Remove
void PhoneDirectoryModelPrivate::slotContactMethodMerged(ContactMethod* other)
{
    // Other == cm when the person they depend on got merged. As a CM is an
    // "person-lite" this still counts as most code paths care about both. Not
    // this, so lets ignore the merged persons.
    auto cm = qobject_cast<ContactMethod*>(sender());

    if (other == cm)
        return;

    Q_ASSERT(cm->isDuplicate() ^ other->isDuplicate());

    emit q_ptr->contactMethodMerged(
        cm->isDuplicate() ? cm    : other,
        cm->isDuplicate() ? other : cm
    );
}

void PhoneDirectoryModelPrivate::slotLastUsedChanged(time_t t)
{
   ContactMethod* cm = qobject_cast<ContactMethod*>(QObject::sender());

   if (cm)
      emit q_ptr->lastUsedChanged(cm, t);
}

void PhoneDirectoryModelPrivate::slotContactChanged(Person* newContact, Person* oldContact)
{
   ContactMethod* cm = qobject_cast<ContactMethod*>(QObject::sender());

   if (cm)
      emit q_ptr->contactChanged(cm, newContact, oldContact);
}

void PhoneDirectoryModelPrivate::slotNewBuddySubscription(const QString& accountId, const QString& uri, bool status, const QString& message)
{
   ContactMethod* number = q_ptr->getNumber(uri,AccountModel::instance().getById(accountId.toLatin1()));
   number->setPresent(status);
   number->setPresenceMessage(message);
   emit number->changed();
}

///Make sure the indexes are still valid for those names
void PhoneDirectoryModelPrivate::indexNumber(ContactMethod* number, const QStringList &names)
{
   foreach(const QString& name, names) {
      const QString lower = name.toLower();
      const QStringList split = lower.split(' ');
      if (split.size() > 1) {
         foreach(const QString& chunk, split) {
            NumberWrapper* wrap = m_hNumbersByNames[chunk];
            if (!wrap) {
               wrap = new NumberWrapper(chunk);
               m_hNumbersByNames[chunk] = wrap;
               m_lSortedNames[chunk]    = wrap;
            }
            const int numCount = wrap->numbers.size();
            if (!((numCount == 1 && wrap->numbers[0] == number) || (numCount > 1 && wrap->numbers.indexOf(number) != -1)))
               wrap->numbers << number;
         }
      }
      NumberWrapper* wrap = m_hNumbersByNames[lower];
      if (!wrap) {
         wrap = new NumberWrapper(lower);
         m_hNumbersByNames[lower] = wrap;
         m_lSortedNames[lower]    = wrap;
      }
      const int numCount = wrap->numbers.size();
      if (!((numCount == 1 && wrap->numbers[0] == number) || (numCount > 1 && wrap->numbers.indexOf(number) != -1)))
         wrap->numbers << number;
   }
}

void
PhoneDirectoryModelPrivate::slotRegisteredNameFound(Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name)
{
    if (status != NameDirectory::LookupStatus::SUCCESS) {
        // unsuccessfull lookup, so its useless
        return;
    }
    if (address.isEmpty() || name.isEmpty()) {
        qDebug() << "registered name address (" << address << ") or name (" << name << ") is empty";
        return;
    }

    // update relevant contact methods
    const URI strippedUri(address);

    //See if the number is already loaded
    auto wrap  = m_hDirectory.value(strippedUri);

    if (wrap) {
        foreach (ContactMethod* cm, wrap->numbers) {
            // Not true 100% of the time, but close enough. As of 2017, there
            // is only 1 name service. //TODO support multiple name service,
            // but only when there *is* multiple ones being used.
            if (!cm->account())
                cm->setAccount(account);

            if (cm->account() == account) {
                cm->incrementAlternativeName(name, QDateTime::currentDateTimeUtc().toTime_t());
                cm->d_ptr->setRegisteredName(name);

                // Add the CM to the directory using the registered name too.
                // Note that in theory the wrapper can exist already if the
                // user was either offline in a call attempt or if there is a
                // collision with a SIP account.
                if (!m_hDirectory.contains(name)) {
                    //TODO support multiple name service, use proper URIs for names
                    auto wrap2 = new NumberWrapper(name);
                    m_hDirectory    [name] = wrap2;
                    m_hSortedNumbers[name] = wrap2;
                    wrap2->numbers << cm;
                }
                else {
                    auto wrapper = m_hDirectory.value(name);
                    // Merge the existing CMs now that it is known that the RingId match the username
                    foreach(ContactMethod* n, wrapper->numbers) {

                        // If the account is the same and (as we know) it is a registered name
                        // there is 100% porbability of match
                        const bool compAccount = n->account() &&
                            n->account() == cm->account();

                        // Less certain, but close enough. We have a contact with a phone
                        // number corresponding with a registeredName and `ring:` in front.
                        // it *could* use a different name service. Anyway, for now this
                        // isn't widespread enough to care.
                        const bool compContact = (!n->account()) && n->contact() &&
                            n->uri().schemeType() == URI::SchemeType::RING;

                        if (n != cm && (compAccount || compContact)) {
                            n->merge(cm);
                        }
                    }
                }

                // Only add it once
                if (!m_hDirectory[name]->numbers.indexOf(cm)) {
                    //TODO check if some deduplication can be performed
                    m_hDirectory[name]->numbers << cm;
                }
            }
            else
                qDebug() << "registered name: uri matches but not account" << name << address << account << cm->account();
        }
    } else {
        // got a registered name for a CM which hasn't been created yet
        // This can be left as-is to save memory. Those CMs are never freed.
        // It is generally preferred to create as little as possible.
    }
}

int PhoneDirectoryModel::count() const {
   return d_ptr->m_lNumbers.size();
}
bool PhoneDirectoryModel::callWithAccount() const {
   return d_ptr->m_CallWithAccount;
}

//Setters
void PhoneDirectoryModel::setCallWithAccount(bool value) {
   d_ptr->m_CallWithAccount = value;
}

///Popular number model related code

MostPopularNumberModel::MostPopularNumberModel() : QAbstractListModel(&PhoneDirectoryModel::instance()) {
   setObjectName(QStringLiteral("MostPopularNumberModel"));
}

QVariant MostPopularNumberModel::data( const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return QVariant();

   return PhoneDirectoryModel::instance().d_ptr->m_lPopularityIndex[index.row()]->roleData(
      role == Qt::DisplayRole ? (int)Call::Role::Name : role
   );
}

int MostPopularNumberModel::rowCount( const QModelIndex& parent ) const
{
   return parent.isValid() ? 0 : PhoneDirectoryModel::instance().d_ptr->m_lPopularityIndex.size();
}

Qt::ItemFlags MostPopularNumberModel::flags( const QModelIndex& index ) const
{
   return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

bool MostPopularNumberModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

void MostPopularNumberModel::addRow()
{
   const int oldSize = PhoneDirectoryModel::instance().d_ptr->m_lPopularityIndex.size()-1;
   beginInsertRows(QModelIndex(),oldSize,oldSize);
   endInsertRows();
}

void MostPopularNumberModel::reload()
{
   emit dataChanged(index(0,0),index(rowCount(),0));
}

QAbstractListModel* PhoneDirectoryModel::mostPopularNumberModel() const
{
   if (!d_ptr->m_pPopularModel)
      d_ptr->m_pPopularModel = new MostPopularNumberModel();

   return d_ptr->m_pPopularModel;
}

/**
 * @return true if any ContactMethod stored has an unread message. False otherwise
 */
bool
PhoneDirectoryModel::hasUnreadMessage() const
{
    return std::any_of(d_ptr->m_lNumbers.constBegin(), d_ptr->m_lNumbers.constEnd(),
    [](ContactMethod* cm){
        return cm->textRecording()->unreadInstantTextMessagingModel()->rowCount() > 0;
    });
}

#include <phonedirectorymodel.moc>
