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

    setEditRow(false);
}

QVariant PersonCMModel::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    if (m_pTmpCM && index.row() >= m_pPerson->phoneNumbers().size()) {
        Q_ASSERT(index.row() == m_pPerson->phoneNumbers().size());

        switch(role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
                return m_pTmpCM->uri();
            case (int) ContactMethod::Role::CategoryName:
                return m_pTmpCM->category() ?
                    m_pTmpCM->category()->name() : QString();
            case static_cast<int>(Call::Role::ContactMethod):
            case static_cast<int>(Ring::Role::Object):
                return QVariant::fromValue(m_pTmpCM);
        }

        return {};
    }

    // As this model is always associated with the person, the relevant icon
    // is the phone number type (category)
    if (index.isValid() && role == Qt::DecorationRole) {
        return m_pPerson->phoneNumbers()[index.row()]->category()->icon();
    }

    return index.isValid() ? m_pPerson->phoneNumbers()[index.row()]->roleData(role) : QVariant();
}

bool PersonCMModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
    if (index.row() == rowCount()) {
        beginInsertRows({}, rowCount(), rowCount());
        setEditRow(true);
        endInsertRows();
    }
    else if (!index.isValid())
        return false;

    switch(role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            m_pTmpCM->setUri(value.toString());
            emit dataChanged(index, index);
            return true;
    }

    return false;
}

int PersonCMModel::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : m_pPerson->phoneNumbers().size() + (
        m_pTmpCM ? 1 : 0
    );
}

// Re-implement to allow adding new rows
QModelIndex PersonCMModel::index(int row, int col, const QModelIndex& parent) const
{
    if (col || parent.isValid())
        return {};

    if (row > m_pPerson->phoneNumbers().size()+(m_pTmpCM ? -1 : 0))
        return {};

    return createIndex(row, 0, row);
}

bool PersonCMModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row >= rowCount())
        return false;

    if (row == rowCount()-1) {
        setEditRow(false);
        emit layoutChanged();
        return true;
    }

    auto pn = m_pPerson->phoneNumbers();
    for (int i = row; i < row+count; i++)
        pn.remove(row);
    m_pPerson->setContactMethods(pn);

    emit layoutChanged();

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

bool PersonCMModel::hasEditRow() const
{
    return m_pTmpCM;
}

void PersonCMModel::setEditRow(bool v)
{
    if (v && !m_pTmpCM) {
        m_pTmpCM = new TemporaryContactMethod();
    }
    else if ((!v) && m_pTmpCM)
        delete m_pTmpCM;

    emit hasEditRowChanged(v);
    emit layoutChanged();
}
