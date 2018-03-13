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
#include "eventmodel.h"

//Ring
#include "account.h"

class EventModelPrivate final : public QObject
{
    Q_OBJECT
public:
    //Attributes
    QVector<Event*> m_lEvent;
//     QHash<QByteArray, Event*> m_hEvents;

    EventModel* q_ptr;
};

EventModel::EventModel(QObject* parent)
  : QAbstractListModel(parent)
  , CollectionManagerInterface<Event>(this)
  , d_ptr(new EventModelPrivate())
{
    d_ptr->q_ptr = this;
}

EventModel& EventModel::instance()
{
    static auto instance = new EventModel(QCoreApplication::instance());
    return *instance;
}

EventModel::~EventModel()
{
//     while (d_ptr->m_lEvent.size()) {
//         Event* e = d_ptr->m_lEvent[0];
//         d_ptr->m_lEvent.removeAt(0);
//         delete e;
//     }
    delete d_ptr;
}

QHash<int,QByteArray> EventModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static std::atomic_flag initRoles {ATOMIC_FLAG_INIT};

    if (!initRoles.test_and_set()) {

    }

    return roles;
}

QVariant EventModel::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    const Event* info = d_ptr->m_lEvent[index.row()];

    switch (role) {
//         case Qt::DisplayRole:
//             return info->name();
    };

    return {};
}

int EventModel::rowCount( const QModelIndex& parent ) const
{
    if (!parent.isValid())
        return d_ptr->m_lEvent.size();
    return 0;
}

QItemSelectionModel* EventModel::defaultSelectionModel() const
{
    return nullptr;
}

bool EventModel::addItemCallback(const Event* item)
{
    Q_UNUSED(item)
    beginInsertRows(QModelIndex(),d_ptr->m_lEvent.size(),d_ptr->m_lEvent.size());
    d_ptr->m_lEvent << const_cast<Event*>(item);
//     d_ptr->m_hEvents[item->uid()] = const_cast<Event*>(item);
    endInsertRows();

    return true;
}

bool EventModel::removeItemCallback(const Event* item)
{
    Q_UNUSED(item)
    return true;
}

#include <eventmodel.moc>
