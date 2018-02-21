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
#include "addressmodel.h"

// Ring
#include <contactmethod.h>
#include <numbercategory.h>
#include <categorizedhistorymodel.h>
#include <phonedirectorymodel.h>
#include <person.h>
#include <address.h>

AddressModel::AddressModel(const Person* parent) :
    QAbstractListModel(const_cast<Person*>(parent)), m_pPerson(const_cast<Person*>(parent))
{
    // Row inserted/deleted can be implemented later
    connect(parent, &Person::addressesAboutToChange, this, [this](){beginResetModel();});
    connect(parent, &Person::addressesChanged      , this, [this](){endResetModel();});
}

AddressModel::~AddressModel()
{}

QVariant AddressModel::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    const auto a = m_pPerson->addresses()[index.row()];

    switch(role) {
        case Qt::DisplayRole:
        case (int)Address::Role::ADDRESSLINE:
            return a->addressLine();
        case (int)Address::Role::CITY:
            return a->city();
        case (int)Address::Role::ZIPCODE:
            return a->zipCode();
        case (int)Address::Role::STATE:
            return a->state();
        case (int)Address::Role::COUNTRY:
            return a->country();
        case (int)Address::Role::TYPE:
            return a->type();
    }

    return {};
}

int AddressModel::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : m_pPerson->addresses().size();
}

bool AddressModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(row)
    Q_UNUSED(count)
    Q_UNUSED(parent)

    //TODO

    return false;
}

QHash<int,QByteArray> AddressModel::roleNames() const
{
    static QHash<int, QByteArray> roles = PhoneDirectoryModel::instance().roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;

        roles[ (int)Address::Role::ADDRESSLINE] = "addressLine";
        roles[ (int)Address::Role::CITY] = "city";
        roles[ (int)Address::Role::ZIPCODE] = "zipCode";
        roles[ (int)Address::Role::STATE] = "state";
        roles[ (int)Address::Role::COUNTRY] = "country";
        roles[ (int)Address::Role::TYPE] = "type";
    }

    return roles;
}
