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
#include "presencestatus.h"

//Qt
#include <QtCore/QUrl>
#include <QtCore/QDir>

class PresenceStatusPrivate
{
public:
    PresenceStatusPrivate();
    QString m_Name;
    QString m_Message;
    QVariant m_Color;
    bool m_IsDefaultStatus {false};
};

PresenceStatusPrivate::PresenceStatusPrivate()
{

}

PresenceStatus::PresenceStatus(QObject* parent) : ItemBase(parent), d_ptr(new PresenceStatusPrivate())
{

}

PresenceStatus::~PresenceStatus()
{
   delete d_ptr;
}

QString PresenceStatus::name() const
{
    return d_ptr->m_Name;
}

QString PresenceStatus::message() const
{
    return d_ptr->m_Message;
}

QVariant PresenceStatus::color() const
{
    return d_ptr->m_Color;
}

bool PresenceStatus::isDefaultStatus() const
{
    return d_ptr->m_IsDefaultStatus;
}

void PresenceStatus::setName(const QString& v)
{
    d_ptr->m_Name = v;
}

void PresenceStatus::setMessage(const QString& v)
{
    d_ptr->m_Message = v;
}

void PresenceStatus::setColor(const QVariant& v)
{
    d_ptr->m_Color = v;
}

void PresenceStatus::setDefaultStatus(bool v)
{
    d_ptr->m_IsDefaultStatus = v;
}

