/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
 *   Copyright (C) 2018 by Bluesystems                                      *
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

class BookmarkModelPrivate;

class LIB_EXPORT BookmarkModel : public QAbstractItemModel, public CollectionManagerInterface<ContactMethod>
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    Q_OBJECT
    #pragma GCC diagnostic pop

    friend class Session; // Factory
public:
    Q_PROPERTY(bool displayMostPopular READ displayMostPopular WRITE setDisplayPopular)

    //Constructor
    virtual ~BookmarkModel();

    bool displayMostPopular() const;
    void setDisplayPopular(bool value);

    //Model implementation
    virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole                 ) const override;
    virtual int           rowCount    ( const QModelIndex& parent = {}                                       ) const override;
    virtual Qt::ItemFlags flags       ( const QModelIndex& index                                             ) const override;
    virtual int           columnCount ( const QModelIndex& parent = {}                                       ) const override;
    virtual QModelIndex   parent      ( const QModelIndex& index                                             ) const override;
    virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent = {}                  ) const override;
    virtual QStringList   mimeTypes   (                                                                      ) const override;
    virtual QMimeData*    mimeData    ( const QModelIndexList &indexes                                       ) const override;
    virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    //Management
    void remove        (const QModelIndex& idx);
    void addBookmark   (ContactMethod* number );
    void removeBookmark(ContactMethod* number );

    //Getters
    int acceptedPayloadTypes();

private:
    explicit BookmarkModel();
    BookmarkModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(BookmarkModel)

    //Backend interface
    virtual void collectionAddedCallback(CollectionInterface* backend) override;
    virtual bool addItemCallback(const ContactMethod* item) override;
    virtual bool removeItemCallback(const ContactMethod* item) override;

public Q_SLOTS:
    void clear();
};

