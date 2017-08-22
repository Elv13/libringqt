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
#include "personcmmodel.h"

// Ring
#include <contactmethod.h>
#include <numbercategory.h>
#include <categorizedhistorymodel.h>
#include <phonedirectorymodel.h>
#include <person.h>

PersonCMModel::PersonCMModel(const Person* parent) :
    QAbstractListModel(const_cast<Person*>(parent)), m_pPerson(const_cast<Person*>(parent))
{
    // Row inserted/deleted can be implemented later
    m_cBeginCB = connect(parent, &Person::phoneNumbersAboutToChange, this, [this](){beginResetModel();});
    m_cEndCB   = connect(parent, &Person::phoneNumbersChanged      , this, [this](){endResetModel  ();});
}

PersonCMModel::~PersonCMModel()
{
    disconnect( m_cEndCB   );
    disconnect( m_cBeginCB );
}

QVariant PersonCMModel::data( const QModelIndex& index, int role ) const
{
    // As this model is always associated with the person, the relevant icon
    // is the phone number type (category)
    if (index.isValid() && role == Qt::DecorationRole) {
        return m_pPerson->phoneNumbers()[index.row()]->category()->icon();
    }

    return index.isValid() ? m_pPerson->phoneNumbers()[index.row()]->roleData(role) : QVariant();
}

int PersonCMModel::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : m_pPerson->phoneNumbers().size();
}

bool PersonCMModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row >= rowCount())
        return false;

    auto pn = m_pPerson->phoneNumbers();
    for (int i = row; i < row+count; i++)
        pn.remove(row);
    m_pPerson->setContactMethods(pn);

    return true;
}

QHash<int,QByteArray> PersonCMModel::roleNames() const
{
    static QHash<int, QByteArray> roles = PhoneDirectoryModel::instance().roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;
    }

    return roles;
}
