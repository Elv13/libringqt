/************************************************************************************
 *   Copyright (C) 2009 by Savoir-Faire Linux                                       *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>                  *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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

//Parent
#include "Account.h"

//Qt
#include <QtCore/QDebug>
#include <QtCore/QString>

//SFLPhone
#include "sflphone_const.h"
#include "VideoCodec.h"

//SFLPhone lib
#include "configurationmanager_interface_singleton.h"
#include "callmanager_interface_singleton.h"
#include "video_interface_singleton.h"
#include "AccountList.h"

///Match state name to user readable string
const QString& account_state_name(const QString& s)
{
   static const QString registered             = "Registered"               ;
   static const QString notRegistered          = "Not Registered"           ;
   static const QString trying                 = "Trying..."                ;
   static const QString error                  = "Error"                    ;
   static const QString authenticationFailed   = "Authentication Failed"    ;
   static const QString networkUnreachable     = "Network unreachable"      ;
   static const QString hostUnreachable        = "Host unreachable"         ;
   static const QString stunConfigurationError = "Stun configuration error" ;
   static const QString stunServerInvalid      = "Stun server invalid"      ;
   static const QString invalid                = "Invalid"                  ;
   
   if(s == QString(ACCOUNT_STATE_REGISTERED)       )
      return registered             ;
   if(s == QString(ACCOUNT_STATE_UNREGISTERED)     )
      return notRegistered          ;
   if(s == QString(ACCOUNT_STATE_TRYING)           )
      return trying                 ;
   if(s == QString(ACCOUNT_STATE_ERROR)            )
      return error                  ;
   if(s == QString(ACCOUNT_STATE_ERROR_AUTH)       )
      return authenticationFailed   ;
   if(s == QString(ACCOUNT_STATE_ERROR_NETWORK)    )
      return networkUnreachable     ;
   if(s == QString(ACCOUNT_STATE_ERROR_HOST)       )
      return hostUnreachable        ;
   if(s == QString(ACCOUNT_STATE_ERROR_CONF_STUN)  )
      return stunConfigurationError ;
   if(s == QString(ACCOUNT_STATE_ERROR_EXIST_STUN) )
      return stunServerInvalid      ;
   return invalid                   ;
} //account_state_name

///Constructors
Account::Account():m_pAccountId(NULL),m_pAccountDetails(NULL)
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   connect(&callManager,SIGNAL(registrationStateChanged(QString,QString,int)),this,SLOT(accountChanged(QString,QString,int)));
}

///Build an account from it'id
Account* Account::buildExistingAccountFromId(const QString& _accountId)
{
   qDebug() << "Building an account from id: " << _accountId;
   Account* a = new Account();
   a->m_pAccountId = new QString(_accountId);
   a->reload();

   return a;
} //buildExistingAccountFromId

///Build an account from it's name / alias
Account* Account::buildNewAccountFromAlias(const QString& alias)
{
   qDebug() << "Building an account from alias: " << alias;
   Account* a = new Account();
   a->m_pAccountDetails = new MapStringString();
   a->setAccountDetail(ACCOUNT_ALIAS,alias);
   return a;
}

///Destructor
Account::~Account()
{
   disconnect();
   delete m_pAccountId;
   delete m_pAccountDetails;
}


/*****************************************************************************
 *                                                                           *
 *                                   Slots                                   *
 *                                                                           *
 ****************************************************************************/

///Callback when the account state change
void Account::accountChanged(QString accountId,QString state,int)
{
   if (m_pAccountId && accountId == *m_pAccountId) {
      Account::updateState();
      stateChanged(getStateName(state));
   }
}


/*****************************************************************************
 *                                                                           *
 *                                  Getters                                  *
 *                                                                           *
 ****************************************************************************/

///IS this account new
bool Account::isNew() const
{
   return (m_pAccountId == NULL);
}

///Get this account ID
const QString& Account::getAccountId() const
{
   if (isNew())
      qDebug() << "Error : getting AccountId of a new account.";
   if (!m_pAccountId) {
      qDebug() << "Account not configured";
      return EMPTY_STRING; //WARNING May explode
   }
   
   return *m_pAccountId;
}

///Get this account details
const MapStringString& Account::getAccountDetails() const
{
   return *m_pAccountDetails;
}

///Get current state
const QString& Account::getStateName(const QString& state) const
{
   return (const QString&)account_state_name(state);
}

///Get an account detail
const QString& Account::getAccountDetail(const QString& param) const
{
   if (!m_pAccountDetails) {
      qDebug() << "The account list is not set";
      return EMPTY_STRING; //May crash, but better than crashing now
   }
   if (m_pAccountDetails->find(param) != m_pAccountDetails->end()) {
      return (*m_pAccountDetails)[param];
   }
   else if (m_pAccountDetails->count() > 0) {
      qDebug() << "Account paramater \"" << param << "\" not found";
      return EMPTY_STRING;
   }
   else {
      qDebug() << "Account details not found, there is " << m_pAccountDetails->count() << " details available";
      return EMPTY_STRING;
   }
} //getAccountDetail

///Get the alias
const QString& Account::getAlias() const
{
   return getAccountDetail(ACCOUNT_ALIAS);
}

///Is this account enabled
bool Account::isEnabled() const
{
   return (getAccountDetail(ACCOUNT_ENABLED) == REGISTRATION_ENABLED_TRUE);
}

///Is this account registered
bool Account::isRegistered() const
{
   return (getAccountDetail(ACCOUNT_REGISTRATION_STATUS) == ACCOUNT_STATE_REGISTERED);
}


/*****************************************************************************
 *                                                                           *
 *                                  Setters                                  *
 *                                                                           *
 ****************************************************************************/

///Set account details
void Account::setAccountDetails(const MapStringString& m)
{
   *m_pAccountDetails = m;
}

///Set a specific detail
void Account::setAccountDetail(const QString& param, const QString& val)
{
   (*m_pAccountDetails)[param] = val;
}

///Set the account id
void Account::setAccountId(const QString& id)
{
   qDebug() << "Setting accountId = " << m_pAccountId;
   if (! isNew())
      qDebug() << "Error : setting AccountId of an existing account.";
   m_pAccountId = new QString(id);
}

///Set account enabled
void Account::setEnabled(bool checked)
{
   setAccountDetail(ACCOUNT_ENABLED, checked ? REGISTRATION_ENABLED_TRUE : REGISTRATION_ENABLED_FALSE);
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/

///Update the account
void Account::updateState()
{
   if(! isNew()) {
      ConfigurationManagerInterface & configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
      MapStringString details = configurationManager.getAccountDetails(getAccountId()).value();
      QString status = details[ACCOUNT_REGISTRATION_STATUS];
      setAccountDetail(ACCOUNT_REGISTRATION_STATUS, status); //Update -internal- object state
   }
}

///Save the current account to the daemon
void Account::save()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   if (isNew()) {
      MapStringString details = getAccountDetails();
      QString currentId = configurationManager.addAccount(details);
      setAccountId(currentId);
      qDebug() << "NEW ID" << currentId;
   }
   else {
      configurationManager.setAccountDetails(getAccountId(), getAccountDetails());
   }

   //QString id = configurationManager.getAccountDetail(getAccountId());
   if (!getAccountId().isEmpty()) {
      Account* acc =  AccountList::getInstance()->getAccountById(getAccountId());
      qDebug() << "Adding the new account to the account list (" << getAccountId() << ")";
      if (acc != this) {
         (*AccountList::getInstance()->m_pAccounts) << this;
      }
      reload();
      updateState();
   }
}

///Synchronise with the daemon, this need to be done manually to prevent reloading the account while it is being edited
void Account::reload()
{
   qDebug() << "Reloading" << getAccountId();
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QMap<QString,QString> aDetails = configurationManager.getAccountDetails(getAccountId());

   if (!aDetails.count()) {
      qDebug() << "Account not found";
   }
   else {
      if (m_pAccountDetails)
         delete m_pAccountDetails;
      m_pAccountDetails = new MapStringString(aDetails);
   }
}

/*****************************************************************************
 *                                                                           *
 *                                 Operator                                  *
 *                                                                           *
 ****************************************************************************/

///Are both account the same
bool Account::operator==(const Account& a)const
{
   return *m_pAccountId == *a.m_pAccountId;
}

/*****************************************************************************
 *                                                                           *
 *                                   Video                                   *
 *                                                                           *
 ****************************************************************************/
#ifdef ENABLE_VIDEO
void Account::setActiveVideoCodecList(QList<VideoCodec*> codecs)
{
   QStringList codecs;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   foreach(VideoCodec* codec,codecs) {
      codecs << codecs->getName();
   }
   interface.setActiveCodecList(codecs,m_pAccountId);
}

QList<VideoCodec*> Account::getActiveVideoCodecList()
{
   QList<VideoCodec*> codecs;
   VideoInterface& interface = VideoInterfaceSingleton::getInstance();
   foreach (QString codec, interface.getActiveCodecList(m_pAccountId)) {
      codecs << VideoCodec::getCodec(codec);
   }
}

#endif