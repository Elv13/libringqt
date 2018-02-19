/****************************************************************************
 *   Copyright (C) 2018 by Bluesystems                                      *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                    *
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

#include <presencestatus.h>
#include "collectionmanagerinterface.h"

class PresenceStatusPrivate;

class LIB_EXPORT PresenceStatus : public ItemBase
{
    Q_OBJECT
public:

    Q_PROPERTY(QString  name          READ name            WRITE setName         )
    Q_PROPERTY(QString  message       READ message         WRITE setMessage      )
    Q_PROPERTY(QVariant color         READ color           WRITE setColor        )
    Q_PROPERTY(bool     defaultStatus READ isDefaultStatus WRITE setDefaultStatus)

    explicit PresenceStatus(QObject* parent = nullptr);
    virtual ~PresenceStatus();

    QString  name         () const;
    QString  message      () const;
    QVariant color        () const;
    bool     isDefaultStatus() const;

    void setName         (const QString& v);
    void setMessage      (const QString& v);
    void setColor        (const QVariant& v);
    void setDefaultStatus(bool v);

private:
    PresenceStatusPrivate* d_ptr;
    Q_DECLARE_PRIVATE(PresenceStatus)
};
