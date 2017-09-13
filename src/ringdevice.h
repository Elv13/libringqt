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
#pragma once

#include <typedefs.h>
#include <itembase.h>

class RingDevicePrivate;
class Account;

class LIB_EXPORT RingDevice : public ItemBase
{
    Q_OBJECT

    friend class RingDeviceModel;
    friend class RingDeviceModelPrivate;

public:
    enum class RevocationStatus {
        SUCCESS = 0,
        WRONG_PASSWORD = 1,
        UNKNOWN_DEVICE = 2,
    };
    Q_ENUM(RevocationStatus);

    Q_PROPERTY(QString  id      READ id      CONSTANT                        )
    Q_PROPERTY(QString  name    READ name    WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(Account* account READ account CONSTANT                        )
    Q_PROPERTY(bool     isSelf  READ isSelf  CONSTANT                        )

    // Getters
    const QString id     () const;
    const QString name   () const;
    Account*      account() const;
    bool          isSelf () const;

    // Setters
    bool setName(const QString& name);

    // Mutators
    Q_INVOKABLE void revoke(const QString& password);

Q_SIGNALS:
    void nameChanged(const QString& name, const QString& old);
    void revoked(RevocationStatus status);

private:
    explicit RingDevice(const QString& id, const QString& name, Account* a, bool isSelf);

    RingDevicePrivate* d_ptr;
    Q_DECLARE_PRIVATE(RingDevice)
};

Q_DECLARE_METATYPE(RingDevice*)
