/****************************************************************************
 *   Copyright (C) 2019 by Bluesystems                                      *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#pragma once

#include <QtCore/QAbstractItemModel>

//Ring
#include "picocms/collectionmanagerinterface.h"
#include "typedefs.h"
#include "contactmethod.h"

class NotificationStateModelPrivate;

class NotificationStateModel : public QAbstractListModel, public CollectionManagerInterface<ContactMethod>
{
    Q_OBJECT

public:
    //Constructor
    virtual ~NotificationStateModel();
    explicit NotificationStateModel(QObject* parent);

    //Model implementation
    virtual QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole                 ) const override;
    virtual int           rowCount ( const QModelIndex& parent = {}                                       ) const override;
    virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent = {}                  ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    //Management
    void setState(ContactMethod* number, ContactMethod::NotificationPolicy p);

    Q_INVOKABLE void clear();

    static NotificationStateModel* instance();

private:
    NotificationStateModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(NotificationStateModel)

    //Backend interface
    virtual void collectionAddedCallback(CollectionInterface* backend) override;
    virtual bool addItemCallback(const ContactMethod* item) override;
    virtual bool removeItemCallback(const ContactMethod* item) override;
};

