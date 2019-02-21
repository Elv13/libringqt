/***************************************************************************
 *   Copyright (C) 2019 by Blue Systems                                    *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "useractionfilter.h"

class UserActionPrivate final
{
public:
    bool m_IsEnabled {true};
    UserActionModel::Action m_Action {UserActionModel::Action::COUNT__};
};

class UserActionFilterPrivate final : public QObject
{
    Q_OBJECT
public:
    QList<UserAction*> m_RealList;
    QQmlListProperty<UserAction> m_List;

    UserActionFilter* q_ptr;

public Q_SLOTS:
    void slotDataChanged();
    void slotFilterChanged();
};

UserAction::UserAction(QObject* parent) : QObject(parent), d_ptr(new UserActionPrivate())
{
    qDebug() << "\n\n\nFOO" << parent;
}

UserAction::~UserAction()
{
    delete d_ptr;
}

bool UserAction::isEnabled() const
{
    return d_ptr->m_IsEnabled;
}

void UserAction::setEnabled(bool v)
{
    d_ptr->m_IsEnabled = v;
    emit changed();
}

UserActionModel::Action UserAction::action() const
{
    return d_ptr->m_Action;
}

void UserAction::setAction(UserActionModel::Action a)
{
    d_ptr->m_Action = a;
    emit changed();
}

UserActionFilter::UserActionFilter(QObject* parent) : QSortFilterProxyModel(parent),
    d_ptr(new UserActionFilterPrivate())
{
    d_ptr->q_ptr  = this;
    d_ptr->m_List = QQmlListProperty<UserAction>(this, d_ptr->m_RealList);
}

UserActionFilter::~UserActionFilter()
{
    delete d_ptr;
}

QQmlListProperty<UserAction> UserActionFilter::actions()
{
    return d_ptr->m_List;
}

QAbstractItemModel* UserActionFilter::model() const
{
    return sourceModel();
}

void UserActionFilter::setModel(QAbstractItemModel* m)
{
    setSourceModel(m);
    connect(m, &QAbstractItemModel::dataChanged,
        d_ptr, &UserActionFilterPrivate::slotDataChanged);
    connect(m, &QAbstractItemModel::rowsInserted,
        d_ptr, &UserActionFilterPrivate::slotDataChanged);
    connect(m, &QAbstractItemModel::rowsRemoved,
        d_ptr, &UserActionFilterPrivate::slotDataChanged);
}

void UserActionFilterPrivate::slotDataChanged()
{
    // Initialize once the data changes. This is late enough to avoid a race

    qDebug() << "\n\nCHANGED!" << q_ptr->sourceModel() <<q_ptr->rowCount();

    static std::atomic_flag initRoles = ATOMIC_FLAG_INIT;
    if (!initRoles.test_and_set()) {
        for (auto ua : qAsConst(m_RealList)) {
            qDebug() << "\n\nACTION!" << ua;
            connect(ua, &UserAction::changed,
                    this, &UserActionFilterPrivate::slotFilterChanged);
        }

    }
    q_ptr->invalidateFilter();
}

void UserActionFilterPrivate::slotFilterChanged()
{
    q_ptr->invalidateFilter();
}


bool UserActionFilter::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    const QModelIndex idx = sourceModel()->index(source_row, 0, source_parent);
    qDebug() << "INVAL" << source_row << idx;
    const int a = idx.data((int)UserActionModel::Role::ACTION).toInt();

    if (!(idx.flags() & Qt::ItemIsEnabled))
        return false;

    //TODO use a matrix
    for (auto ua : qAsConst(d_ptr->m_RealList)) {
        qDebug() << "F" << a << ua->action() <<  ua->isEnabled();
        if ((int)ua->action() == a)
            return ua->isEnabled();
    }

    return true;
}

#include <useractionfilter.moc>
