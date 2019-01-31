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
#include "lensmanager_p.h"

#include "timelinelens_p.h"

int LensManager::maximumSize = 0;
FlagPack<IndividualTimelineModel::LensType> LensManager::defaultLenses =
    IndividualTimelineModel::LensType::NONE;

LensManager::LensManager(IndividualTimelineModel* parent) : q_ptr(parent)
{

}

QSharedPointer<QAbstractItemModel> LensManager::getLens(IndividualTimelineModel::LensType t)
{
    QSharedPointer<QAbstractItemModel> m = m_hModels.value((int)t);

    if (!m) {
        m = QSharedPointer<QAbstractItemModel>(new TimelineLens(q_ptr));
        m_hModels[(int)t] = m;
    }

    return m;
}
