/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#pragma once

//Qt
#include <QtCore/QObject>
#include <QtCore/QStringList>

//Ring
#include <account.h>
#include <mime.h>
#include <accountmodel.h>
#include "matrixutils.h"
class AccountModel;
class ProtocolModel;
class PendingContactRequestModel;
class QItemSelectionModel;

class AccountModelPrivate final : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(AccountModel)

public:
    //Constructor
    explicit AccountModelPrivate(AccountModel* parent);
    void init();

    //Helpers
    void enableProtocol(Account::Protocol proto);
    AccountModel::EditState convertAccountEditState(const Account::EditState s);
    void insertAccount(Account* a, int idx);
    void removeAccount(Account* account);

    //Attributes
    QItemSelectionModel*              m_pSelectionModel          {nullptr};
    QItemSelectionModel*              m_pUserSelectionModel      {nullptr};
    Account*                          m_pIP2IP                   {nullptr};
    ProtocolModel*                    m_pProtocolModel           {nullptr};
    PendingContactRequestModel*       m_pPendingIncomingRequests {nullptr};

    QVector<Account*>                 m_lAccounts       ;
    QStringList                       m_lDeletedAccounts;
    QList<Account*>                   m_pRemovedAccounts;
    QList<Account*>                   m_lSipAccounts    ;
    QList<Account*>                   m_lRingAccounts   ;
    QMutex                            m_pSingleton      ;

    QStringList                       m_lMimes {{RingMimes::ACCOUNT}};
    Matrix1D<Account::Protocol, bool> m_lSupportedProtocols {{
        { Account::Protocol::SIP  , false},
        { Account::Protocol::RING , false},
    }};

    AccountModel* q_ptr;

    //Future account cache
    static QHash<QByteArray,AccountPlaceHolder*> m_hsPlaceHolder;

public Q_SLOTS:
    void slotDaemonAccountChanged(const QString& account, const QString&  registration_state, unsigned code, const QString& status);
    void slotAccountChanged(Account* a);
    void slotSupportedProtocolsChanged();
    void slotVoiceMailNotify( const QString& accountID , int count );
    void slotAccountPresenceEnabledChanged(bool state);
    void slotVolatileAccountDetailsChange(const QString& accountId, const MapStringString& details);
    void slotMediaParametersChanged(const QString& accountId);
    void slotIncomingContactRequest(const QString& accountId, const QString& hash, const QByteArray& payload, time_t time);
    void slotDeviceRevocationEnded(const QString& accountId, const QString& deviceId, int status);
    void slotKownDevicesChanged(const QString& accountId, const MapStringString& devices);
    void slotExportOnRingEnded(const QString& accountId, int status, const QString& pin);
    void slotMigrationEnded(const QString& accountId, const QString& result);
    void slotContactRemoved(const QString &accountID, const QString &uri, bool banned);
    void slotHasMediaCodecChanged(bool status);
};
