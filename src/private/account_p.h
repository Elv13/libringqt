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

//Qt
#include <QtCore/QObject>
#include <QtCore/QHash>

//Ring
#include <account.h>
#include <libcard/matrixutils.h>

class AccountPrivate;
class ContactMethod;
class CipherModel;
class AccountStatusModel;
class TlsMethodModel;
class ProtocolModel;
class NetworkInterfaceModel;
class BootstrapModel;
class DaemonCertificateCollection;
class PendingContactRequestModel;
class Calendar;

typedef void (AccountPrivate::*account_function)();

class AccountPrivate final : public QObject
{
   Q_OBJECT
   Q_DECLARE_PUBLIC(Account)
public:

    class RegistrationEnabled {
        public:
            constexpr static const char* YES  = "true";
            constexpr static const char* NO   = "false";
    };

    friend class AccountPlaceHolder;

    //Constructor
    explicit AccountPrivate(Account* acc);

    //Attributes
    QByteArray                 m_AccountId                ;
    QHash<QString,QString>     m_hAccountDetails          ;
    QString                    m_LastTransportMessage     ;
    QString                    m_LastSipRegistrationStatus;
    ContactMethod*             m_pAccountNumber           {nullptr};
    uint                       m_InternalId               ; //init in .cpp
    Account*                   q_ptr                      {nullptr};
    bool                       m_isLoaded                 {true};
    int                        m_LastTransportCode        {0};
    Account::RegistrationState m_RegistrationState        {Account::RegistrationState::UNREGISTERED};
    unsigned short             m_UseDefaultPort           {false};
    bool                       m_RemoteEnabledState       {false};
    Account::EditState         m_CurrentState             {Account::EditState::READY};
    Account::Protocol          m_Protocol                 {Account::Protocol::COUNT__};
    QMetaObject::Connection    m_cTlsCert                 {};
    QMetaObject::Connection    m_cTlsCaCert               {};
    Account::ContactMethods    m_NumbersFromDaemon        {};

    //Factory
    static Account* buildExistingAccountFromId(const QByteArray& _accountId);
    static Account* buildNewAccountFromAlias  (Account::Protocol proto, const QString& alias);

    //Setters
    void setAccountProperties(const QHash<QString,QString>& m);
    bool setAccountProperty(const QString& param, const QString& val);
    void setRegistrationState(Account::RegistrationState value);
    void setId(const QByteArray& id);
    void setLastSipRegistrationStatus(const QString& value);
    void setLastTransportCode(int value);
    void setLastTransportMessage(const QString& value);

    //Getters
    QString accountDetail(const QString& param) const;

    //Mutator
    bool merge(Account* account);

    //Helpers
    inline void changeState(Account::EditState state);
    void regenSecurityValidation();
    bool updateState();
    QString buildUri();

    //State actions
    void performAction(Account::EditAction action);
    void nothing();
    void edit   ();
    void modify ();
    void remove ();
    void cancel ();
    void outdate();
    void reload ();
    void save   ();
    void reloadMod() {reload();modify();}

    CredentialModel*             m_pCredentials                {nullptr};
    CodecModel*                  m_pCodecModel                 {nullptr};
    KeyExchangeModel*            m_pKeyExchangeModel           {nullptr};
    CipherModel*                 m_pCipherModel                {nullptr};
    AccountStatusModel*          m_pStatusModel                {nullptr};
    SecurityEvaluationModel*     m_pSecurityEvaluationModel    {nullptr};
    TlsMethodModel*              m_pTlsMethodModel             {nullptr};
    ProtocolModel*               m_pProtocolModel              {nullptr};
    BootstrapModel*              m_pBootstrapModel             {nullptr};
    RingDeviceModel*             m_pRingDeviceModel            {nullptr};
    QAbstractItemModel*          m_pKnownCertificates          {nullptr};
    QAbstractItemModel*          m_pBannedCertificates         {nullptr};
    QAbstractItemModel*          m_pAllowedCertificates        {nullptr};
    NetworkInterfaceModel*       m_pNetworkInterfaceModel      {nullptr};
    DaemonCertificateCollection* m_pAllowedCerts               {nullptr};
    DaemonCertificateCollection* m_pBannedCerts                {nullptr};
    BannedContactModel*          m_pBannedContactModel         {nullptr};
    PendingContactRequestModel*  m_pPendingContactRequestModel {nullptr};
    Calendar*                    m_pCalendar                   {nullptr};

    QHash<int, Account::RoleStatus> m_hRoleStatus;

    // State machines
    static const Matrix2D<Account::EditState, Account::EditAction, account_function> stateMachineActionsOnState;

    //Cached account details (as they are called too often for the hash)
    mutable QString      m_HostName;
    mutable int          m_LastErrorCode {-1};
    mutable int          m_VoiceMailCount {0};
    mutable Certificate* m_pCaCert  {nullptr};
    mutable Certificate* m_pTlsCert {nullptr};

public Q_SLOTS:
      void slotPresentChanged        (bool  present  );
      void slotPresenceMessageChanged(const QString& );
      void slotUpdateCertificate     (               );
      void slotHasMediaCodecChanged  (               );
};
