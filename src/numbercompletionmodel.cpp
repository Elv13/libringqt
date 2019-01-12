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
#include "numbercompletionmodel.h"

//Qt
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QTimer>

//System
#include <cmath>

//DRing
#include <account_const.h>

//Ring
#include "phonedirectorymodel.h"
#include "contactmethod.h"
#include "call.h"
#include "session.h"
#include "callmodel.h"
#include "uri.h"
#include "numbercategory.h"
#include "accountmodel.h"
#include "availableaccountmodel.h"
#include "numbercategorymodel.h"
#include "namedirectory.h"
#include "person.h"
#include "libcard/matrixutils.h"

//Private
#include "private/phonedirectorymodel_p.h"

class NumberCompletionModelPrivate final : public QObject
{
   Q_OBJECT
public:

   enum class Columns {
      CONTENT = 0,
      NAME    = 1,
      ACCOUNT = 2,
      WEIGHT  = 3,
   };

   //Constructor
   NumberCompletionModelPrivate(NumberCompletionModel* parent);

   //Methods
   void updateModel();

   //Helper
   QSet<Account*> locateNameRange  (const QString& prefix, QSet<ContactMethod*>& set);
   QSet<Account*> locateNumberRange(const QString& prefix, QSet<ContactMethod*>& set);
   uint getWeight(ContactMethod* number);
   QSet<Account*> getRange(const QMap<QString,NumberWrapper*>& map, const QString& prefix, QSet<ContactMethod*>& set) const;

   //Attributes
   QMultiMap<int,ContactMethod*> m_hNumbers              ;
   URI                           m_Prefix                ;
   Call*                         m_pCall                 ;
   bool                          m_Enabled               ;
   bool                          m_UseUnregisteredAccount;
   bool                          m_DisplayMostUsedNumbers;
   QItemSelectionModel*          m_pSelectionModel       ;
   bool                          m_HasCustomSelection    ;
   QHash<QString, QString>       m_hNameCache            ;

   QHash<Account*,TemporaryContactMethod*> m_hSipTemporaryNumbers;
   QHash<Account*,TemporaryContactMethod*> m_hRingTemporaryNumbers;
   QHash<int, TemporaryContactMethod*> m_pPreferredTemporaryNumbers;

   QPair<bool, bool> matchSipAndRing(const URI& uri) const;
   NumberCompletionModel::LookupStatus entryStatus(const ContactMethod* cm) const;
   QString entryStatusName(const ContactMethod* cm) const;
   NumberCompletionModel::EntrySource entrySource(const ContactMethod* cm) const;
   bool isSelectable(const ContactMethod* cm) const;

public Q_SLOTS:
   void setPrefix(const QString& str);

   bool accountAdded  (Account* a);
   void accountRemoved(Account* a);

   void resetSelectionModel();
   void slotSelectionChanged(const QModelIndex& sel, const QModelIndex& prev);

   void slotRegisteredNameFound(const Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name);
   void slotClearNameCache();

private:
   NumberCompletionModel* q_ptr;
};


NumberCompletionModelPrivate::NumberCompletionModelPrivate(NumberCompletionModel* parent) : QObject(parent), q_ptr(parent),
m_pCall(nullptr),m_Enabled(false),m_UseUnregisteredAccount(true), m_Prefix(QString()),m_DisplayMostUsedNumbers(false),
m_pSelectionModel(nullptr),m_HasCustomSelection(false)
{
   //Create the temporary number list
   bool     hasNonIp2Ip = false;
   Account* ip2ip       = Session::instance()->accountModel()->ip2ip();

   for (int i =0; i < Session::instance()->accountModel()->size();i++) {
      Account* a = (*Session::instance()->accountModel())[i];
      if (a != ip2ip) {
         hasNonIp2Ip |= accountAdded(a);
      }
   }

   //If SIP accounts are present, IP2IP is not needed
   if (ip2ip && !hasNonIp2Ip) {
      TemporaryContactMethod* cm = new TemporaryContactMethod();
      cm->setAccount(ip2ip);
      m_hSipTemporaryNumbers[ip2ip] = cm;
   }

   connect(Session::instance()->accountModel(), &AccountModel::accountAdded  , this, &NumberCompletionModelPrivate::accountAdded  );
   connect(Session::instance()->accountModel(), &AccountModel::accountRemoved, this, &NumberCompletionModelPrivate::accountRemoved);

   connect(Session::instance()->nameDirectory(), &NameDirectory::registeredNameFound,
      this, &NumberCompletionModelPrivate::slotRegisteredNameFound);

   auto t = new QTimer(this);
   t->setInterval(5 * 60 * 1000);
   connect(t, &QTimer::timeout, this, &NumberCompletionModelPrivate::slotClearNameCache);
   t->start();
}

NumberCompletionModel::NumberCompletionModel() : QAbstractTableModel(&PhoneDirectoryModel::instance()), d_ptr(new NumberCompletionModelPrivate(this))
{
   setObjectName(QStringLiteral("NumberCompletionModel"));
   connect(Session::instance()->callModel(), &CallModel::dialNumberChanged, d_ptr,
      [this](Call*, const QString& str) {d_ptr->setPrefix(str);}
   );
}

NumberCompletionModel::~NumberCompletionModel()
{
   QList<TemporaryContactMethod*> l = d_ptr->m_hSipTemporaryNumbers.values();

   d_ptr->m_hSipTemporaryNumbers.clear();

   while(l.size()) {
      TemporaryContactMethod* cm = l.takeAt(0);
      delete cm;
   }

   l = d_ptr->m_hRingTemporaryNumbers.values();
   d_ptr->m_hRingTemporaryNumbers.clear();

   while(l.size()) {
      TemporaryContactMethod* cm = l.takeAt(0);
      delete cm;
   }

   delete d_ptr;
}

QHash<int,QByteArray> NumberCompletionModel::roleNames() const
{
   static QHash<int, QByteArray> roles = PhoneDirectoryModel::instance().roleNames();
   static bool initRoles = false;

   if (!initRoles) {
      initRoles = true;
      roles[Role::ALTERNATE_ACCOUNT]= "alternateAccount";
      roles[Role::FORCE_ACCOUNT    ]= "forceAccount"    ;
      roles[Role::ACCOUNT          ]= "account"         ;
      roles[Role::ACCOUNT_ALIAS    ]= "accountAlias"    ;
      roles[Role::IS_TEMP          ]= "temporary"       ;
      roles[Role::NAME_STATUS      ]= "nameStatus"      ;
      roles[Role::NAME_STATUS_SRING]= "nameStatusString";
      roles[Role::SUPPORTS_REGISTRY]= "supportsRegistry";
      roles[Role::ENTRY_SOURCE     ]= "entrySource"     ;
      roles[Role::IS_SELECTABLE    ]= "selectable"      ;
   }

   return roles;
}

QVariant NumberCompletionModel::data(const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return QVariant();

   const QMap<int,ContactMethod*>::iterator i = d_ptr->m_hNumbers.end()-1-index.row();
   const ContactMethod* n = i.value();
   const int weight     = i.key  ();

   const bool needAcc = (role>=100 || role == Qt::UserRole) && n->account() /*&& n->account() != AvailableAccountModel::currentDefaultAccount()*/
        && !n->account()->isIp2ip();

   switch (static_cast<NumberCompletionModelPrivate::Columns>(index.column())) {
      case NumberCompletionModelPrivate::Columns::CONTENT:
         switch (role) {
            case Qt::DisplayRole:
               return n->bestName();
            case Qt::ToolTipRole:
               return QStringLiteral("<table><tr><td>%1</td></tr><tr><td>%2</td></tr></table>")
                  .arg(n->primaryName())
                  .arg(n->category() ? n->category()->name() : QString());
            case NumberCompletionModel::Role::ALTERNATE_ACCOUNT:
            case Qt::UserRole:
               if (needAcc)
                  return n->account()->alias();
               else
                  return QString();
            case NumberCompletionModel::Role::FORCE_ACCOUNT:
               return needAcc;
            case NumberCompletionModel::Role::PEER_NAME:
               return n->primaryName();
            case NumberCompletionModel::Role::ACCOUNT:
               if (needAcc)
                  return QVariant::fromValue(n->account());
               break;
            case static_cast<int>(Role::ACCOUNT_ALIAS):
               return n->account() ? n->account()->alias() : QString();
            case static_cast<int>(Role::IS_TEMP):
               return n->type() == ContactMethod::Type::TEMPORARY;
            case static_cast<int>(Role::NAME_STATUS):
               return QVariant::fromValue(d_ptr->entryStatus(n));
            case static_cast<int>(Role::NAME_STATUS_SRING):
               return d_ptr->entryStatusName(n);
            case static_cast<int>(Role::SUPPORTS_REGISTRY):
               return n->type() == ContactMethod::Type::TEMPORARY &&
                  n->account() && n->account()->protocol() == Account::Protocol::RING && n->uri().protocolHint() != URI::ProtocolHint::RING;
            case static_cast<int>(Role::ENTRY_SOURCE):
                return QVariant::fromValue(d_ptr->entrySource(n));
            case static_cast<int>(Role::IS_SELECTABLE):
                return d_ptr->isSelectable(n);
         };
         return n->roleData(role);
      case NumberCompletionModelPrivate::Columns::NAME:
         return n->roleData(role);
      case NumberCompletionModelPrivate::Columns::ACCOUNT:
         if(auto acc = n->account() ? n->account() :  Session::instance()->availableAccountModel()->currentDefaultAccount())
            return acc->roleData(role);
         break;
      case NumberCompletionModelPrivate::Columns::WEIGHT:
         switch (role) {
            case Qt::DisplayRole:
               return weight;
         };
         break;
   };

   return QVariant();
}

int NumberCompletionModel::rowCount(const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;

   return d_ptr->m_hNumbers.size();
}

int NumberCompletionModel::columnCount(const QModelIndex& parent ) const
{
   if (parent.isValid())
      return 0;

   return 4;
}

Qt::ItemFlags NumberCompletionModel::flags(const QModelIndex& index ) const
{
   if (!index.isValid())
      return Qt::NoItemFlags;

   const QMap<int,ContactMethod*>::iterator i = d_ptr->m_hNumbers.end()-1-index.row();

   return (
      d_ptr->isSelectable(i.value()) ?
         Qt::ItemIsEnabled : Qt::NoItemFlags
      ) |Qt::ItemIsSelectable;
}

QVariant NumberCompletionModel::headerData (int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(orientation)
   static const QString headers[] = {tr("URI"), tr("Name"), tr("Account"), tr("Weight")};

   if (role == Qt::DisplayRole)
      return headers[section];

   return QVariant();
}

bool NumberCompletionModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED( index )
   Q_UNUSED( value )
   Q_UNUSED( role  )
   return false;
}

void NumberCompletionModelPrivate::setPrefix(const QString& str)
{
    if ((!str.isEmpty()) && !Session::instance()->callModel()->hasDialingCall()) {
        m_Prefix.clear();
        if (auto c = Session::instance()->callModel()->dialingCall()) {

            if (c != m_pCall)
                resetSelectionModel();

            m_pCall = c;
        }
        else
            m_pCall = nullptr;
    }

    m_Prefix = str;

    const bool e = ((m_pCall && m_pCall->lifeCycleState() == Call::LifeCycleState::CREATION) || (!m_pCall)) && ((m_DisplayMostUsedNumbers || !str.isEmpty()));

    if (m_Enabled != e) {
        m_Enabled = e;
        emit q_ptr->enabled(e);
    }

    if (!m_Enabled) {
        q_ptr->beginRemoveRows({}, 0, m_hNumbers.size()-1);
        m_hNumbers.clear();
        q_ptr->endRemoveRows();
    }

    auto show = matchSipAndRing(m_Prefix);

    if (show.second) {
        for(TemporaryContactMethod* cm : qAsConst(m_hRingTemporaryNumbers)) {
            if (!cm)
                continue;

            if (cm->account()->registrationState() != Account::RegistrationState::READY)
                continue;

            if (m_hNameCache.contains(m_Prefix) && m_hNameCache[m_Prefix] != QLatin1String("-1")) {
                cm->setUri(m_hNameCache[m_Prefix]);
                cm->setRegisteredName(m_Prefix);
            }
            else
                cm->setUri(m_Prefix);

            // Perform name lookups
            if (str.size() >=3 && !m_hNameCache.contains(str))
                Session::instance()->nameDirectory()->lookupName(cm->account(), {}, str);
        }
    }

    if (show.first) {
        for(auto cm : qAsConst(m_hSipTemporaryNumbers)) {
            if (!cm)
                continue;

            if (cm->account()->registrationState() != Account::RegistrationState::READY)
                continue;

            cm->setUri(m_Prefix);
        }
    }

    if (m_Enabled)
        updateModel();
}

ContactMethod* NumberCompletionModel::number(const QModelIndex& idx) const
{
   if (idx.isValid()) {
      //Keep the temporary contact methods private, export a copy
      ContactMethod* m = (d_ptr->m_hNumbers.end()-1-idx.row()).value();
      return m->type() == ContactMethod::Type::TEMPORARY ?
         PhoneDirectoryModel::instance().fromTemporary(qobject_cast<TemporaryContactMethod*>(m))
         : m;
   }

   return nullptr;
}

QPair<bool, bool> NumberCompletionModelPrivate::matchSipAndRing(const URI& uri) const
{
    bool showSip(false), showRing(false);

    switch(uri.protocolHint()) {
        case URI::ProtocolHint::SIP_OTHER:
        showSip = true;
        showRing = true;
        break;
        case URI::ProtocolHint::RING:
        case URI::ProtocolHint::RING_USERNAME:
        showRing = true;
        break;
        case URI::ProtocolHint::IP:
        case URI::ProtocolHint::SIP_HOST:
        showSip = true;
        break;
    }

    return {showSip, showRing};
}

void NumberCompletionModelPrivate::updateModel()
{
   QSet<ContactMethod*> numbers;
   q_ptr->beginRemoveRows({}, 0, m_hNumbers.size()-1);
   m_hNumbers.clear();
   q_ptr->endRemoveRows();

   if (!m_Prefix.isEmpty()) {
      const auto perfectMatches1 = locateNameRange  ( m_Prefix, numbers );
      const auto perfectMatches2 = locateNumberRange( m_Prefix, numbers );

      auto show = matchSipAndRing(m_Prefix);

      if (show.second) {
         for (TemporaryContactMethod* cm : qAsConst(m_hRingTemporaryNumbers)) {
            if (perfectMatches1.contains(cm->account()) || perfectMatches2.contains(cm->account()))
               continue;

            const int weight = getWeight(cm);
            if (weight) {
               q_ptr->beginInsertRows({}, m_hNumbers.size(), m_hNumbers.size());
               m_hNumbers.insert(weight,cm);
               q_ptr->endInsertRows();
            }
         }
      }

      if (show.first) {
         for (auto cm : qAsConst(m_hSipTemporaryNumbers)) {
            if (!cm) continue;
            if (perfectMatches1.contains(cm->account()) || perfectMatches2.contains(cm->account()))
               continue;

            if (auto weight = getWeight(cm)) {
               q_ptr->beginInsertRows({}, m_hNumbers.size(), m_hNumbers.size());
               m_hNumbers.insert(weight, cm);
               q_ptr->endInsertRows();
            }
         }
      }

      for (ContactMethod* n : qAsConst(numbers)) {
         if (m_UseUnregisteredAccount || ((n->account() && n->account()->registrationState() == Account::RegistrationState::READY)
          || !n->account())) {
            if (auto weight = getWeight(n)) {
                q_ptr->beginInsertRows({}, m_hNumbers.size(), m_hNumbers.size());
                m_hNumbers.insert(weight, n);
                q_ptr->endInsertRows();
            }
         }
      }
   }
   else if (m_DisplayMostUsedNumbers) {
      //If enabled, display the most probable entries
      const QVector<ContactMethod*> cl = PhoneDirectoryModel::instance().getNumbersByPopularity();

      for (int i=0;i<((cl.size()>=10)?10:cl.size());i++) {
         ContactMethod* n = cl[i];
         if (auto weight = getWeight(n)) {
            q_ptr->beginInsertRows({}, m_hNumbers.size(), m_hNumbers.size());
            m_hNumbers.insert(weight, n);
            q_ptr->endInsertRows();
         }
      }
   }
}

QSet<Account*> NumberCompletionModelPrivate::getRange(const QMap<QString,NumberWrapper*>& map, const QString& prefix, QSet<ContactMethod*>& set) const
{
    if (prefix.isEmpty() || map.isEmpty())
        return {};

    static NumberWrapper fake(QLatin1String(""));
    fake.key = prefix;

    auto start = std::lower_bound(map.constBegin(), map.constEnd(), &fake,
      [](NumberWrapper* candidate, NumberWrapper* target) {
        return candidate->key.toLower().left(target->key.size()) < target->key;
    });

    auto end = std::upper_bound(start, map.constEnd(), &fake,
      [](NumberWrapper* target, NumberWrapper* candidate) {
        return candidate->key.toLower().left(target->key.size()) > target->key;
    });

    if (start == map.constEnd())
        return {};

    QSet<Account*> ret;

    std::for_each(start, end, [&set, &ret, &prefix](NumberWrapper* n) {
        for (auto cm : qAsConst(n->numbers)) {
            if (!cm) continue;

            set << cm;

            // Don't show the TemporaryContactMethod when there's a perfect match
            if (cm->account() && cm->uri() == prefix)
                ret << cm->account();
        }
    });

    return ret;
}

QSet<Account*> NumberCompletionModelPrivate::locateNameRange(const QString& prefix, QSet<ContactMethod*>& set)
{
   return getRange(PhoneDirectoryModel::instance().d_ptr->m_lSortedNames,prefix,set);
}

QSet<Account*> NumberCompletionModelPrivate::locateNumberRange(const QString& prefix, QSet<ContactMethod*>& set)
{
   return getRange(PhoneDirectoryModel::instance().d_ptr->m_hSortedNumbers,prefix,set);
}

uint NumberCompletionModelPrivate::getWeight(ContactMethod* number)
{
    // Don't waste effort on unregistered accounts
    if(number->account() && number->account()->registrationState() != Account::RegistrationState::READY)
        return 0;

    // Do not add own contact methods to the completion
    if (number->isSelf())
        return 0;

    uint weight = 1;

    // Ring accounts should have higher priorities when the registered name exists,
    // but not too much to avoid a bias on common names.
    const bool isDialingRing = m_Prefix.size() && number->account()
        && number->account()->protocol() == Account::Protocol::RING
        && number->type() == ContactMethod::Type::TEMPORARY;

    // The name service never reply on those
    if (isDialingRing && number->registeredName().isEmpty() && m_Prefix.size() < 3)
        return 0;

    // Invalid ring names (but allow hashes)
    //if (isDialingRing && number->registeredName().isEmpty() && m_Prefix.size() < 40)
    //    return 0;

    const auto account = number->account();

    switch(number->type()) {
        case ContactMethod::Type::TEMPORARY:
            Q_ASSERT(account);
            weight += (account->weekCallCount         ()+1)*15;
            weight += (account->trimesterCallCount    ()+1)*7 ;
            weight += (account->totalCallCount        ()+1)*3 ;
            weight *= (account->isUsedForOutgogingCall()?3:1 );
            weight *= isDialingRing ? 10 : 1;
            break;
        case ContactMethod::Type::USED:
        case ContactMethod::Type::UNUSED:
            weight += (number->weekCount()+1)*150;
            weight += (number->trimCount()+1)*75 ;
            weight += (number->callCount()+1)*35 ;
            weight *= (number->uri().indexOf(m_Prefix)!= -1?3:1);
            weight *= (number->isPresent()?2:1);
            weight *= (uint) (isDialingRing ? 1.1 : 1.0);
            break;
        case ContactMethod::Type::ACCOUNT:
            qWarning() << "An account own contact method leaked into the completion";
            return 0;
        case ContactMethod::Type::BLANK:
            Q_ASSERT(false);
    }

    return weight;
}

QString NumberCompletionModel::prefix() const
{
   return d_ptr->m_Prefix;
}

void NumberCompletionModel::setUseUnregisteredAccounts(bool value)
{
   d_ptr->m_UseUnregisteredAccount = value;
}

bool NumberCompletionModel::isUsingUnregisteredAccounts()
{
   return d_ptr->m_UseUnregisteredAccount;
}


void NumberCompletionModel::setDisplayMostUsedNumbers(bool value)
{
   d_ptr->m_DisplayMostUsedNumbers = value;
}

bool NumberCompletionModel::displayMostUsedNumbers() const
{
   return d_ptr->m_DisplayMostUsedNumbers;
}

void NumberCompletionModelPrivate::resetSelectionModel()
{
   if (!m_pSelectionModel)
      return;

   //const Account* preferredAccount = AvailableAccountModel::currentDefaultAccount();

   //m_pSelectionModel->setCurrentIndex(index(idx,0), QItemSelectionModel::ClearAndSelect);

   m_HasCustomSelection = false;
}

void NumberCompletionModelPrivate::slotSelectionChanged(const QModelIndex& sel, const QModelIndex& prev)
{
   Q_UNUSED(sel)
   Q_UNUSED(prev)
   m_HasCustomSelection = true;
   emit q_ptr->selectionChanged();
}

bool NumberCompletionModel::callSelectedNumber()
{
   if (!d_ptr->m_pSelectionModel || !d_ptr->m_pCall)
      return false;

   const auto idx = d_ptr->m_pSelectionModel->currentIndex();

   if (!idx.isValid())
      return false;

   ContactMethod* nb = number(idx);

   if (!nb)
      return false;

   if (d_ptr->m_pCall->lifeCycleState() != Call::LifeCycleState::CREATION)
      return false;

   d_ptr->m_pCall->setDialNumber(nb);
   d_ptr->m_pCall->setAccount(nb->account());

   d_ptr->resetSelectionModel();

   d_ptr->m_pCall->performAction(Call::Action::ACCEPT);

   d_ptr->setPrefix({});

   return true;
}

QItemSelectionModel* NumberCompletionModel::selectionModel() const
{
   if (!d_ptr->m_pSelectionModel) {
      d_ptr->m_pSelectionModel = new QItemSelectionModel(const_cast<NumberCompletionModel*>(this));
      connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged, d_ptr, &NumberCompletionModelPrivate::slotSelectionChanged);
   }

   return d_ptr->m_pSelectionModel;
}

// Simplify accessing selection from QML
ContactMethod* NumberCompletionModel::selectedContactMethod() const
{
    if (!d_ptr->m_pSelectionModel)
        return nullptr;

    return number(d_ptr->m_pSelectionModel->currentIndex());
}

bool NumberCompletionModelPrivate::accountAdded(Account* a)
{
    bool hasNonIp2Ip = false;
    TemporaryContactMethod* cm = nullptr;

    switch(a->protocol()) {
        case Account::Protocol::SIP:
            hasNonIp2Ip = a->isEnabled();
            cm = new TemporaryContactMethod();

            if (!m_pPreferredTemporaryNumbers[(int)a->protocol()])
                m_pPreferredTemporaryNumbers[(int)a->protocol()] = cm;

            cm->setAccount(a);
            m_hSipTemporaryNumbers[a] = cm;
            break;
        case Account::Protocol::RING:
            cm = new TemporaryContactMethod();
            cm->setAccount(a);
            m_hRingTemporaryNumbers[a] = cm;

            if (!m_pPreferredTemporaryNumbers[(int)Account::Protocol::RING])
                m_pPreferredTemporaryNumbers[(int)Account::Protocol::RING] = cm;

            break;
        case Account::Protocol::COUNT__:
            break;
    }

    return hasNonIp2Ip;
}

void NumberCompletionModelPrivate::accountRemoved(Account* a)
{
   TemporaryContactMethod* cm = m_hSipTemporaryNumbers[a];

   if (!cm)
      cm = m_hRingTemporaryNumbers[a];

   m_hSipTemporaryNumbers.remove(a);
   m_hRingTemporaryNumbers.remove(a);

   setPrefix(q_ptr->prefix());

   if (cm) {
      delete cm;
   }
}

void NumberCompletionModelPrivate::slotRegisteredNameFound(const Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name)
{
    Q_UNUSED(account)

    bool reload = false;

    // Check if there's a match
    for (TemporaryContactMethod* cm : qAsConst(m_hRingTemporaryNumbers)) {
        if (cm->uri() == name) {

            // Cache the names for up to 5 minutes to prevent useless deaemon calls
            m_hNameCache[name] = status == NameDirectory::LookupStatus::SUCCESS ?
                address : QStringLiteral("-1");

            if (status == NameDirectory::LookupStatus::SUCCESS) {
                cm->setUri(address);
                cm->setRegisteredName(name);
                reload = true;
            }

            // Until the patch to cleanup model insertion is merged, this will
            // have to do.
            emit q_ptr->dataChanged(q_ptr->index(0,0), q_ptr->index(m_hNumbers.size()-1, 0));
        }
    }

    if (reload)
        updateModel();
}

void NumberCompletionModelPrivate::slotClearNameCache()
{
    m_hNameCache.clear();
}

NumberCompletionModel::LookupStatus NumberCompletionModelPrivate::entryStatus(const ContactMethod* cm) const
{
    if (cm->type() == ContactMethod::Type::TEMPORARY) {
        if (cm->account() && cm->account()->protocol() != Account::Protocol::RING)
            return NumberCompletionModel::LookupStatus::NOT_APPLICABLE;

        if (!cm->registeredName().isEmpty())
            return NumberCompletionModel::LookupStatus::SUCCESS;

        if (m_hNameCache.contains(cm->uri())) {
            return m_hNameCache[cm->uri()] == QLatin1String("-1") ?
                NumberCompletionModel::LookupStatus::FAILURE:
                NumberCompletionModel::LookupStatus::SUCCESS;
        }
        else
            return NumberCompletionModel::LookupStatus::IN_PROGRESS;
    }

    return cm->registeredName().isEmpty() ?
        NumberCompletionModel::LookupStatus::NOT_APPLICABLE :
        NumberCompletionModel::LookupStatus::SUCCESS;
}

// Detect if the element can be selected
bool NumberCompletionModelPrivate::isSelectable(const ContactMethod* cm) const
{
    // Assume the homework were already done
    if (cm->type() != ContactMethod::Type::TEMPORARY)
        return true;

    // That's not supposed to happen
    if (cm->uri().isEmpty())
        return false;

    // There is no way to know
    if (cm->account() && cm->account()->protocol() == Account::Protocol::SIP)
        return true;

    if (cm->account() && cm->account()->protocol() == Account::Protocol::RING) {
        // Names under 3 chars are not allowed
        if (cm->uri().size() < 3)
            return false;

        if (cm->uri().protocolHint() == URI::ProtocolHint::RING)
            return true;

        if ((!cm->registeredName().isEmpty()) || entryStatus(cm) == NumberCompletionModel::LookupStatus::SUCCESS)
            return true;

        return false;
    }

    // There is no way to know
    return true;
}

QString NumberCompletionModelPrivate::entryStatusName(const ContactMethod* cm) const
{
    static const Matrix1D<NumberCompletionModel::LookupStatus ,QString> names = {
        {NumberCompletionModel::LookupStatus::NOT_APPLICABLE, QObject::tr("N/A"       )},
        {NumberCompletionModel::LookupStatus::IN_PROGRESS   , QObject::tr("Looking up")},
        {NumberCompletionModel::LookupStatus::SUCCESS       , QObject::tr("Found"     )},
        {NumberCompletionModel::LookupStatus::FAILURE       , QObject::tr("Not found" )},
    };

    return names[entryStatus(cm)];
}

NumberCompletionModel::EntrySource NumberCompletionModelPrivate::entrySource(const ContactMethod* cm) const
{
    if (cm->isBookmarked())
        return NumberCompletionModel::EntrySource::FROM_BOOKMARKS;

    if (cm->lastUsed())
        return NumberCompletionModel::EntrySource::FROM_HISTORY;

    if (cm->contact())
        return NumberCompletionModel::EntrySource::FROM_CONTACTS;

    // Technically it can also be a random value on a SIP account, but for Ring
    // accounts it's good enough.
    return NumberCompletionModel::EntrySource::FROM_WEB;
}

#include <numbercompletionmodel.moc>
