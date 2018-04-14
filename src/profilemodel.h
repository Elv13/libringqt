/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
 *   Copyright (C) 2017-2018 by Bluesystems                                 *
 *   Authors : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *             Alexandre Lision <alexandre.lision@savoirfairelinux.com>     *
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

// Qt
#include <QtCore/QAbstractItemModel>
class QItemSelectionModel;
class QStringList;

// Ring
#include "collectionmanagerinterface.h"
class Person;
class ProfileModelPrivate;
class Account;

/**
 * This model manage the "profile" property of `::Account` objects.
 *
 * When the vCards are loaded, it checks the X-RINGACCOUNTID entries and
 * assume this is the profile.
 */
class LIB_EXPORT ProfileModel : public QAbstractItemModel, public CollectionManagerInterface<Person>
{
    Q_OBJECT

public:

    Q_PROPERTY(QAbstractItemModel* availableProfileModel READ availableProfileModel CONSTANT)
    Q_PROPERTY(bool hasAvailableProfiles READ hasAvailableProfiles NOTIFY hasAvailableProfileChanged)

    explicit ProfileModel(QObject* parent = nullptr);
    virtual ~ProfileModel();
    static ProfileModel& instance();

    //Abstract model member
    virtual QVariant      data        ( const QModelIndex& index, int role = Qt::DisplayRole         ) const override;
    virtual int           rowCount    ( const QModelIndex& parent = QModelIndex()                    ) const override;
    virtual int           columnCount ( const QModelIndex& parent = QModelIndex()                    ) const override;
    virtual Qt::ItemFlags flags       ( const QModelIndex& index                                     ) const override;
    virtual bool          setData     ( const QModelIndex& index, const QVariant &value, int role    )       override;
    virtual QModelIndex   index       ( int row, int column, const QModelIndex& parent=QModelIndex() ) const override;
    virtual QModelIndex   parent      ( const QModelIndex& index                                     ) const override;
    virtual QVariant      headerData  ( int section, Qt::Orientation orientation, int role           ) const override;
    virtual QStringList   mimeTypes   (                                                              ) const override;
    virtual QMimeData*    mimeData    ( const QModelIndexList &indexes                               ) const override;
    virtual bool          dropMimeData( const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    virtual QHash<int,QByteArray> roleNames() const override;

    //Getter
    int acceptedPayloadTypes() const;
    QItemSelectionModel* selectionModel() const;
    QItemSelectionModel* sortedProxySelectionModel() const;
    QAbstractItemModel*  sortedProxyModel() const;
    QAbstractItemModel*  availableProfileModel() const;
    bool hasAvailableProfiles() const;

    Person* getProfile(const QModelIndex& idx) const;
    Account* getAccount(const QModelIndex& idx) const;
    QModelIndex accountIndex(Account* a) const;

    QItemSelectionModel* getAccountSelectionModel(Account* a) const;
    Q_INVOKABLE bool setProfile(Account* a, Person* p);

private:
    ProfileModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ProfileModel)

    virtual bool addItemCallback(const Person* item) override;
    virtual bool removeItemCallback(const Person* item) override;

public Q_SLOTS:
    bool remove(const QModelIndex& idx);
    bool add(Person* person = nullptr);
    Person* add(const QString& name);

Q_SIGNALS:
    void hasAvailableProfileChanged();
};
