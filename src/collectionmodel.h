/****************************************************************************
 *   Copyright (C) 2014-2016 by Savoir-faire Linux                          *
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

#include "typedefs.h"
#include "person.h"

#include <QtCore/QAbstractItemModel>

#include "collectionmanagerinterface.h"
#include "persondirectory.h"
#include "collectioninterface.h"

//Ring
class CollectionExtensionInterface;

class CollectionModelPrivate;
class CollectionManagerInterfaceBase;

class LIB_EXPORT CollectionModel : public QAbstractTableModel
{
   Q_OBJECT

   friend class CollectionManagerInterfaceBase;

   /**
    * Imported from CollectionInterface::SupportedFeatures
    */
   enum class Role {
      count = Qt::UserRole,
      supportNone         ,
      supportLoad         ,
      supportSave         ,
      supportEdit         ,
      supportProbe        ,
      supportAdd          ,
      supportSave_all     ,
      supportClear        ,
      supportRemove       ,
      supportExport       ,
      supportImport       ,
      isEnableable        ,
      isDisableable       ,
      isManageable        ,
      hasManageableChildren,
   };

public:
   explicit CollectionModel(QObject* parent = nullptr);
   virtual ~CollectionModel();

   virtual QVariant data        (const QModelIndex& index, int role = Qt::DisplayRole         ) const override;
   virtual int rowCount         (const QModelIndex& parent = QModelIndex()                    ) const override;
   virtual int columnCount      (const QModelIndex& parent = QModelIndex()                    ) const override;
   virtual Qt::ItemFlags flags  (const QModelIndex& index                                     ) const override;
   virtual QVariant headerData  (int section, Qt::Orientation orientation, int role           ) const override;
   virtual bool setData         (const QModelIndex& index, const QVariant &value, int role    )       override;
   virtual QModelIndex   parent ( const QModelIndex& index                                    ) const override;
   virtual QModelIndex   index  ( int row, int column, const QModelIndex& parent=QModelIndex()) const override;
   virtual QHash<int,QByteArray> roleNames() const override;

   CollectionInterface* collectionAt(const QModelIndex& index);

   QAbstractItemModel* manageableCollections() const;

   void addExtension(CollectionExtensionInterface* extension);

   bool save();
   bool load();

   static CollectionModel& instance();

Q_SIGNALS:
   void checkStateChanged();

private:
   QScopedPointer<CollectionModelPrivate> d_ptr;
   Q_DECLARE_PRIVATE(CollectionModel)

};

