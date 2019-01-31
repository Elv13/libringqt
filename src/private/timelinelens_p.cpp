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
#include "timelinelens_p.h"

#include <individualtimelinemodel.h>
#include "textrecording_p.h"

TimelineLens::TimelineLens(IndividualTimelineModel* p) :
    QAbstractListModel(p), q_ptr(p)
{}

TimelineLens::~TimelineLens()
{}

void TimelineLens::addEntry(TextMessageNode* n)
{
    beginInsertRows({}, m_lEntries.size(), m_lEntries.size());
    m_lEntries << n;
    endInsertRows();
}

QVariant TimelineLens::data( const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    return m_lEntries[index.row()]->roleData(role);
}

int TimelineLens::rowCount( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_lEntries.size();
}

QHash<int, QByteArray> TimelineLens::roleNames() const
{
    return q_ptr->roleNames();
}
