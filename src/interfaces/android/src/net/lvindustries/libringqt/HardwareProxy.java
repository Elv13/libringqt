/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Thibault Wittemberg <thibault.wittemberg@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

package net.lvindustries.libringqt;

import org.qtproject.qt5.android.bindings.QtActivity;
import org.qtproject.qt5.android.bindings.QtApplication;
import android.util.Log;
import android.content.Context;
import android.os.Bundle;
import android.media.AudioAttributes;
import java.util.Locale;
import java.lang.String;
import android.media.AudioManager;
import android.os.Build;

/*
 * WARNING: The content of this file is GPLv3 and imported from the official
 * Jami for Android client.
 */

public class HardwareProxy
{
    protected static Context mContext;
    private static int mSampleRate;
    private static int mBufferSize;

    public static int setContext(android.content.Context ctx) {
        mContext = ctx;

        AudioManager am = (AudioManager) ctx.getSystemService(Context.AUDIO_SERVICE);

        int sr = 44100;
        int bs = 64;

        try {
            // If possible, query the native sample rate and buffer size.
            sr = Integer.parseInt(am.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE));
            bs = Integer.parseInt(am.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER));
        } catch (NumberFormatException e) {
            System.out.println("Failed to read native OpenSL config: " + e);
            return 1;
        }

        mSampleRate = sr;
        mBufferSize = bs;

        return 0;
    }

    public static String deviceManufacturer() {
        return Build.MANUFACTURER;
    }

    public static String deviceModel() {
        return Build.MODEL;
    }

    public static int sampleRate(int _) {
        return mSampleRate;
    }

    public static int bufferSize(int _) {
        return mBufferSize;
    }
}
