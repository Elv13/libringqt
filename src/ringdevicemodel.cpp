/****************************************************************************
 *   Copyright (C) 2016 by Savoir-faire Linux                               *
 *   Author : Alexandre Viau <alexandre.viau@savoirfairelinux.com>          *
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
#include "ringdevice.h"
#include "ringdevicemodel.h"
#include "private/ringdevicemodel_p.h"

#include "dbus/configurationmanager.h"

//Qt
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QDirIterator>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStandardPaths>

//Ring daemon
#include <account_const.h>

//Ring
#include "account.h"

RingDeviceModelPrivate::RingDeviceModelPrivate(RingDeviceModel* q, Account* a) : q_ptr(q),m_pAccount(a)
{
}

void RingDeviceModelPrivate::clearLines()
{
    m_pOwnDevice = nullptr;
    q_ptr->beginRemoveRows(QModelIndex(),0,m_lRingDevices.size());
    qDeleteAll(m_lRingDevices);
    m_lRingDevices.clear();
    q_ptr->endRemoveRows();
}

void RingDeviceModelPrivate::reload()
{
    const MapStringString accountDevices = ConfigurationManager::instance().getKnownRingDevices(this->m_pAccount->id());
    reload(accountDevices);
}

void RingDeviceModelPrivate::reload(const MapStringString& accountDevices)
{
    clearLines();

    QMapIterator<QString, QString> i(accountDevices);
    while (i.hasNext()) {
        i.next();

        const bool isSelf = i.key() == m_DevId;

        RingDevice* device = new RingDevice(i.key(), i.value(), m_pAccount, isSelf);

        if (isSelf)
            m_pOwnDevice = device;

        q_ptr->beginInsertRows({} ,m_lRingDevices.size(), m_lRingDevices.size());
        m_lRingDevices << device;
        q_ptr->endInsertRows();
    }
}

void RingDeviceModelPrivate::revoked(const QString& deviceId, int status)
{
    Q_ASSERT(status <= 2 && status >= 0);
    const auto s = static_cast<RingDevice::RevocationStatus>(status);

    auto d = std::find_if(m_lRingDevices.constBegin(), m_lRingDevices.constEnd(), [&deviceId](RingDevice* d) {
        return d->id() == deviceId;
    });

    if (d == m_lRingDevices.constEnd()) {
        qWarning() << "The revoked device has not been found" << deviceId;
        return;
    }

    emit (*d)->revoked(s);
    emit q_ptr->deviceRevoked((*d), s);
}

RingDeviceModel::RingDeviceModel(Account* a, const QString& devId, const QString& devName) : QAbstractTableModel(a), d_ptr(new RingDeviceModelPrivate(this,a))
{
    Q_UNUSED(devName)
    d_ptr->m_DevId = devId;
    d_ptr->reload();
}

RingDeviceModel::~RingDeviceModel()
{
    d_ptr->clearLines();
    delete d_ptr;
}

QHash<int,QByteArray> RingDeviceModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;

    if (!initRoles) {
        initRoles = true;
        roles.insert((int)Roles::NAME   , QByteArray( "name"   ));
        roles.insert((int)Roles::ID     , QByteArray( "id"     ));
        roles.insert((int)Roles::OBJECT , QByteArray( "object" ));
        roles.insert((int)Roles::IS_SELF, QByteArray( "isSelf" ));
    }

    return roles;
}

QVariant RingDeviceModel::data( const QModelIndex& index, int role) const
{
    Q_UNUSED(role);

    if (!index.isValid())
        return {};

    const auto device = d_ptr->m_lRingDevices[index.row()];

    switch (role) {
        case Qt::DisplayRole:
        case (int) Roles::NAME:
            return device->name();
        case (int) Roles::ID:
            return device->id();
        case (int) Roles::OBJECT:
            return QVariant::fromValue(device);
        case (int) Roles::IS_SELF:
            return device->isSelf();
    }

    return {};
}

bool RingDeviceModel::setData(const QModelIndex& index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    if (role == (int) Roles::NAME) {
        auto device = d_ptr->m_lRingDevices[index.row()];

        if (!device->isSelf())
            return false;

        if (device->setName(value.toString())) {
            emit dataChanged(index, index);
            return true;
        }
    }

    return false;
}

QVariant RingDeviceModel::headerData( int section, Qt::Orientation ori, int role) const
{
    Q_UNUSED(section)
    if (role != Qt::DisplayRole || ori != Qt::Horizontal)
        return {};

    return tr("Name");
}

int RingDeviceModel::rowCount( const QModelIndex& parent) const
{
    return parent.isValid()?0:d_ptr->m_lRingDevices.size();
}

int RingDeviceModel::columnCount( const QModelIndex& parent) const
{
    return parent.isValid()?0:1;
}

int RingDeviceModel::size() const
{
    return d_ptr->m_lRingDevices.size();
}

Qt::ItemFlags RingDeviceModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : Qt::NoItemFlags;
}

/// Return the device corresponding to this account
RingDevice* RingDeviceModel::ownDevice() const
{
    return d_ptr->m_pOwnDevice;
}

void RingDeviceModelPrivate::slotNameChanged(const QString& name, const QString& oldName)
{
    Q_UNUSED(name)
    Q_UNUSED(oldName)
    m_pAccount << Account::EditAction::MODIFY;
}
