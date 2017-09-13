/****************************************************************************
 *   Copyright (C) 2016 by Savoir-faire Linux                               *
 *   Author : Alexandre Viau <alexandre.viau@savoirfairelinux.com>          *
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

#include <QtCore/QAbstractTableModel>

#include <typedefs.h>

#include <ringdevice.h>

class Account;

class RingDeviceModelPrivate;

class LIB_EXPORT RingDeviceModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   friend class Account;
   friend class AccountModelPrivate;

   enum class Roles {
       ID = Qt::UserRole+1,
       NAME,
       OBJECT,
       IS_SELF,
   };

   RingDevice* ownDevice() const;

   //Abstract model accessors
   virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole        ) const override;
   virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role   )       override;
   virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role          ) const override;
   virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                   ) const override;
   virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                   ) const override;
   virtual Qt::ItemFlags flags       (const QModelIndex &index                                     ) const override;
   virtual QHash<int,QByteArray> roleNames(                                                        ) const override;
   virtual int           size        (                               ) const;

Q_SIGNALS:
   void deviceRevoked(RingDevice* device, RingDevice::RevocationStatus status);

private:
   explicit RingDeviceModel(Account* a, const QString& devId, const QString& devName);
   virtual ~RingDeviceModel();

   RingDeviceModelPrivate* d_ptr;
   Q_DECLARE_PRIVATE(RingDeviceModel)

};

Q_DECLARE_METATYPE(RingDeviceModel*)
