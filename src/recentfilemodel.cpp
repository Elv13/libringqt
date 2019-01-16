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
#include "recentfilemodel.h"

// Qt
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>

// LibRingQt
#include "globalinstances.h"
#include "interfaces/fileprovideri.h"

class RecentFileModelPrivate
{
public:
    QList<QUrl> m_lFiles;

    QStringList toStringList() const;
};

RecentFileModel::RecentFileModel() :
    QStringListModel(QCoreApplication::instance()), d_ptr(new RecentFileModelPrivate)
{
    d_ptr->m_lFiles = GlobalInstances::fileProvider().recentFiles();

    setStringList(d_ptr->toStringList());
}

RecentFileModel::~RecentFileModel()
{
    delete d_ptr;
}

QUrl RecentFileModel::addFile()
{
    const auto fileName = GlobalInstances::fileProvider().getAnyFile({
        "png", "jpg", "gif", "mp4", "mkv", "webm", "txt", "avi", "mpg"
    });

    d_ptr->m_lFiles << fileName;

    setStringList(d_ptr->toStringList());

    GlobalInstances::fileProvider().addRecentFile(fileName);

    return fileName;
}

QStringList RecentFileModelPrivate::toStringList() const
{
    // Uses QUrl for future-proofness of the API
    QStringList ret;

    for (const auto& u : qAsConst(m_lFiles)) {
        ret << u.path();
    }

    return ret;
}
