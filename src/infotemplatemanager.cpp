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
#include "infotemplatemanager.h"

//Qt
#include <QtCore/QTimer>
#include <QtCore/QFileInfo>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>

//Ring
#include "dbus/configurationmanager.h"
#include "dbus/callmanager.h"
#include "account.h"
#include "infotemplate.h"
#include "accountmodel.h"
#include "ringtone.h"
#include <collections/localinfotemplatecollection.h>

class InfoTemplateManagerPrivate final : public QObject
{
    Q_OBJECT
public:
    //Attributes
    QVector<InfoTemplate*> m_lInfoTemplate;
    LocalInfoTemplateCollection* m_pCollection {nullptr};
    QHash<QByteArray, InfoTemplate*> m_hInfoTemplates;

    InfoTemplateManager* q_ptr;
};

InfoTemplateManager::InfoTemplateManager(QObject* parent)
  : QAbstractListModel(parent)
  , CollectionManagerInterface<InfoTemplate>(this)
  , d_ptr(new InfoTemplateManagerPrivate())
{
    d_ptr->q_ptr = this;
    d_ptr->m_pCollection = addCollection<LocalInfoTemplateCollection>();
}

InfoTemplateManager::~InfoTemplateManager()
{
    while (d_ptr->m_lInfoTemplate.size()) {
        InfoTemplate* ringtone = d_ptr->m_lInfoTemplate[0];
        d_ptr->m_lInfoTemplate.removeAt(0);
        delete ringtone;
    }
    delete d_ptr;
}

QHash<int,QByteArray> InfoTemplateManager::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static std::atomic_flag initRoles = ATOMIC_FLAG_INIT;

    if (!initRoles.test_and_set()) {

    }

    return roles;
}

QVariant InfoTemplateManager::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    //TODO
    //const InfoTemplate* info = d_ptr->m_lInfoTemplate[index.row()];

    switch (role) {
//         case Qt::DisplayRole:
//             return info->name();
    };

    return {};
}

int InfoTemplateManager::rowCount( const QModelIndex& parent ) const
{
    if (!parent.isValid())
        return d_ptr->m_lInfoTemplate.size();
    return 0;
}

QItemSelectionModel* InfoTemplateManager::defaultSelectionModel() const
{
    return nullptr;
}

bool InfoTemplateManager::addItemCallback(const InfoTemplate* item)
{
    Q_UNUSED(item)
    beginInsertRows(QModelIndex(),d_ptr->m_lInfoTemplate.size(),d_ptr->m_lInfoTemplate.size());
    d_ptr->m_lInfoTemplate << const_cast<InfoTemplate*>(item);
    d_ptr->m_hInfoTemplates[item->uid()] = const_cast<InfoTemplate*>(item);
    endInsertRows();

    return true;
}

bool InfoTemplateManager::removeItemCallback(const InfoTemplate* item)
{
    Q_UNUSED(item)
    return true;
}

InfoTemplate* InfoTemplateManager::getByUid(const QByteArray& a) const
{
    return d_ptr->m_hInfoTemplates.value(a);
}

InfoTemplate* InfoTemplateManager::defaultInfoTemplate() const
{
    return d_ptr->m_lInfoTemplate.first();
}

#include <infotemplatemanager.moc>
