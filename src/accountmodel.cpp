/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *            Nicolas Jäger <nicolas.jager@savoirfairelinux.com>            *
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
#include "accountmodel.h"

//Std
#include <atomic>
#include <algorithm>

//Qt
#include <QtCore/QObject>
#include <QtCore/QCoreApplication>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QMimeData>
#include <QtCore/QDir>

//Ring daemon
#include <account_const.h>

//Ring library
#include "private/account_p.h"
#include "account.h"
#include "session.h"
#include "mime.h"
#include "profilemodel.h"
#include "protocolmodel.h"
#include "contactrequest.h"
#include "pendingcontactrequestmodel.h"
#include "ringdevicemodel.h"
#include "private/accountmodel_p.h"
#include "private/ringdevicemodel_p.h"
#include "accountstatusmodel.h"
#include "dbus/configurationmanager.h"
#include "dbus/callmanager.h"
#include "dbus/instancemanager.h"
#include "codecmodel.h"
#include "private/pendingcontactrequestmodel_p.h"
#include "person.h"
#include "private/vcardutils.h"
#include "individualdirectory.h"
#include "bannedcontactmodel.h"

QHash<QByteArray,AccountPlaceHolder*> AccountModelPrivate::m_hsPlaceHolder;

AccountModelPrivate::AccountModelPrivate(AccountModel* parent) : QObject(parent),q_ptr(parent)
{}

///Constructors
AccountModel::AccountModel()
    : QAbstractListModel(QCoreApplication::instance())
    , d_ptr(new AccountModelPrivate(this))
{
    d_ptr->init();
}

void AccountModelPrivate::init()
{
    InstanceManager::instance(); // Make sure the daemon is running before calling updateAccounts()
    q_ptr->updateAccounts();

    CallManagerInterface& callManager = CallManager::instance();
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();

    connect(&configurationManager, &ConfigurationManagerInterface::registrationStateChanged,this ,
            &AccountModelPrivate::slotDaemonAccountChanged, Qt::QueuedConnection);
    connect(&configurationManager, SIGNAL(accountsChanged())                               ,q_ptr,
            SLOT(updateAccounts()), Qt::QueuedConnection);
    connect(&callManager         , SIGNAL(voiceMailNotify(QString,int))                    ,this ,
            SLOT(slotVoiceMailNotify(QString,int))  );
    connect(&configurationManager, SIGNAL(volatileAccountDetailsChanged(QString,MapStringString)),this,
            SLOT(slotVolatileAccountDetailsChange(QString,MapStringString)), Qt::QueuedConnection);
    connect(&configurationManager, SIGNAL(mediaParametersChanged(QString))                 ,this ,
            SLOT(slotMediaParametersChanged(QString)), Qt::QueuedConnection);
    connect(&configurationManager, &ConfigurationManagerInterface::knownDevicesChanged, this,
            &AccountModelPrivate::slotKownDevicesChanged, Qt::QueuedConnection);
    connect(&configurationManager, &ConfigurationManagerInterface::exportOnRingEnded, this,
            &AccountModelPrivate::slotExportOnRingEnded, Qt::QueuedConnection);
    connect(&configurationManager, &ConfigurationManagerInterface::deviceRevocationEnded, this,
            &AccountModelPrivate::slotDeviceRevocationEnded, Qt::QueuedConnection);
    connect(&configurationManager, &ConfigurationManagerInterface::migrationEnded, this,
            &AccountModelPrivate::slotMigrationEnded, Qt::QueuedConnection);
    connect(&configurationManager, &ConfigurationManagerInterface::contactRemoved, this,
            &AccountModelPrivate::slotContactRemoved, Qt::QueuedConnection);

    slotAvailabilityStatusChanged();
}

///Destructor
AccountModel::~AccountModel()
{
   while(d_ptr->m_lAccounts.size()) {
      Account* a = d_ptr->m_lAccounts[0];
      d_ptr->m_lAccounts.remove(0);
      delete a;
   }
   for(Account* a : d_ptr->m_pRemovedAccounts) {
      delete a;
   }
   delete d_ptr;
}

#define CAST(item) static_cast<int>(item)
QHash<int,QByteArray> AccountModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

    static std::atomic_flag initRoles = ATOMIC_FLAG_INIT;
    if (!initRoles.test_and_set()) {
        for (auto i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); i++)
            roles[i.key()] = i.value();

        roles.insert((int) Account::Role::Alias                       , "alias");
        roles.insert((int) Account::Role::Proto                       , "proto");
        roles.insert((int) Account::Role::Hostname                    , "hostname");
        roles.insert((int) Account::Role::Username                    , "username");
        roles.insert((int) Account::Role::Mailbox                     , "mailbox");
        roles.insert((int) Account::Role::Proxy                       , "proxy");
        roles.insert((int) Account::Role::TlsPassword                 , "tlsPassword");
        roles.insert((int) Account::Role::TlsCaListCertificate        , "tlsCaListCertificate");
        roles.insert((int) Account::Role::TlsCertificate              , "tlsCertificate");
        roles.insert((int) Account::Role::TlsPrivateKey               , "tlsPrivateKey");
        roles.insert((int) Account::Role::TlsServerName               , "tlsServerName");
        roles.insert((int) Account::Role::SipStunServer               , "sipStunServer");
        roles.insert((int) Account::Role::PublishedAddress            , "publishedAddress");
        roles.insert((int) Account::Role::RingtonePath                , "ringtonePath");
        roles.insert((int) Account::Role::RegistrationExpire          , "registrationExpire");
        roles.insert((int) Account::Role::TlsNegotiationTimeoutSec    , "tlsNegotiationTimeoutSec");
        roles.insert((int) Account::Role::TlsNegotiationTimeoutMsec   , "tlsNegotiationTimeoutMsec");
        roles.insert((int) Account::Role::LocalPort                   , "localPort");
        roles.insert((int) Account::Role::BootstrapPort               , "bootstrapPort");
        roles.insert((int) Account::Role::PublishedPort               , "publishedPort");
        roles.insert((int) Account::Role::Enabled                     , "enabled");
        roles.insert((int) Account::Role::TlsVerifyServer             , "tlsVerifyServer");
        roles.insert((int) Account::Role::TlsVerifyClient             , "tlsVerifyClient");
        roles.insert((int) Account::Role::TlsRequireClientCertificate , "tlsRequireClientCertificate");
        roles.insert((int) Account::Role::TlsEnabled                  , "tlsEnabled");
        roles.insert((int) Account::Role::SrtpRtpFallback             , "srtpRtpFallback");
        roles.insert((int) Account::Role::SipStunEnabled              , "sipStunEnabled");
        roles.insert((int) Account::Role::PublishedSameAsLocal        , "publishedSameAsLocal");
        roles.insert((int) Account::Role::RingtoneEnabled             , "ringtoneEnabled");
        roles.insert((int) Account::Role::dTMFType                    , "dTMFType");
        roles.insert((int) Account::Role::Id                          , "id");
        roles.insert((int) Account::Role::TypeName                    , "typeName");
        roles.insert((int) Account::Role::PresenceStatus              , "presenceStatus");
        roles.insert((int) Account::Role::PresenceMessage             , "presenceMessage");
        roles.insert((int) Account::Role::RegistrationState           , "registrationState");
        roles.insert((int) Account::Role::UseDefaultPort              , "useDefaultPort");
        roles.insert((int) Account::Role::UsedForOutgogingCall        , "usedForOutgogingCall");
        roles.insert((int) Account::Role::TotalCallCount              , "totalCallCount");
        roles.insert((int) Account::Role::WeekCallCount               , "weekCallCount");
        roles.insert((int) Account::Role::TrimesterCallCount          , "trimesterCallCount");
        roles.insert((int) Account::Role::LastUsed                    , "lastUsed");
        roles.insert((int) Account::Role::UserAgent                   , "userAgent");
        roles.insert((int) Account::Role::Password                    , "password");
        roles.insert((int) Account::Role::SupportPresencePublish      , "supportPresencePublish");
        roles.insert((int) Account::Role::SupportPresenceSubscribe    , "supportPresenceSubscribe");
        roles.insert((int) Account::Role::PresenceEnabled             , "presenceEnabled");
        roles.insert((int) Account::Role::IsVideoEnabled              , "isVideoEnabled");
        roles.insert((int) Account::Role::VideoPortMax                , "videoPortMax");
        roles.insert((int) Account::Role::VideoPortMin                , "videoPortMin");
        roles.insert((int) Account::Role::AudioPortMin                , "audioPortMin");
        roles.insert((int) Account::Role::AudioPortMax                , "audioPortMax");
        roles.insert((int) Account::Role::IsUpnpEnabled               , "isUpnpEnabled");
        roles.insert((int) Account::Role::HasCustomUserAgent          , "hasCustomUserAgent");
        roles.insert((int) Account::Role::LastTransportErrorCode      , "lastTransportErrorCode");
        roles.insert((int) Account::Role::LastTransportErrorMessage   , "lastTransportErrorMessage");
        roles.insert((int) Account::Role::TurnServer                  , "turnServer");
        roles.insert((int) Account::Role::TurnServerEnabled           , "turnServerEnabled");
        roles.insert((int) Account::Role::TurnServerUsername          , "turnServerUsername");
        roles.insert((int) Account::Role::TurnServerPassword          , "turnServerPassword");
        roles.insert((int) Account::Role::TurnServerRealm             , "turnServerRealm");
        roles.insert((int) Account::Role::HasProxy                    , "hasProxy");
        roles.insert((int) Account::Role::DisplayName                 , "displayName");
        roles.insert((int) Account::Role::SrtpEnabled                 , "srtpEnabled");
        roles.insert((int) Account::Role::HasCustomBootstrap          , "hasCustomBootstrap");
        roles.insert((int) Account::Role::CredentialModel             , "credentialModel");
        roles.insert((int) Account::Role::CodecModel                  , "codecModel");
        roles.insert((int) Account::Role::KeyExchangeModel            , "keyExchangeModel");
        roles.insert((int) Account::Role::CipherModel                 , "cipherModel");
        roles.insert((int) Account::Role::StatusModel                 , "statusModel");
        roles.insert((int) Account::Role::SecurityEvaluationModel     , "securityEvaluationModel");
        roles.insert((int) Account::Role::TlsMethodModel              , "tlsMethodModel");
        roles.insert((int) Account::Role::ProtocolModel               , "protocolModel");
        roles.insert((int) Account::Role::BootstrapModel              , "bootstrapModel");
        roles.insert((int) Account::Role::RingDeviceModel             , "ringDeviceModel");
        roles.insert((int) Account::Role::NetworkInterfaceModel       , "networkInterfaceModel");
        roles.insert((int) Account::Role::KnownCertificateModel       , "knownCertificateModel");
        roles.insert((int) Account::Role::BannedCertificatesModel     , "bannedCertificatesModel");
        roles.insert((int) Account::Role::AllowedCertificatesModel    , "allowedCertificatesModel");
        roles.insert((int) Account::Role::AllowIncomingFromHistory    , "allowIncomingFromHistory");
        roles.insert((int) Account::Role::AllowIncomingFromContact    , "allowIncomingFromContact");
        roles.insert((int) Account::Role::AllowIncomingFromUnknown    , "allowIncomingFromUnknown");
        roles.insert((int) Account::Role::ActiveCallLimit             , "activeCallLimit");
        roles.insert((int) Account::Role::HasActiveCallLimit          , "hasActiveCallLimit");
        roles.insert((int) Account::Role::SecurityLevel               , "securityLevel");
        roles.insert((int) Account::Role::SecurityLevelIcon           , "securityLevelIcon");
        roles.insert((int) Account::Role::RegistrationStateColor      , "registrationStateColor");
        roles.insert((int) Account::Role::LastStatusChangeTimeStamp   , "lastStatusChangeTimeStamp");
        roles.insert((int) Account::Role::RegisteredName              , "registeredName");
        roles.insert((int) Account::Role::LastErrorCode               , "lastErrorCode");
        roles.insert((int) Account::Role::LastErrorMessage            , "lastErrorMessage");
        roles.insert((int) Account::Role::Profile                     , "profile");
   }
   return roles;
}
#undef CAST

///Get the first IP2IP account
Account* AccountModel::ip2ip() const
{
   if (!d_ptr->m_pIP2IP) {
      foreach(Account* a, d_ptr->m_lAccounts) {
         if (a->isIp2ip())
            d_ptr->m_pIP2IP = a;
      }
   }
   return d_ptr->m_pIP2IP;
}

QItemSelectionModel* AccountModel::selectionModel() const
{
   if (!d_ptr->m_pSelectionModel)
      d_ptr->m_pSelectionModel = new QItemSelectionModel(const_cast<AccountModel*>(this));

   return d_ptr->m_pSelectionModel;
}


QItemSelectionModel* AccountModel::userSelectionModel() const
{
   if (!d_ptr->m_pUserSelectionModel)
      d_ptr->m_pUserSelectionModel = new QItemSelectionModel(const_cast<AccountModel*>(this));

   return d_ptr->m_pUserSelectionModel;
}

PendingContactRequestModel* AccountModel::incomingContactRequestModel() const
{
    if (!d_ptr->m_pPendingIncomingRequests) {
        d_ptr->m_pPendingIncomingRequests = new PendingContactRequestModel(
            const_cast<AccountModel*>(this)
        );
        d_ptr->m_pPendingIncomingRequests->setObjectName("incomingContactRequestModel");
    }

    return d_ptr->m_pPendingIncomingRequests;
}

/**
 * returns the select account
 */
Account*
AccountModel::selectedAccount() const
{
   auto accIdx = Session::instance()->accountModel()->selectionModel()->currentIndex();
   return Session::instance()->accountModel()->getAccountByModelIndex(accIdx);
}

QList<Account*> AccountModel::accountsToMigrate() const
{
    QList<Account*> accounts;
    foreach(Account* account, d_ptr->m_lAccounts) {
        if (account->needsMigration())
            accounts << account;
    }
    return accounts;
}

/**
 * returns a vector of contacts from the daemon
 * @param account the account to query
 * @return contacts a QVector<QMap<QString, QString>>, keywords : "id" and "added"
 */
/*QVector<QMap<QString, QString>>
AccountModel::getContacts(const Account* account) const
{
    return ConfigurationManager::instance().getContacts(account->id().data());
}*/

Account::AutoAnswerStatus AccountModel::globalAutoAnswerStatus() const
{
    //FIXME implement properly
    for (Account* account : qAsConst(d_ptr->m_lAccounts))
        return account->autoAnswerStatus();

    return Account::AutoAnswerStatus::MANUAL;
}

void AccountModel::setGlobalAutoAnswerStatus(Account::AutoAnswerStatus s)
{
    for (Account* account : qAsConst(d_ptr->m_lAccounts))
        account->setAutoAnswerStatus(s);
}

/// Mutualize all the connections
void AccountModelPrivate::connectAccount(Account* acc)
{
    connect(acc, &Account::changed                , this, &AccountModelPrivate::slotAccountChanged                );
    connect(acc, &Account::presenceEnabledChanged , this, &AccountModelPrivate::slotAccountPresenceEnabledChanged );
    connect(acc, &Account::enabled                , this, &AccountModelPrivate::slotSupportedProtocolsChanged     );
    connect(acc, &Account::canCallChanged         , this, &AccountModelPrivate::slotHasMediaCodecChanged          );
    connect(acc, &Account::canVideoCallChanged    , this, &AccountModelPrivate::slotHasMediaCodecChanged          );
    connect(acc, &Account::enabled                , this, &AccountModelPrivate::slotAvailabilityStatusChanged     );
    connect(acc, &Account::autoAnswerStatusChanged, this, [this]() {
        emit q_ptr->autoAnswerStatusChanged();
    }); //FIXME
}

///Account status changed
void AccountModelPrivate::slotDaemonAccountChanged(const QString& account, const QString& registration_state, unsigned code, const QString& status)
{
   Q_UNUSED(registration_state);
   Account* a = q_ptr->getById(account.toLatin1());

   //TODO move this to AccountStatusModel
   if (!a || (a && a->lastSipRegistrationStatus() != status )) {
      if (status != QLatin1String("OK")) //Do not pollute the log
         qDebug() << "Account" << account << "status changed to" << status;
   }

   if (a)
      a->d_ptr->setLastSipRegistrationStatus(status);

   ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();

   //The account may have been deleted by the user, but 'apply' have not been pressed
   if (!a) {
      qDebug() << "received account changed for non existing account" << account;
      const QStringList accountIds = configurationManager.getAccountList();

      for (int i = 0; i < accountIds.size(); ++i) {
         if ((!q_ptr->getById(accountIds[i].toLatin1())) && m_lDeletedAccounts.indexOf(accountIds[i]) == -1) {
            Account* acc = AccountPrivate::buildExistingAccountFromId(accountIds[i].toLatin1(), q_ptr);
            qDebug() << "building missing account" << accountIds[i];
            insertAccount(acc,i);
            connectAccount(acc);

            emit q_ptr->accountAdded(acc);

            if (!acc->isIp2ip())
               enableProtocol(acc->protocol());

         }
      }

      // remove any accounts that are not found in the daemon and which are marked to be REMOVED
      // its not clear to me why this would ever happen in the first place, but the code does seem
      // to be able to enter into this state...
      QMutableVectorIterator<Account *> accIter(m_lAccounts);
      int modelRow = 0;
      while (accIter.hasNext()) {
          auto acc = accIter.next();
          const int daemonRow = accountIds.indexOf(acc->id());

          if (daemonRow == -1 && (acc->editState() == Account::EditState::READY || acc->editState() == Account::EditState::REMOVED)) {
              q_ptr->beginRemoveRows(QModelIndex(), modelRow, modelRow);
              accIter.remove();
              q_ptr->endRemoveRows();

              // should we put it in the list of deleted accounts? who knows?

              // decrement which row we're on
              --modelRow;
          }
          // we're going to the next row
          ++modelRow;
      }
   }
   else {
      const bool isRegistered = a->registrationState() == Account::RegistrationState::READY;
      a->d_ptr->updateState();
      const QModelIndex idx = a->index();
      emit q_ptr->dataChanged(idx, idx);
      const bool regStateChanged = isRegistered != (a->registrationState() == Account::RegistrationState::READY);

      //Handle some important events directly
      if (regStateChanged && (code == 502 || code == 503)) {
         emit q_ptr->badGateway();
      }
      else if (regStateChanged)
         emit q_ptr->registrationChanged(a,a->registrationState() == Account::RegistrationState::READY);

      //Send the messages to AccountStatusModel for processing
      a->statusModel()->addSipRegistrationEvent(status,code);

      //Make sure volatile details get reloaded
      //TODO eventually remove this call and trust the signal
      slotVolatileAccountDetailsChange(account,configurationManager.getVolatileAccountDetails(account));

      emit q_ptr->accountStateChanged(a,a->registrationState());
      emit q_ptr->hasAvailableAccountsChanged();
   }

}

void AccountModelPrivate::slotSupportedProtocolsChanged()
{
    emit q_ptr->supportedProtocolsChanged();
}

///Tell the model something changed
void AccountModelPrivate::slotAccountChanged(Account* a)
{
   int idx = m_lAccounts.indexOf(a);
   if (idx != -1) {
      emit q_ptr->dataChanged(q_ptr->index(idx, 0), q_ptr->index(idx, 0));
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/


///When a new voice mail is available
void AccountModelPrivate::slotVoiceMailNotify(const QString &accountID, int count)
{
   Account* a = q_ptr->getById(accountID.toLatin1());
   if (a) {
      a->setVoiceMailCount(count);
      emit q_ptr->voiceMailNotify(a,count);
   }
}

///Propagate account presence state
void AccountModelPrivate::slotAccountPresenceEnabledChanged(bool state)
{
   Q_UNUSED(state)
   emit q_ptr->presenceEnabledChanged(q_ptr->isPresenceEnabled());
}

///Emitted when some runtime details changes
void AccountModelPrivate::slotVolatileAccountDetailsChange(const QString& accountId, const MapStringString& details)
{
   if (auto a = q_ptr->getById(accountId.toLatin1())) {
      const int     transportCode = details[DRing::Account::VolatileProperties::Transport::STATE_CODE].toInt();
      const QString transportDesc = details[DRing::Account::VolatileProperties::Transport::STATE_DESC];
      const QString status        = details[DRing::Account::VolatileProperties::Registration::STATUS];

      a->statusModel()->addTransportEvent(transportDesc,transportCode);

      a->d_ptr->setLastTransportCode(transportCode);
      a->d_ptr->setLastTransportMessage(transportDesc);

      const Account::RegistrationState state = Account::fromDaemonName(
          a->d_ptr->accountDetail(DRing::Account::ConfProperties::Registration::STATUS)
      );

      a->d_ptr->setRegistrationState(state);

      slotAvailabilityStatusChanged();
   }
}

///Known Ring devices have changed
void AccountModelPrivate::slotKownDevicesChanged(const QString& accountId, const MapStringString& accountDevices)
{
   qDebug() << "Known devices changed" << accountId;

   Account* a = q_ptr->getById(accountId.toLatin1());

   if (!a) {
      qWarning() << "Known devices changed for unknown account" << accountId;
      return;
   }

   a->ringDeviceModel()->d_ptr->reload(accountDevices);
}

///Export on Ring ended
void AccountModelPrivate::slotExportOnRingEnded(const QString& accountId, int status, const QString& pin)
{
   qDebug() << "Export on ring ended" << accountId;

   Account* a = q_ptr->getById(accountId.toLatin1());

   if (!a) {
      qWarning() << "export on Ring ended for unknown account" << accountId;
      return;
   }

   emit a->exportOnRingEnded(static_cast<Account::ExportOnRingStatus>(status), pin);
}

void AccountModelPrivate::slotDeviceRevocationEnded(const QString& accountId, const QString& deviceId, int status)
{
    Account* a = q_ptr->getById(accountId.toLatin1());

    if (!a) {
        qWarning() << "device revocation on Ring ended for unknown account" << accountId;
        return;
    }

    a->ringDeviceModel()->d_ptr->revoked(deviceId, status);
}

/// Migration ended
void
AccountModelPrivate::slotMigrationEnded(const QString& accountId, const QString& result)
{
    Account* a = q_ptr->getById(accountId.toLatin1());

    Account::MigrationEndedStatus status;
    if(result == QLatin1String("SUCCESS"))
        status = Account::MigrationEndedStatus::SUCCESS;
    else if(result == QLatin1String("INVALID"))
        status = Account::MigrationEndedStatus::INVALID;
    else
        status = Account::MigrationEndedStatus::UNDEFINED_STATUS;


    if (status == Account::MigrationEndedStatus::UNDEFINED_STATUS)
        qWarning() << "cannot emit migrationEnded signal, status is undefined";
    else
        emit a->migrationEnded(status);
}

/**
 * slot function used with ConfigurationManagerInterface::contactRemoved signal
 */
void
AccountModelPrivate::slotContactRemoved(const QString &accountID, const QString &uri, bool banned)
{
    if (not banned)
        return;

    auto account = q_ptr->getById(accountID.toLatin1());
    auto cm = Session::instance()->individualDirectory()->getNumber(uri, account);
    account->bannedContactModel()->add(cm);
}

void AccountModelPrivate::slotHasMediaCodecChanged(bool status)
{
    auto account = qobject_cast<Account*>(sender());

    emit q_ptr->canCallChanged(account, status);
    emit q_ptr->canVideoCallChanged(account, status);
}

void AccountModelPrivate::slotAvailabilityStatusChanged()
{
    m_HasAvailableAccounts = 0;
    m_HasEnabledAccounts   = 0;
    for (auto a : qAsConst(m_lAccounts)) {
        m_HasAvailableAccounts +=
            a->registrationState() == Account::RegistrationState::READY ? 1 : 0;
        m_HasEnabledAccounts +=
            a->isEnabled() ? 1 : 0;
    }

   emit q_ptr->hasAvailableAccountsChanged();
   emit q_ptr->hasEnabledAccountsChanged();
}

///Update accounts
void AccountModel::update()
{
   ConfigurationManagerInterface & configurationManager = ConfigurationManager::instance();
   QList<Account*> tmp;
   for (int i = 0; i < d_ptr->m_lAccounts.size(); i++)
      tmp << d_ptr->m_lAccounts[i];

   for (int i = 0; i < tmp.size(); i++) {
      Account* current = tmp[i];
      if (!current->isNew() && (current->editState() != Account::EditState::NEW
         && current->editState() != Account::EditState::MODIFIED_COMPLETE
         && current->editState() != Account::EditState::MODIFIED_INCOMPLETE
         && current->editState() != Account::EditState::OUTDATED))
         remove(current);
   }
   //ask for the list of accounts ids to the configurationManager
   const QStringList accountIds = configurationManager.getAccountList();

   for (int i = 0; i < accountIds.size(); ++i) {
      if (d_ptr->m_lDeletedAccounts.indexOf(accountIds[i]) == -1) {
         Account* a = AccountPrivate::buildExistingAccountFromId(accountIds[i].toLatin1(), this);
         d_ptr->insertAccount(a,i);
         emit dataChanged(index(i,0),index(size()-1,0));
         d_ptr->connectAccount(a);

         emit accountAdded(a);

         if (!a->isIp2ip())
            d_ptr->enableProtocol(a->protocol());
      }
   }

   d_ptr->slotAvailabilityStatusChanged();

} //update

///Update accounts
void AccountModel::updateAccounts()
{
   qDebug() << "Updating all accounts";
   ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
   QStringList accountIds = configurationManager.getAccountList();

   // Detect removed accounts
   foreach(Account* account, d_ptr->m_lAccounts) {
       if (accountIds.indexOf(account->id()) == -1) {
           remove(account);
       }
   }

   //m_lAccounts.clear();
   for (int i = 0; i < accountIds.size(); ++i) {
      Account* acc = getById(accountIds[i].toLatin1());
      if (!acc) {
         Account* a = AccountPrivate::buildExistingAccountFromId(accountIds[i].toLatin1(), this);
         d_ptr->insertAccount(a,d_ptr->m_lAccounts.size());
         d_ptr->connectAccount(a);
         emit dataChanged(index(size()-1,0),index(size()-1,0));

         if (!a->isIp2ip())
            d_ptr->enableProtocol(a->protocol());

         emit accountAdded(a);
      }
      else {
         acc->performAction(Account::EditAction::RELOAD);
      }
   }
   emit accountListUpdated();
} //updateAccounts

///Save accounts details and reload it
void AccountModel::save()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
   const QStringList accountIds = QStringList(configurationManager.getAccountList());

   //create or update each account from accountList
   for (int i = 0; i < size(); i++) {
      Account* current = (*this)[i];
      current->performAction(Account::EditAction::SAVE);
   }

   //remove accounts that are in the configurationManager but not in the client
   for (int i = 0; i < accountIds.size(); i++) {
      if(!getById(accountIds[i].toLatin1())) {
         configurationManager.removeAccount(accountIds[i]);
      }
   }

   //Set account order
   QString order;
   for( int i = 0 ; i < size() ; i++)
      order += d_ptr->m_lAccounts[i]->id() + '/';
   configurationManager.setAccountsOrder(order);
   d_ptr->m_lDeletedAccounts.clear();
}

int AccountModel::exportAccounts(const QStringList& accountIDs, const QString& filePath, const QString& password)
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    return configurationManager.exportAccounts(accountIDs, filePath, password);
}

int AccountModel::importAccounts(const QString& filePath, const QString& password)
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    return configurationManager.importAccounts(filePath, password);
}

///Move account up
bool AccountModel::moveUp()
{
   if (d_ptr->m_pSelectionModel) {
      const auto idx = d_ptr->m_pSelectionModel->currentIndex();

      if (!idx.isValid())
         return false;

      if (dropMimeData(mimeData({idx}), Qt::MoveAction, idx.row()-1, idx.column(),idx.parent())) {
         return true;
      }
   }
   return false;
}

///Move account down
bool AccountModel::moveDown()
{
   if (d_ptr->m_pSelectionModel) {
      const auto idx = d_ptr->m_pSelectionModel->currentIndex();

      if (!idx.isValid())
         return false;

      if (dropMimeData(mimeData({idx}), Qt::MoveAction, idx.row()+1, idx.column(),idx.parent())) {
         return true;
      }
   }
   return false;
}

///Try to register all enabled accounts
void AccountModel::registerAllAccounts()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
   configurationManager.registerAllAccounts();
}

///Cancel all modifications
void AccountModel::cancel() {
   foreach (Account* a, d_ptr->m_lAccounts) {
      // Account::EditState::NEW is only for new and unmodified accounts
      if (a->isNew())
         remove(a);
      else {
         switch(a->editState()) {
            case Account::EditState::NEW                :
               remove(a);
               break;
            case Account::EditState::MODIFIED_INCOMPLETE:
            case Account::EditState::MODIFIED_COMPLETE  :
               a << Account::EditAction::CANCEL;
               break;
            case Account::EditState::OUTDATED           :
               a << Account::EditAction::RELOAD;
               break;
            case Account::EditState::READY              :
            case Account::EditState::REMOVED            :
            case Account::EditState::EDITING            :
            case Account::EditState::COUNT__            :
               break;
         }
      }
   }
   d_ptr->m_lDeletedAccounts.clear();
}


void AccountModelPrivate::enableProtocol(Account::Protocol proto)
{
   const bool cache = m_lSupportedProtocols[proto];

   //Set the supported protocol bits, for now, this intentionally ignore account states
   m_lSupportedProtocols.setAt(proto, true);

   if (!cache) {
      emit q_ptr->supportedProtocolsChanged();
   }
}

AccountModel::EditState AccountModelPrivate::convertAccountEditState(const Account::EditState s)
{
   AccountModel::EditState ams = AccountModel::EditState::INVALID;

   switch (s) {
      case Account::EditState::READY              :
      case Account::EditState::OUTDATED           :
      case Account::EditState::EDITING            :
      case Account::EditState::COUNT__            :
         ams = AccountModel::EditState::SAVED;
         break;
      case Account::EditState::MODIFIED_INCOMPLETE:
         ams = AccountModel::EditState::INVALID;
         break;
      case Account::EditState::NEW                :
      case Account::EditState::REMOVED            :
      case Account::EditState::MODIFIED_COMPLETE  :
         ams = AccountModel::EditState::UNSAVED;
         break;
   }

   return ams;
}

///Check if the AccountModel need/can be saved as a whole
AccountModel::EditState AccountModel::editState() const
{
   typedef AccountModel::EditState  ES ;
   typedef const Account::EditState AES;

   static ES s_CurrentState = ES::INVALID;

   //This class is a singleton, so using static variables is ok
   static Matrix1D<ES,int> estates = {
      { ES::SAVED   , 0},
      { ES::UNSAVED , 0},
      { ES::INVALID , 0},
   };

   auto genState = [this]( const Account* a, AES s, AES p ) {
      Q_UNUSED(a)

      const ES newState = d_ptr->convertAccountEditState(s);
      const ES oldState = d_ptr->convertAccountEditState(p);

      if (newState != oldState)
         estates.setAt(oldState,estates[oldState]-1);

      estates.setAt(newState,estates[newState]+1);

      const ES oldGlobalState = s_CurrentState;

      s_CurrentState = estates[ES::INVALID] ? ES::INVALID:
                       estates[ES::UNSAVED] ? ES::UNSAVED:
                                              ES::SAVED  ;

      if (s_CurrentState != oldGlobalState)
         emit editStateChanged(s_CurrentState, oldGlobalState);

   };

   static bool isInit = false;
   if (!isInit) {
      isInit = true;

      for (const Account* a : d_ptr->m_lAccounts) {
         genState(a,a->editState(),a->editState());
      }

      connect(this, &AccountModel::accountEditStateChanged, genState);
   }


   return s_CurrentState;
}

///Called when codec bitrate changes
void AccountModelPrivate::slotMediaParametersChanged(const QString& accountId)
{
   Account* a = q_ptr->getById(accountId.toLatin1());
   if (a) {
      if (auto codecModel = a->codecModel()) {
         qDebug() << "reloading codecs";
         codecModel << CodecModel::EditAction::RELOAD;
      }
   }
}

/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

/**
 * Get an account by its ID
 *
 * @note This method have O(N) complexity, but the average account count is low
 *
 * @param id The account identifier
 * @param usePlaceHolder Return a placeholder for a future account instead of nullptr
 * @return an account if it exist, a placeholder if usePlaceHolder==true or nullptr
 */
Account* AccountModel::getById(const QByteArray& id, bool usePlaceHolder) const
{
   if (id.isEmpty())
       return nullptr;
   //This function use a loop as the expected size is < 5
   for (int i = 0; i < d_ptr->m_lAccounts.size(); i++) {
      Account* acc = d_ptr->m_lAccounts[i];
      if (acc && !acc->isNew() && acc->id() == id)
         return acc;
   }

   //The account doesn't exist (yet)
   if (usePlaceHolder) {
      AccountPlaceHolder* ph =  d_ptr->m_hsPlaceHolder[id];
      if (!ph) {
         ph = new AccountPlaceHolder(id);
         d_ptr->m_hsPlaceHolder[id] = ph;
      }
      return ph;
   }

   return nullptr;
}

///Get the account size
int AccountModel::size() const
{
   return d_ptr->m_lAccounts.size();
}

///Get data from the model
QVariant AccountModel::data ( const QModelIndex& idx, int role) const
{
   if (!idx.isValid() || idx.row() < 0 || idx.row() >= rowCount())
      return QVariant();

   return d_ptr->m_lAccounts[idx.row()]->roleData(role);
} //data

///Flags for "idx"
Qt::ItemFlags AccountModel::flags(const QModelIndex& idx) const
{
   if (idx.column() == 0)
      return QAbstractItemModel::flags(idx) | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
   return QAbstractItemModel::flags(idx);
}

///Number of account
int AccountModel::rowCount(const QModelIndex& parentIdx) const
{
   return parentIdx.isValid() ? 0 : d_ptr->m_lAccounts.size();
}

Account* AccountModel::getAccountByModelIndex(const QModelIndex& item) const
{
   if (!item.isValid())
      return nullptr;
   return d_ptr->m_lAccounts[item.row()];
}

///Generate an unique suffix to prevent multiple account from sharing alias
QString AccountModel::getSimilarAliasIndex(const QString& alias)
{
    auto self = Session::instance()->accountModel();

    int count = 0;
    foreach (Account* a, self->d_ptr->m_lAccounts) {
        if (a->alias().left(alias.size()) == alias)
            count++;
    }
    bool found = true;
    do {
        found = false;
        foreach (Account* a, self->d_ptr->m_lAccounts) {
            if (a->alias() == alias+QStringLiteral(" (%1)").arg(count)) {
                count++;
                found = false;
                break;
            }
        }
    } while(found);
    if (count)
        return QStringLiteral(" (%1)").arg(count);
    return QString();
}

QList<Account*> AccountModel::getAccountsByProtocol( const Account::Protocol protocol ) const
{
   switch(protocol) {
      case Account::Protocol::SIP:
         return d_ptr->m_lSipAccounts;
      case Account::Protocol::RING:
         return d_ptr->m_lRingAccounts;
      case Account::Protocol::COUNT__:
         break;
   }

   return {};
}

bool AccountModel::isPresenceEnabled() const
{
   foreach(Account* a, d_ptr->m_lAccounts) {
      if (a->presenceEnabled())
         return true;
   }
   return false;
}

bool AccountModel::isPresencePublishSupported() const
{
   foreach(Account* a,d_ptr->m_lAccounts) {
      if (a->supportPresencePublish())
         return true;
   }
   return false;
}

bool AccountModel::isPresenceSubscribeSupported() const
{
   foreach(Account* a,d_ptr->m_lAccounts) {
      if (a->supportPresenceSubscribe())
         return true;
   }
   return false;
}

bool AccountModel::isSipSupported() const
{
   return d_ptr->m_lSupportedProtocols[Account::Protocol::SIP];
}

bool AccountModel::isIP2IPSupported() const
{
    if (auto a = ip2ip())
        return a->isEnabled();
    return false;
}

bool AccountModel::isRingSupported() const
{
   return d_ptr->m_lSupportedProtocols[Account::Protocol::RING];
}

/**
 * Returns true if more than one account can be used for the same URI.
 *
 * This happens if there is multiple ring accounts or multiple SIP accounts.
 *
 * It is useful, for examples, to show the account name alongside each CM when
 * there is an ambiguity, but otherwise hide it.
 */
bool AccountModel::hasAmbiguousAccounts() const
{
    switch(d_ptr->m_lAccounts.size()) {
        case 0:
        case 1:
            return false;
        case 2:
            return !hasMultipleProtocols();
    }

    return true;
}

bool AccountModel::hasMultipleProtocols() const
{
    return d_ptr->m_lSupportedProtocols[Account::Protocol::RING] &&
        d_ptr->m_lSupportedProtocols[Account::Protocol::SIP ];
}

bool AccountModel::hasAvailableAccounts() const
{
    return d_ptr->m_HasAvailableAccounts;
}

bool AccountModel::hasEnabledAccounts() const
{
    return d_ptr->m_HasEnabledAccounts;
}


ProtocolModel* AccountModel::protocolModel() const
{
   if (!d_ptr->m_pProtocolModel)
      d_ptr->m_pProtocolModel = new ProtocolModel();
   return d_ptr->m_pProtocolModel;
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Have a single place where m_lAccounts receive inserts
void AccountModelPrivate::insertAccount(Account* a, int idx)
{
   q_ptr->beginInsertRows(QModelIndex(), idx, idx);
   m_lAccounts.insert(idx,a);
   q_ptr->endInsertRows();

   connect(a,&Account::editStateChanged, a, [a,this](const Account::EditState state, const Account::EditState previous) {
      emit q_ptr->accountEditStateChanged(a, state, previous);
   });

   // Connect the signal when a contact was added by an account
   connect(a, &Account::contactRequestAccepted, a, [a, this](const ContactRequest* r){
      emit q_ptr->accountContactAdded(a, r);
   });

   switch(a->protocol()) {
      case Account::Protocol::SIP:
         m_lSipAccounts  << a;
         break;
      case Account::Protocol::RING:
         m_lRingAccounts << a;
         break;
      case Account::Protocol::COUNT__:
         break;
   }
}

void AccountModelPrivate::removeAccount(Account* account)
{
   const int aindex = m_lAccounts.indexOf(account);

   q_ptr->beginRemoveRows(QModelIndex(),aindex,aindex);
   m_lAccounts.remove(aindex);
   m_lDeletedAccounts << account->id();
   q_ptr->endRemoveRows();

   m_pRemovedAccounts << account;

   switch(account->protocol()) {
      case Account::Protocol::RING:
         m_lRingAccounts.removeOne(account);
         break;
      case Account::Protocol::SIP:
         m_lSipAccounts.removeOne(account);
         break;
      case Account::Protocol::COUNT__:
         break;
   }
}

Account* AccountModel::add(const QString& alias, const Account::Protocol proto)
{
   Account* a = AccountPrivate::buildNewAccountFromAlias(proto, alias, this);
   d_ptr->insertAccount(a,d_ptr->m_lAccounts.size());
   d_ptr->connectAccount(a);

   emit dataChanged(index(d_ptr->m_lAccounts.size()-1,0), index(d_ptr->m_lAccounts.size()-1,0));

   if (d_ptr->m_pSelectionModel) {
      d_ptr->m_pSelectionModel->setCurrentIndex(index(d_ptr->m_lAccounts.size()-1,0), QItemSelectionModel::ClearAndSelect);
   }

   if (!a->isIp2ip())
      d_ptr->enableProtocol(proto);

// Override ringtone path
#if defined(Q_OS_OSX)
    QDir ringtonesDir(QCoreApplication::applicationDirPath());
    ringtonesDir.cdUp();
    ringtonesDir.cd("Resources/ringtones/");
    a->setRingtonePath(ringtonesDir.path()+"/default.wav");
#endif

   emit accountAdded(a);

   editState();

   return a;
}

Account* AccountModel::add(const QString& alias, const QModelIndex& idx)
{
   return add(alias, qvariant_cast<Account::Protocol>(idx.data((int)ProtocolModel::Role::Protocol)));
}

///Remove an account
void AccountModel::remove(Account* account)
{
  if (not account) {
    return;
  }
  qDebug() << "Removing" << account->alias() << account->id();
  d_ptr->removeAccount(account);
  emit accountRemoved(account);
}

void AccountModel::remove(const QModelIndex& idx )
{
   remove(getAccountByModelIndex(idx));
}

///Set model data
bool AccountModel::setData(const QModelIndex& idx, const QVariant& value, int role)
{
   if (idx.isValid() && idx.column() == 0 && role == Qt::CheckStateRole) {
      const bool prevEnabled = d_ptr->m_lAccounts[idx.row()]->isEnabled();
      d_ptr->m_lAccounts[idx.row()]->setEnabled(value.toBool());
      emit dataChanged(idx, idx);
      if (prevEnabled != value.toBool())
         emit accountEnabledChanged(d_ptr->m_lAccounts[idx.row()]);
      emit dataChanged(idx, idx);
      return true;
   }
   else if ( role == Qt::EditRole ) {
      if (value.toString() != data(idx,Qt::EditRole)) {
         d_ptr->m_lAccounts[idx.row()]->setAlias(value.toString());
         emit dataChanged(idx, idx);
      }
   }
   else if (idx.isValid()) {
      if (auto a = getAccountByModelIndex(idx))
          return a->setRoleData(role, value);
   }

   return false;
}


/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Get the account from its index
const Account* AccountModel::operator[] (int i) const
{
   return d_ptr->m_lAccounts[i];
}

///Get the account from its index
Account* AccountModel::operator[] (int i)
{
   return d_ptr->m_lAccounts[i];
}

///Get accoutn by id
Account* AccountModel::operator[] (const QByteArray& i) {
   return getById(i);
}

void AccountModel::add(Account* acc)
{
   d_ptr->insertAccount(acc,d_ptr->m_lAccounts.size());
   emit accountAdded(acc);
}


/*****************************************************************************
 *                                                                           *
 *                              Drag and drop                                *
 *                                                                           *
 ****************************************************************************/


QStringList AccountModel::mimeTypes() const
{
   return d_ptr->m_lMimes;
}

bool AccountModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
   Q_UNUSED(action)
   if(parent.isValid() || column > 0) {
      qDebug() << "column invalid";
      return false;
   }

   if (data->hasFormat(RingMimes::ACCOUNT)) {
      int destinationRow = -1;

      if(row < 0) {
         //drop on top
         destinationRow = d_ptr->m_lAccounts.size() - 1;
      }
      else if(row >= d_ptr->m_lAccounts.size()) {
         destinationRow = 0;
      }
      else {
         destinationRow = row;
      }

      Account* dest = getById(data->data(RingMimes::ACCOUNT));
      if (!dest)
         return false;

      const QModelIndex accIdx = dest->index();

      beginRemoveRows(QModelIndex(), accIdx.row(), accIdx.row());
      Account* acc = d_ptr->m_lAccounts[accIdx.row()];
      d_ptr->m_lAccounts.removeAt(accIdx.row());
      endRemoveRows();

      d_ptr->insertAccount(acc,destinationRow);

      d_ptr->m_pSelectionModel->setCurrentIndex(index(destinationRow), QItemSelectionModel::ClearAndSelect);

      return true;
   }

   return false;
}

QMimeData* AccountModel::mimeData(const QModelIndexList& indexes) const
{
   QMimeData* mMimeData = new QMimeData();

   for (const QModelIndex& index : indexes) {
      if (index.isValid()) {
         mMimeData->setData(RingMimes::ACCOUNT, index.data((int)Account::Role::Id).toByteArray());
      }
   }

   return mMimeData;
}

Qt::DropActions AccountModel::supportedDragActions() const
{
   return Qt::MoveAction | Qt::TargetMoveAction;
}

Qt::DropActions AccountModel::supportedDropActions() const
{
   return Qt::MoveAction | Qt::TargetMoveAction;
}

void AccountModel::slotConnectivityChanged()
{
    ConfigurationManager::instance().connectivityChanged();
}

Account* AccountModel::findPlaceHolder(const QByteArray& accountId) const
{
    auto iter = d_ptr->m_hsPlaceHolder.find(accountId);
    if (iter != d_ptr->m_hsPlaceHolder.end())
        return *iter;
    return nullptr;
}

Account* AccountModel::findAccountIf(const std::function<bool(const Account&)>& pred) const
{
    auto iter = std::find_if(std::begin(d_ptr->m_lAccounts), std::end(d_ptr->m_lAccounts),
                             [&](Account* acc){ return acc and pred(*acc); });
    if (iter != std::end(d_ptr->m_lAccounts))
        return *iter;
    return nullptr;
}
