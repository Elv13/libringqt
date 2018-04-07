/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
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
#include "file.h"

//Dring
#include <media_const.h>
#include "dbus/callmanager.h"

//Ring
#include <call.h>

class FilePrivate
{
public:
    QUrl m_Path;
    Media::Attachment::BuiltInTypes m_Type {Media::Attachment::BuiltInTypes::OTHER};
    QMimeType* m_pMimeType {nullptr};
};

Media::File::File(const QUrl& path, BuiltInTypes t, QMimeType* mt) : d_ptr(new FilePrivate)
{
    d_ptr->m_Path      = path;
    d_ptr->m_pMimeType = mt;
    d_ptr->m_Type      = t;
}

Media::Media::Type Media::File::type()
{
    return Media::Media::Type::FILE;
}

Media::File::~File()
{
    delete d_ptr;
}

QMimeType* Media::File::mimeType() const
{
    return d_ptr->m_pMimeType;
}

QUrl Media::File::path() const
{
    return d_ptr->m_Path;
}

Media::Attachment::BuiltInTypes Media::File::type() const
{
    return d_ptr->m_Type;
}
