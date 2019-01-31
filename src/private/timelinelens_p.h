/************************************************************************************
 *   Copyright (C) 2019 by BlueSystems GmbH                                         *
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

// Qt
#include <QtCore/QAbstractListModel>

class TextMessageNode;
class IndividualTimelineModel;

/**
 * Provide some timeline filters for easier lookup as done by most chat apps.
 */
class TimelineLens : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit TimelineLens(IndividualTimelineModel* parent = nullptr);
    virtual ~TimelineLens();

    virtual QVariant data    ( const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual int      rowCount( const QModelIndex& parent = {}                      ) const override;
    virtual QHash<int,QByteArray> roleNames() const override;

    void addEntry(TextMessageNode* n);

protected:
    QVector<TextMessageNode*> m_lEntries;
    IndividualTimelineModel* q_ptr;
};
