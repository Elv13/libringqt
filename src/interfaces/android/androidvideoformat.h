/****************************************************************************
 *   Copyright (C) 2019 by Emmanuel Lepage Vallee                           *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>              *
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
#include <interfaces/videoformati.h>

namespace Interfaces {

/**
 * To implement on plarforms where the media frames needs to be processed by
 * the client (Android and iOS)
 */
class LIB_EXPORT AndroidVideoFormat : public VideoFormatI
{
public:
    // To be called AFTER AndroidAudioFormat::init()
    static void init();

    virtual QVector<VideoFormatI::AbstractDevice*> devices() const override;
    virtual void stopCapture() override;
};

}
