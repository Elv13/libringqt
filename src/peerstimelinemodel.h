/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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

#include <QtCore/QAbstractTableModel>
#include <QtCore/QItemSelectionModel>

#include "typedefs.h"

#include <memory>

class ContactMethod;
class Individual;

class PeersTimelineModelPrivate;

/**
 * This model sort the PhoneDirectoryModel contact methods into a timeline.
 *
 * It is a successor to the RecentModel as it became too complex to be extended
 * any further. Compared to its predesessor, it doesn't attempt to track
 * multiple object type nor does it exposes a tree. It is a thin "proxy" on top
 * of the PhoneDirectoryModel and use modern C++ algorithm to rotate the CMs
 * instead of a thousand line of imperative code. It delegate some of the
 * responsabilities previously handled by the RecentModel to the ContactMethod.
 * This simplify everything as the contact method object have direct access to
 * the events instead of trying to recreate them on an upper layer.
 *
 * This model could have been merged into PhoneDirectoryModel, but that would
 * add complexity to mature code. Rewriting RecentModel would have carried risk
 * for shipping products. Once/If this model meet the requirement to replace
 * RecentModel, the old code could be retired.
 *
 * This models may keep multiple entries per "Person" object. If this is
 * undesirable, a QSortFilterProxyModel can be added on top of this model
 * with the acceptRow condition being:
 *
 *    ((!contact()) || contact()->mostRecentContactMethod())
 *
 */
class LIB_EXPORT PeersTimelineModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    Q_PROPERTY(QSharedPointer<QAbstractItemModel> timelineSummaryModel READ timelineSummaryModel CONSTANT)
    Q_PROPERTY(Individual*  mostRecentIndividual READ mostRecentIndividual NOTIFY headChanged)
    Q_PROPERTY(bool empty READ isEmpty  NOTIFY headChanged)

    /// Roles used in the `timelineSumaryModel`
    enum class SummaryRoles {
        CATEGORY_ENTRIES = Qt::UserRole + 1,
        ACTIVE_CATEGORIES,
        TOTAL_ENTRIES,
        RECENT_DATE,
        DISTANT_DATE,
    };
    Q_ENUMS(SumaryRoles)

    // Singleton
    static PeersTimelineModel& instance();
    virtual ~PeersTimelineModel();

    // Model re-implementation
    virtual QVariant data       ( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    virtual int      rowCount   ( const QModelIndex& parent = {}                       ) const override;
    virtual int      columnCount( const QModelIndex& parent = {}                       ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    QSharedPointer<QAbstractItemModel> timelineSummaryModel() const;

    QSharedPointer<QAbstractItemModel> bookmarkedTimelineModel() const;

    Individual* mostRecentIndividual() const;
    bool isEmpty() const;

    QModelIndex contactMethodIndex(ContactMethod* cm) const;
    QModelIndex individualIndex(Individual* i) const;

    // Mutator

    // Display in the timeline even if it was never contacted
    void whiteList(Individual* ind);

Q_SIGNALS:
    void headChanged();
    void lastUsedIndividualChanged(Individual* cm, time_t t);
    void individualMerged(Individual* oldInd, Individual* mergedInto);
    void individualAdded(Individual* ind);
    void individualChanged(Individual* ind);
    void selfRemoved(Individual* ind);

private:
    explicit PeersTimelineModel();

    PeersTimelineModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(PeersTimelineModel)
};

class PeersTimelineSelectionModelPrivate;

class LIB_EXPORT PeersTimelineSelectionModel : public QItemSelectionModel
{
    Q_OBJECT
public:
    Q_PROPERTY(ContactMethod* contactMethod READ contactMethod WRITE setContactMethod)

    explicit PeersTimelineSelectionModel();
    virtual ~PeersTimelineSelectionModel();

    ContactMethod* contactMethod() const;
    void setContactMethod(ContactMethod* cm);
private:
    PeersTimelineSelectionModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(PeersTimelineSelectionModel)
};
Q_DECLARE_METATYPE(PeersTimelineSelectionModel*);
