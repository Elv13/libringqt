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
#ifndef RECENT_FILE_MODEL_H
#define RECENT_FILE_MODEL_H

#include <QtCore/QStringListModel>

class RecentFileModelPrivate;

/**
 * Small model to store the list of recently used files.
 *
 * It doesn't store anything and doesn't integrate with the platform recent file
 * capabilities. It's usecase is limited to prevent the user to look for the
 * same file(s) many time in a row if she/he wishes to stream them again or
 * stream/transfer them to many people.
 */
class RecentFileModel : public QStringListModel
{
    Q_OBJECT
public:
    Q_INVOKABLE explicit RecentFileModel();
    virtual ~RecentFileModel();

    Q_INVOKABLE QUrl addFile();
private:

    RecentFileModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(RecentFileModel)
};

#endif
