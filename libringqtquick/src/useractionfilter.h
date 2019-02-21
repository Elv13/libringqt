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
#ifndef ACTIONFILTER_H
#define ACTIONFILTER_H

#include <QtCore/QSortFilterProxyModel>
#include <QQmlListProperty>

#include <useractionmodel.h>

class UserActionPrivate;
class UserActionFilterPrivate;

class Q_DECL_EXPORT UserAction : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(UserActionModel::Action action READ action WRITE setAction NOTIFY changed)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY changed)

    Q_INVOKABLE explicit UserAction(QObject* parent = nullptr);
    virtual ~UserAction();

    bool isEnabled() const;
    void setEnabled(bool);

    UserActionModel::Action action() const;
    void setAction(UserActionModel::Action a);

Q_SIGNALS:
    void changed();

private:
    UserActionPrivate* d_ptr;
    Q_DECLARE_PRIVATE(UserAction)
};

class Q_DECL_EXPORT UserActionFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "actions")
public:
    Q_PROPERTY(QQmlListProperty<UserAction> actions READ actions)
    Q_PROPERTY(QAbstractItemModel* model READ model WRITE setModel)

    Q_INVOKABLE explicit UserActionFilter(QObject* parent = nullptr);
    virtual ~UserActionFilter();

    QQmlListProperty<UserAction> actions();

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* m);

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
private:
    UserActionFilterPrivate* d_ptr;
    Q_DECLARE_PRIVATE(UserActionFilter)
};

#endif
