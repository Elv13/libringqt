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
#pragma once

#include <typedefs.h>
#include <itembase.h>

class InfoTemplatePrivate;

/**
 * This class is the frontend (GUI) view of an ::Individual (and ::Person)
 * object.
 *
 * Before this concept was introduced, the list of visible contact attributes
 * were mostly hardcoded and not flexible enough to accommodate the changes
 * in scope.
 *
 * This class also allows a smoother ::Individual -> ::Person conversion when
 * the time comes.
 */
class LIB_EXPORT InfoTemplate : public ItemBase
{
    Q_OBJECT
public:
    explicit InfoTemplate(const QByteArray& data, QObject* parent = nullptr);
    virtual ~InfoTemplate();

    QByteArray uid() const;

    QByteArray toVCard() const;

    int formRowCount() const;

    QString formLabel(int row) const;

private:
    InfoTemplatePrivate* d_ptr;
    Q_DECLARE_PRIVATE(InfoTemplate)
};
