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
#include "plugin.h"

// Qt
#include <QtCore/QObject>
#include <QQmlEngine>

// LibRingQt
#include "accountfields.h"

void RingQtQuick::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("net.lvindustries.ringqtquick"));

    qmlRegisterType<AccountFields>(uri, 1, 0, "AccountFields");

    // Alias
    qmlRegisterUncreatableType<AttachedAccountFieldStatus>(
        uri, 1, 0, "FieldStatus",
        "Cannot create objects of type FieldStatus, use it as an attached poperty"
    );
}

void RingQtQuick::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(engine)
    Q_UNUSED(uri)
}
