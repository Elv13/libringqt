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
#ifndef TIMELINEFILTER_H
#define TIMELINEFILTER_H

#include <QtCore/QSortFilterProxyModel>

class Individual;
class TimelineFilterPrivate;

class Q_DECL_EXPORT TimelineFilter : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    Q_PROPERTY(Individual* individual READ individual WRITE setIndividual NOTIFY changed)
    Q_PROPERTY(bool showMessages READ areMessagesShown WRITE setShowMessages NOTIFY changed)
    Q_PROPERTY(bool showCalls READ areCallsShown WRITE setShowCalls NOTIFY changed)
    Q_PROPERTY(bool showEmptyGroups READ areEmptyGroupsShown WRITE setShowEmptyGroups NOTIFY changed)

    /**
     * Set to something larger than the animation speed to avoid very expensive
     * reflows during the animation(s).
     *
     * Plus, it has to be delayed anyway because all the boolean properties
     * might be set after the source model because of QML undefined property
     * assignment order.
     */
    Q_PROPERTY(int initDelay READ initDelay WRITE setInitDelay NOTIFY changed)

    Q_INVOKABLE explicit TimelineFilter(QObject* parent = nullptr);
    virtual ~TimelineFilter();

    Individual* individual() const;
    void setIndividual(Individual* i);

    // Make them invokable
    Q_INVOKABLE virtual QModelIndex mapFromSource(const QModelIndex& idx) const override;
    Q_INVOKABLE virtual QModelIndex mapToSource(const QModelIndex& idx) const override;

    bool areMessagesShown() const;
    void setShowMessages(bool v);

    bool areCallsShown() const;
    void setShowCalls(bool v);

    bool areEmptyGroupsShown() const;
    void setShowEmptyGroups(bool v);

    int initDelay() const;
    void setInitDelay(int v);

Q_SIGNALS:
    void changed();

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
    TimelineFilterPrivate* d_ptr;
    Q_DECLARE_PRIVATE(TimelineFilter)
};

#endif
