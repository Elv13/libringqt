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
#pragma once

#include <media/media.h>
#include <media/attachment.h>
#include <typedefs.h>

class FilePrivate;

namespace Media {

/**
 * A file transfer event.
 */
class LIB_EXPORT File : public ::Media::Media, public ::Media::Attachment
{
   Q_OBJECT
public:
    explicit File(const QUrl& path, BuiltInTypes t, QMimeType* mt);
    virtual ~File();

    virtual Media::Type type() override;

    // The attachment properties
    virtual QMimeType* mimeType() const override;
    virtual QUrl path          () const override;
    virtual BuiltInTypes type  () const override;

private:

    FilePrivate* d_ptr;
    Q_DECLARE_PRIVATE(File)
};

}

