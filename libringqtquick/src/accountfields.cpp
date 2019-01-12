/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                            *
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
#include "accountfields.h"

#include <account.h>
#include <session.h>
#include <accountmodel.h>

class AttachedAccountFieldStatusPrivate final : public QObject
{
    Q_OBJECT
public:
    explicit AttachedAccountFieldStatusPrivate(QObject* widget);
    virtual ~AttachedAccountFieldStatusPrivate();

    QByteArray     m_Name    {                  };
    Account::Role  m_Role    { (Account::Role)0 };
    AccountFields* m_pParent {      nullptr     };

    bool getAccount();

    AttachedAccountFieldStatus* q_ptr;

public Q_SLOTS:
    void slotAccountEvent();
    void slotAccountChanged();
};

class AccountFieldsPrivate
{
public:
    Account* m_pAccount {nullptr};
};

AttachedAccountFieldStatus::AttachedAccountFieldStatus(QObject* parent) :
    QObject(parent), d_ptr(new AttachedAccountFieldStatusPrivate(this))
{
    Q_ASSERT(parent);
    d_ptr->q_ptr = this;
}

AccountFields::AccountFields(QQuickItem* parent) : QQuickItem(parent),
    d_ptr(new AccountFieldsPrivate())
{
}

AccountFields::~AccountFields()
{
    delete d_ptr;
}

AttachedAccountFieldStatus::~AttachedAccountFieldStatus()
{}

AttachedAccountFieldStatusPrivate::AttachedAccountFieldStatusPrivate(QObject* widget) :
    QObject(widget)
{}


AttachedAccountFieldStatusPrivate::~AttachedAccountFieldStatusPrivate()
{}


bool AttachedAccountFieldStatusPrivate::getAccount()
{
    if ((int)m_Role)
        return true;

    if (m_Name.isEmpty())
        return false;

    auto p = QObject::parent();
    Q_ASSERT(p);

    if (!m_pParent) {
        while (p) {
            if (p->metaObject()->inherits(&AccountFields::staticMetaObject)) {
                m_pParent = qobject_cast<AccountFields*>(p);
                Q_ASSERT(m_pParent);
                break;
            }

            p = p->parent();
        }
    }

    Q_ASSERT(m_pParent);

    if (!m_pParent)
        return false;

    connect(m_pParent, &AccountFields::accountChanged,
        this, &AttachedAccountFieldStatusPrivate::slotAccountChanged);
    connect(m_pParent, &AccountFields::accountStatusChanged,
        this, &AttachedAccountFieldStatusPrivate::slotAccountEvent);

    slotAccountChanged();

    if (Session::instance()->accountModel()->roleNames().key(m_Name) == -1)
        return false;

    m_Role = (Account::Role) Session::instance()->accountModel()->roleNames().key(m_Name);

    return true;
}

QString AttachedAccountFieldStatus::name() const
{
    return d_ptr->m_Name;
}

void AttachedAccountFieldStatus::setName(const QString& n)
{
    if (d_ptr->m_Name == n)
        return;

    d_ptr->m_Name = n.toLatin1();
    emit changed();
}

bool AttachedAccountFieldStatus::isAvailable() const
{
    if (!d_ptr->getAccount())
        return false;

    if (!d_ptr->m_pParent->account())
        return false;

    return d_ptr->m_pParent->account()->roleState(d_ptr->m_Role) !=
        Account::RoleState::UNAVAILABLE;
}

bool AttachedAccountFieldStatus::isReadOnly() const
{
    if (!d_ptr->getAccount())
        return true;

    if (!d_ptr->m_pParent->account())
        return true;

    return d_ptr->m_pParent->account()->roleState(d_ptr->m_Role) ==
        Account::RoleState::READ_ONLY;
}

bool AttachedAccountFieldStatus::isRequired() const
{
    if (!d_ptr->getAccount())
        return false;

    if (!d_ptr->m_pParent->account())
        return false;

    return d_ptr->m_pParent->account()->roleStatus(d_ptr->m_Role) ==
        Account::RoleStatus::REQUIRED_EMPTY;
}

bool AttachedAccountFieldStatus::isValid() const
{
    if (!d_ptr->getAccount())
        return false;

    if (!d_ptr->m_pParent->account())
        return false;

    return d_ptr->m_pParent->account()->roleStatus(d_ptr->m_Role) ==
        Account::RoleStatus::OK;
}

void AttachedAccountFieldStatusPrivate::slotAccountEvent()
{
    emit q_ptr->availableChanged();
}

void AttachedAccountFieldStatusPrivate::slotAccountChanged()
{
    emit q_ptr->changed();
    emit q_ptr->availableChanged();
}

AttachedAccountFieldStatus* AttachedAccountFieldStatus::qmlAttachedProperties(QObject *object)
{
    return new AttachedAccountFieldStatus(object);
}

Account* AccountFields::account() const
{
    return d_ptr->m_pAccount;
}

void AccountFields::setAccount(Account* a)
{
    d_ptr->m_pAccount = a;
    emit accountChanged();
    emit accountStatusChanged();
}

#include <accountfields.moc>
