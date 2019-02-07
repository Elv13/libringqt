/***************************************************************************
 *   Copyright (C) 2019 2019 by Blue Systems                               *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "ringaccountbuilder.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>
#include <QtCore/QTimer>

// LRC
#include <account.h>
#include <session.h>
#include <profilemodel.h>
#include <accountmodel.h>
#include <libcard/matrixutils.h>

using NDLS = NameDirectory::LookupStatus;

class RingAccountBuilderPrivate final : public QObject
{
    Q_OBJECT
public:
    enum class State {
        NEW             ,
        CREATING        ,
        CREATE_TIMEOUT  ,
        CREATED         ,
        REGISTERING     ,
        REGISTER_TIMEOUT,
        REGISTER_ERROR  ,
        REGISTER_SUCCESS,
        COUNT__,
    } m_State {State::NEW};

    struct TimedEvent {
        bool m_IsError     ;
        bool m_NeedsDisplay;
        int  m_Timeout     ;
    };

    NDLS     m_CurrentStatus      {NDLS::INVALID_NAME};
    QString  m_Password           {                  };
    QString  m_RepeatPassword     {                  };
    QString  m_RegisteredRingName {                  };
    bool     m_RegisterRingName   {       true       };
    bool     m_UsePassword        {       false      };
    bool     m_IsCreating         {       false      };
    bool     m_IsLooking          {       false      };
    Account* m_pAccount           {      nullptr     };
    QTimer*  m_pTimer             {      nullptr     };

    void changeState();

    RingAccountBuilder* q_ptr;
public Q_SLOTS:
    void slotAccountStateChanged();
    void slotRegisteredNameFound(Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name);
    void slotNameRegistrationEnded(Account* account, NameDirectory::RegisterNameStatus status, const QString& name);
};

// How long to display the popup
static const Matrix1D<RingAccountBuilderPrivate::State, RingAccountBuilderPrivate::TimedEvent> eventTimeout = {
    { RingAccountBuilderPrivate::State::NEW             , {true , false, 0} },
    { RingAccountBuilderPrivate::State::CREATING        , {true , false, 0} },
    { RingAccountBuilderPrivate::State::CREATE_TIMEOUT  , {true , false, 0} },
    { RingAccountBuilderPrivate::State::CREATED         , {true , false, 0} },
    { RingAccountBuilderPrivate::State::REGISTERING     , {true , false, 0} },
    { RingAccountBuilderPrivate::State::REGISTER_TIMEOUT, {true , false, 0} },
    { RingAccountBuilderPrivate::State::REGISTER_ERROR  , {true , false, 0} },
    { RingAccountBuilderPrivate::State::REGISTER_SUCCESS, {true , false, 0} },
};

RingAccountBuilder::RingAccountBuilder(QObject* parent) : QObject(parent),
    d_ptr(new RingAccountBuilderPrivate())
{
    d_ptr->q_ptr = this;

    connect(Session::instance()->nameDirectory(), &NameDirectory::registeredNameFound,
        d_ptr, &RingAccountBuilderPrivate::slotRegisteredNameFound);
    connect(Session::instance()->nameDirectory(), &NameDirectory::nameRegistrationEnded,
        d_ptr, &RingAccountBuilderPrivate::slotNameRegistrationEnded);
}

RingAccountBuilder::~RingAccountBuilder()
{
    delete d_ptr;
}

// Save again when first time they are registered to make sure dring.yml has
// the key path saved on disk.
void RingAccountBuilderPrivate::slotAccountStateChanged()
{
    using RS = Account::RegistrationState;

    auto a = qobject_cast<Account*>(sender());
    if (!a)
        return;

    if (a->registrationState() == RS::READY) {
        Session::instance()->accountModel()->save();
        disconnect(a, &Account::stateChanged, this, &RingAccountBuilderPrivate::slotAccountStateChanged);

        m_IsCreating = false;
        emit q_ptr->changed();
    }
}

void RingAccountBuilderPrivate::slotRegisteredNameFound(Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name)
{
    if (name != m_RegisteredRingName)
        return;

    m_CurrentStatus = status;

    m_IsLooking = false;

    emit q_ptr->changed();
}

void RingAccountBuilderPrivate::slotNameRegistrationEnded(Account* account, NameDirectory::RegisterNameStatus status, const QString& name)
{
    m_IsCreating = false;
    emit q_ptr->changed();
    switch(status) {
        case NameDirectory::RegisterNameStatus::SUCCESS:
            emit q_ptr->finished();
            break;
        case NameDirectory::RegisterNameStatus::WRONG_PASSWORD:
        case NameDirectory::RegisterNameStatus::INVALID_NAME:
        case NameDirectory::RegisterNameStatus::ALREADY_TAKEN:
        case NameDirectory::RegisterNameStatus::NETWORK_ERROR:
            break;
    }
}

void RingAccountBuilder::create()
{
    Account* a = Session::instance()->accountModel()->add(
        tr("New profile"), protocol()
    );

    QModelIndex accIdx = Session::instance()->profileModel()->accountIndex(a);
    accIdx = static_cast<QAbstractProxyModel*>(
        Session::instance()->profileModel()->sortedProxyModel()
    )->mapFromSource(accIdx);

    Session::instance()->profileModel()->sortedProxySelectionModel()->setCurrentIndex(
        accIdx, QItemSelectionModel::ClearAndSelect
    );

    // Yes, QML somehow takes ownership by default, don't ask me why
    const auto ctx = QQmlEngine::contextForObject(this);
    ctx->engine()->setObjectOwnership(a, QQmlEngine::CppOwnership);

    Session::instance()->accountModel()->save();
    connect(a, &Account::stateChanged, d_ptr, &RingAccountBuilderPrivate::slotAccountStateChanged);
    d_ptr->m_IsCreating = true;

    d_ptr->m_pAccount = a;

    emit changed();
}

void RingAccountBuilder::commit()
{
    if (!canCreate()) {
        Q_ASSERT(false);
        return;
    }

    Q_ASSERT(d_ptr->m_pAccount);

    if (usePassword())
        d_ptr->m_pAccount->setArchivePassword(d_ptr->m_Password);

    if (d_ptr->m_RegisterRingName) {
        Session::instance()->nameDirectory()->registerName(
            d_ptr->m_pAccount, d_ptr->m_Password, d_ptr->m_RegisteredRingName
        );
        d_ptr->m_IsCreating = true;
    }
    else
        emit finished();

    emit changed();
}

void RingAccountBuilder::cancel()
{
    if (!d_ptr->m_pAccount)
        return;

    Session::instance()->accountModel()->remove(d_ptr->m_pAccount);
    Session::instance()->accountModel()->save();
}

bool RingAccountBuilder::isCreating() const
{
    return d_ptr->m_IsCreating;
}

bool RingAccountBuilder::canCreate() const
{
    return (protocol() != Account::Protocol::RING) || (
        passwordMatch() &&
        ((!d_ptr->m_RegisterRingName) || (
            d_ptr->m_RegisteredRingName.size() >= 3 && nameStatus() == NDLS::NOT_FOUND
        )
    ));
}

bool RingAccountBuilder::nameLookup() const
{
    return d_ptr->m_IsLooking;
}

bool RingAccountBuilder::hasNameLookupError() const
{
    if (d_ptr->m_IsLooking)
        return false;

    const int s = d_ptr->m_RegisteredRingName.size();
    return (s && s < 3) || nameStatus() == NDLS::SUCCESS || nameStatus() == NDLS::ERROR;
}

bool RingAccountBuilder::passwordMatch() const
{
    return (!d_ptr->m_UsePassword) || d_ptr->m_Password == d_ptr->m_RepeatPassword;
}

NDLS RingAccountBuilder::nameStatus() const
{
    return d_ptr->m_CurrentStatus;
}

QString RingAccountBuilder::nameStatusMessage() const
{
    if (d_ptr->m_RegisteredRingName.isEmpty())
        return tr("Please enter an username");

    if (d_ptr->m_IsLooking)
        return tr("Checking availability");

    switch(nameStatus()) {
        case NDLS::SUCCESS:
            return tr("The username is already taken");
        case NDLS::INVALID_NAME:
            return tr("Please enter an username (3 characters minimum)");
        case NDLS::NOT_FOUND:
            return tr("The username is available");
        case NDLS::ERROR:
            return tr("The registered name lookup failed, ignoring");
    }

    return tr("Error");
}

QString RingAccountBuilder::creationStatusMessage() const
{
    //FIXME
    return tr(
                "Creating your anonymous cryptographic identity, this can take some time"
            );

    switch(d_ptr->m_State) {
        case RingAccountBuilderPrivate::State::NEW:
            return {};
        case RingAccountBuilderPrivate::State::CREATING:
            return tr(
                "Creating your anonymous cryptographic identity, this can take some time"
            );
        case RingAccountBuilderPrivate::State::CREATE_TIMEOUT:
            return tr(
                "Failed to create the cryptographic identity, your device is too slow"
            );
        case RingAccountBuilderPrivate::State::CREATED:
            return {};
        case RingAccountBuilderPrivate::State::REGISTERING:
            return tr(
                "Adding your username to the blockchain"
            );
        case RingAccountBuilderPrivate::State::REGISTER_TIMEOUT:
            return tr(
                "Registering you username took too long, check your Internet connection"
            );
        case RingAccountBuilderPrivate::State::REGISTER_ERROR:
            //TODO
            return {};
        case RingAccountBuilderPrivate::State::REGISTER_SUCCESS:
            return tr(
                "You are now "
            ) + d_ptr->m_pAccount->registeredName();
    }

    return {};
}

Account::Protocol RingAccountBuilder::protocol() const
{
    return Account::Protocol::RING;
}

void RingAccountBuilder::setProtocol(Account::Protocol)
{
    Q_ASSERT(false); //TODO
    emit changed();
}

bool RingAccountBuilder::registerRingName() const
{
    return d_ptr->m_RegisterRingName;
}

void RingAccountBuilder::setRegisterRingName(bool v)
{
    d_ptr->m_RegisterRingName = v;

    if (!d_ptr->m_RegisteredRingName.isEmpty()) {
        Session::instance()->nameDirectory()->lookupName(
            nullptr, "", d_ptr->m_RegisteredRingName
        );
        d_ptr->m_CurrentStatus = NDLS::ERROR;
        d_ptr->m_IsLooking = true;
    }

    emit changed();
}

bool RingAccountBuilder::usePassword() const
{
    return d_ptr->m_UsePassword || d_ptr->m_RegisterRingName;
}

void RingAccountBuilder::setUsePassword(bool v)
{
    d_ptr->m_UsePassword = v;
    emit changed();
}

QString RingAccountBuilder::password() const
{
    return d_ptr->m_Password;
}

void RingAccountBuilder::setPassword(const QString& passwd)
{
    d_ptr->m_Password = passwd;
    emit changed();
}

QString RingAccountBuilder::repeatPassword() const
{
    return d_ptr->m_RepeatPassword;
}

void RingAccountBuilder::setRepeatPassword(const QString& passwd)
{
    d_ptr->m_RepeatPassword = passwd;
    emit changed();
}

Account* RingAccountBuilder::account() const
{
    return d_ptr->m_pAccount;
}

QString RingAccountBuilder::registeredRingName() const
{
    return d_ptr->m_RegisteredRingName;
}

void RingAccountBuilder::setRegisteredRingName(const QString& name)
{
    d_ptr->m_RegisteredRingName = name;

    if (d_ptr->m_RegisterRingName && !d_ptr->m_RegisteredRingName.isEmpty()) {
        Session::instance()->nameDirectory()->lookupName(
            nullptr, "", d_ptr->m_RegisteredRingName
        );
        d_ptr->m_CurrentStatus = NDLS::ERROR;
        d_ptr->m_IsLooking = true;
    }

    emit changed();
}

void RingAccountBuilderPrivate::changeState()
{
    //TODO timer stuff
}

#include <ringaccountbuilder.moc>

// kate: space-indent on; indent-width 4; replace-tabs on;
