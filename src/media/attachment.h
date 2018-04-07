/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
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

#include <typedefs.h>

class QMimeType;

namespace Media {

/**
 * This interface defines the basic methods and properties necessary to record
 * the existence of the object during serialization.
 *
 * This should be implemented by all classes that represent a single file such
 * as Media::File or Media::AVRecording.
 */
class LIB_EXPORT Attachment
{
public:

    /**
     * A finite number of supported attachments for an event.
     *
     * In accordance to rfc5545 section 3.2.8, each attachment has a MIME types
     * (rfc2046), a variable number of parameters and a value (either has an
     * URI, as a payload or as a CID).
     *
     * @warning only add new entries at the end to preserve the file format
     */
    enum class BuiltInTypes {
        OTHER            = 0, /*!< Unknown, handle them as normal files       */
        AUDIO_RECORDING  = 1, /*!< .wav file extracted from calls             */
        TEXT_RECORDING   = 2, /*!< .json of the text messages                 */
        EMBEDDED         = 3, /*!< Anything that's embedded into the timeline */
        TRANSFERRED_FILE = 4, /*!< Files download from the peer               */
    };

    virtual QMimeType* mimeType() const = 0;

    virtual QUrl path() const = 0;

    virtual BuiltInTypes type() const = 0;

    /**
     * The role of this file within the ::Event it is serialized in.
     *
     * This is a custom concept used to create the right kind of asset when
     * parsing the serialized events.
     */
    virtual QByteArray role() const;

};

}

