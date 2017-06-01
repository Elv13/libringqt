/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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

#include <QtCore/QAbstractListModel>

// Ring
#include <contactmethod.h>
class Person;

/**
 * Simple class to access the phone numbers of a Person object as a model.
 *
 * This simplify the code where model binding is an option.
 */
class AddressModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AddressModel(const Person* parent);
    virtual ~AddressModel();

    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    virtual int rowCount( const QModelIndex& parent = {} ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;
    virtual bool removeRows(int row, int count, const QModelIndex &parent) override;

private:
    // This is a private class, no need for d_ptr
    Person* m_pPerson;
};
