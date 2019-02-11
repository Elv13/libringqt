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
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraCharacteristics;
import java.util.HashMap;
import java.util.Map;
import java.util.List;

/*
 * WARNING: The content of this file is GPLv3 and imported from the official
 * Jami for Android client.
 */

public class HardwareProxy
{
    private static class VideoParams {
        public String id;
        public int format;
        // size as captured by Android
        public int width;
        public int height;
        //size, rotated, as seen by the daemon
        public int rotWidth;
        public int rotHeight;
        public int rate;
        public int rotation;

        public VideoParams(String id, int format, int width, int height, int rate) {
            this.id = id;
            this.format = format;
            this.width = width;
            this.height = height;
            this.rate = rate;
        }
    }

    private static class DeviceParams {
        Point size;
        long rate;
        Camera.CameraInfo infos;

        StringMap toMap(int orientation) {
            StringMap map = new StringMap();
            boolean rotated = (size.x > size.y) == (orientation == Configuration.ORIENTATION_PORTRAIT);
            map.set("size", Integer.toString(rotated ? size.y : size.x) + "x" + Integer.toString(rotated ? size.x : size.y));
            map.set("rate", Long.toString(rate));
            return map;
        }
    }

    protected static Context mContext;

    // Audio
    private static int mSampleRate;
    private static int mBufferSize;

    // Camera
    private static CameraManager mCameraManager;
    private static HashMap<String, VideoParams> mParams = new HashMap<>();
    private static Map<String, DeviceParams> mNativeParams = new HashMap<>();
    private static List<String> mCameraList;
    private static String mDefaultDeviceName;
    private static String mCurrentCameraName;
    pruvate static String mFrontCameraName;

    public static int setContext(android.content.Context ctx) {
        mContext = ctx;
        int ret = 0;

        // Audio
        ret = initAudio();

        // Video
        initCamera();

        return ret;
    }

    public static void initAudio() {
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
    }

    public static void initCamera() {
        mCameraManager = (CameraManager) c.getSystemService(Context.CAMERA_SERVICE);
        mNativeParams.clear();
        if (manager == null)
            return;

        try {

            for (String id : manager.getCameraIdList()) {
                mCurrentCameraName = id;
                CameraCharacteristics cc = manager.getCameraCharacteristics(id);
                int facing = cc.get(CameraCharacteristics.LENS_FACING);

                if (facing == CameraCharacteristics.LENS_FACING_FRONT) {
                    mFrontCameraName = id;
                } else if (facing == CameraCharacteristics.LENS_FACING_BACK) {
                    cameraBack = id;
                } else if (facing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                    cameraExternal = id;
                }

                mCameraList.add(id);
            }

            if (!TextUtils.isEmpty(mFrontCameraName))
                mCurrentCameraName = mFrontCameraName;

            if (mCurrentCameraName != null)
                mDefaultDeviceName = mCurrentCameraName;

        } catch (Exception e) {
            System.out.println("initCamera: can't enumerate devices", e);
        }
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

    public static int cameraCount() {
        return mCameraList.size();
    }

    public static String getCameraName(int id) {
        return mCameraList.get(id);
    }
}
