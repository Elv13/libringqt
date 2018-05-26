/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
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

#include <QtCore/QAbstractTableModel>
#include "typedefs.h"

//Qt
class QItemSelectionModel;

//Ring
#include <phonedirectorymodel.h>
#include <itemdataroles.h>
#include <contactmethod.h>
class Call;

//Private
class NumberCompletionModelPrivate;

class LIB_EXPORT NumberCompletionModel : public QAbstractTableModel {
   Q_OBJECT

public:

   //Properties
   Q_PROPERTY(QString prefix READ prefix)
   Q_PROPERTY(bool displayMostUsedNumbers READ displayMostUsedNumbers WRITE setDisplayMostUsedNumbers)
   Q_PROPERTY(QItemSelectionModel* selectionModel READ selectionModel CONSTANT)
   Q_PROPERTY(ContactMethod* selectedContactMethod READ selectedContactMethod NOTIFY selectionChanged)

   enum class LookupStatus {
       NOT_APPLICABLE,
       IN_PROGRESS,
       SUCCESS,
       FAILURE,
       COUNT__
   };
   Q_ENUMS(LookupStatus)

   /**
    * Define where the completion entry is coming from.
    *
    * The order represent the priority in case it's available from multiple
    * sources.
    */
   enum class EntrySource {
       FROM_BOOKMARKS,
       FROM_HISTORY,
       FROM_CONTACTS,
       FROM_WEB,
   };
   Q_ENUMS(EntrySource)

   enum Role {
      ALTERNATE_ACCOUNT = (int)ContactMethod::Role::UserData,
      FORCE_ACCOUNT      ,
      ACCOUNT            ,
      PEER_NAME          ,
      ACCOUNT_ALIAS      ,
      IS_TEMP            ,
      NAME_STATUS        ,
      NAME_STATUS_SRING  ,
      SUPPORTS_REGISTRY  ,
      ENTRY_SOURCE       ,
      IS_SELECTABLE      ,
   };

   explicit NumberCompletionModel();
   virtual ~NumberCompletionModel();

   //Abstract model member
   virtual QVariant      data       ( const QModelIndex& index, int role = Qt::DisplayRole                 ) const override;
   virtual int           rowCount   ( const QModelIndex& parent = QModelIndex()                            ) const override;
   virtual Qt::ItemFlags flags      ( const QModelIndex& index                                             ) const override;
   virtual bool          setData    ( const QModelIndex& index, const QVariant &value, int role            )       override;
   virtual int           columnCount( const QModelIndex& parent = QModelIndex()                            ) const override;
   virtual QVariant      headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;

   virtual QHash<int,QByteArray> roleNames() const override;

   //Setters
   void setUseUnregisteredAccounts(bool value);
   void setDisplayMostUsedNumbers(bool value);

   //Getters
   ContactMethod* number(const QModelIndex& idx) const;
   bool isUsingUnregisteredAccounts();
   QString prefix() const;
   bool displayMostUsedNumbers() const;
   QItemSelectionModel* selectionModel() const;
   ContactMethod* selectedContactMethod() const;

private:
   NumberCompletionModelPrivate* d_ptr;
   Q_DECLARE_PRIVATE(NumberCompletionModel)

public Q_SLOTS:
   bool callSelectedNumber();

Q_SIGNALS:
   void enabled(bool);
   void selectionChanged();

};

Q_DECLARE_METATYPE(NumberCompletionModel*)
Q_DECLARE_METATYPE(NumberCompletionModel::LookupStatus)
Q_DECLARE_METATYPE(NumberCompletionModel::EntrySource)

