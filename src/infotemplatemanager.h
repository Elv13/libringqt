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

#include "typedefs.h"

//Qt
#include <QtCore/QAbstractTableModel>
class QTimer;
class QItemSelectionModel;

//Ring
#include "picocms/collectionmanagerinterface.h"
class Account;
class InfoTemplateManagerPrivate;
class InfoTemplate;

///CredentialModel: A model for account credentials
class LIB_EXPORT InfoTemplateManager : public QAbstractListModel, public CollectionManagerInterface<InfoTemplate>
{
   Q_OBJECT
   friend class Session; // factory
public:

    virtual ~InfoTemplateManager();

    //Model functions
    virtual QVariant data( const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual int rowCount( const QModelIndex& parent = {}) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    QItemSelectionModel* defaultSelectionModel() const;

    Q_INVOKABLE InfoTemplate* getByUid(const QByteArray& a) const;

    InfoTemplate* defaultInfoTemplate() const;

private:
    explicit InfoTemplateManager(QObject* parent = nullptr);

    InfoTemplateManagerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(InfoTemplateManager)

    //Collection interface
    virtual bool addItemCallback   (const InfoTemplate* item) override;
    virtual bool removeItemCallback(const InfoTemplate* item) override;

};
