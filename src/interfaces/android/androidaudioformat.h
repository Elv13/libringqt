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

#include <interfaces/audioformati.h>
#include <typedefs.h>

namespace Interfaces {

/**
 * This class calls into some Java classes to get the optimial size for the
 * audio buffers. Without this it will either have bad latency or crash
 * with a segmentation fault.
 */
class LIB_EXPORT AndroidAudioFormat : public AudioFormatI
{
public:
    explicit AndroidAudioFormat();

    /**
     * The context object lifecycles seems to be incorrect.
     *
     * Qt reference gets dropped and the object is GCed, if this
     * happens before the methods below are called, it will crash.
     *
     * This function has to be called early in main to add a new
     * Jeva reference to the Android context.
     */
    static void init();

    virtual int bufferSize() const override;
    virtual int sampleRate() const override;

    virtual QString deviceModel() const override;
    virtual QString deviceManufacturer() const override;
};

}
