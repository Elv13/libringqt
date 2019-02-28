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
import android.hardware.Camera;
import android.media.AudioAttributes;
import java.util.Locale;
import java.lang.String;
import android.media.AudioManager;
import android.os.Build;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.graphics.ImageFormat;
import java.util.HashMap;
import java.util.Map;
import java.util.ArrayList;
import java.util.List;
import android.graphics.Point;
import android.util.Size;

/*
 * WARNING: The content of this file is GPLv3 and imported from the official
 * Jami for Android client.
 */

public class HardwareProxy
{
    //TODO use this instead of the first CameraInfo
    private class VideoParams {
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

        public VideoParams(String _id, int _format, int _width, int _height, int _rate) {
            this.id = id;
            this.format = format;
            this.width = width;
            this.height = height;
            this.rate = rate;
        }
    }

    private static class DeviceParams {
        int width;
        int height;
        long rate;
        Camera.CameraInfo infos;

        /*StringMap toMap(int orientation) {
            StringMap map = new StringMap();
            boolean rotated = (size.x > size.y) == (orientation == Configuration.ORIENTATION_PORTRAIT);
            map.set("size", Integer.toString(rotated ? size.y : size.x) + "x" + Integer.toString(rotated ? size.x : size.y));
            map.set("rate", Long.toString(rate));
            return map;
        }*/
    }
    

    static public class CameraInfo {
        public String name;
        public ArrayList<Integer> formats = new ArrayList<Integer>();
        public ArrayList<Integer> sizes   = new ArrayList<Integer>();
        public ArrayList<Integer> rates   = new ArrayList<Integer>();
    }

    protected static Context mContext;

    // Audio
    private static int mSampleRate;
    private static int mBufferSize;

    // Camera
    private static CameraManager mCameraManager;
    private static HashMap<String, VideoParams> mParams = new HashMap<>();
    private static Map<String, DeviceParams> mNativeParams = new HashMap<>();
    private static List<CameraInfo> mCameraList = new ArrayList<CameraInfo>();
    private static String mDefaultDeviceName;
    private static String mCurrentCameraName;
    private static String mFrontCameraName;
    private static String mBackCameraName;
    private static String mExternalCameraName;

    public static CameraManager getCameraManager() {
        return mCameraManager;
    }

    public static Context getContext() {
        return mContext; 
    }

    public static int setContext(android.content.Context ctx) {
        mContext = ctx;
        int ret = 0;
        mCameraList = new ArrayList<CameraInfo>();
        System.out.println("===============INIT");
        // Audio
        ret = initAudio();

        // Video
        //initCamera();

        return ret;
    }

    public static int initAudio() {
        AudioManager am = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);

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

    public static int initCamera() {
        System.out.println("============INIT CAM");

        mCameraManager = (CameraManager) mContext.getSystemService(Context.CAMERA_SERVICE);
        mNativeParams.clear();
        if (mCameraManager == null)
            return 1;

        try {

            for (String id : mCameraManager.getCameraIdList()) {
                System.out.println("======BEGIN CAM "+id);
                mCurrentCameraName = id;
                CameraCharacteristics cc = mCameraManager.getCameraCharacteristics(id);
                int facing = cc.get(CameraCharacteristics.LENS_FACING);

                if (facing == CameraCharacteristics.LENS_FACING_FRONT) {
                    mFrontCameraName = id;
                } else if (facing == CameraCharacteristics.LENS_FACING_BACK) {
                    mBackCameraName = id;
                } else if (facing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                    mExternalCameraName = id;
                }

                System.out.println("=========CAM "+mCameraList);
                CameraInfo devinf = fillCameraInfo(id);
                mCameraList.add(devinf);

            }

            if (mFrontCameraName.length() == 0)
                mCurrentCameraName = mFrontCameraName;

            if (mCurrentCameraName != null)
                mDefaultDeviceName = mCurrentCameraName;

        } catch (Exception e) {
            System.out.println("initCamera: can't enumerate devices" + e);
            return 2;
        }

        return 0;
    }

    static CameraInfo fillCameraInfo(String camId) {
        if (mCameraManager == null)
            return null;
        try {
            CameraInfo inf = new CameraInfo();
            inf.name = camId;
            final CameraCharacteristics cc = mCameraManager.getCameraCharacteristics(camId);
            StreamConfigurationMap streamConfigs = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

            if (streamConfigs == null)
                return null;

            Size[] rawSizes = streamConfigs.getOutputSizes(ImageFormat.YUV_420_888);
            Size newSize = rawSizes[0];
            /*for (Size s : rawSizes) {
                if (s.getWidth() < s.getHeight()) {
                    continue;
                }
                if ((s.getWidth() == minVideoSize.x && s.getHeight() == minVideoSize.y) ||
                        (newSize.getWidth() < minVideoSize.x
                                ? s.getWidth() > newSize.getWidth()
                                : (s.getWidth() >= minVideoSize.x && s.getWidth() < newSize.getWidth()))) {
                    newSize = s;
                }
            }*/
            //p.size.x = newSize.getWidth();
            //p.size.y = newSize.getHeight();

            long minDuration = streamConfigs.getOutputMinFrameDuration(ImageFormat.YUV_420_888, newSize);
            double maxfps = 1000e9d / minDuration;
            long fps = (long) maxfps;
            inf.rates.add((int)fps);

            inf.sizes.add(newSize.getWidth());
            inf.sizes.add(newSize.getHeight());
            inf.sizes.add(newSize.getHeight());
            inf.sizes.add(newSize.getWidth());

            //p.rate = fps;

            //int facing = cc.get(CameraCharacteristics.LENS_FACING);
            //p.infos.orientation = cc.get(CameraCharacteristics.SENSOR_ORIENTATION);
            //p.infos.facing = facing == CameraCharacteristics.LENS_FACING_FRONT ? Camera.CameraInfo.CAMERA_FACING_FRONT : Camera.CameraInfo.CAMERA_FACING_BACK;

            return inf;
        } catch (Exception e) {
            System.out.println("An error occurred getting camera info: " + e);
        }

        return null;
    }

    public static String deviceManufacturer() {
        return Build.MANUFACTURER;
    }

    public static String deviceModel() {
        return Build.MODEL;
    }

    public static int sampleRate(int i) {
        return mSampleRate;
    }

    public static int bufferSize(int i) {
        return mBufferSize;
    }

    public static int cameraCount() {
        return mCameraList.size();
    }

    public static String getCameraName(int id) {
        System.out.println("=========INFO "+ id);
        return mCameraList.get(id).name;
    }

    public static int getFrameRate(int device, int id) {
          return mCameraList.get(device).rates.get(id);
    }

    public static int getSizeCount(int device) {
          return mCameraList.get(device).sizes.size();
    }

    public static int getSize(int device, int id) {
          return mCameraList.get(device).sizes.get(id);
    }

    public static int getFormat() {
        return ImageFormat.YUV_420_888;
    }

    public static CameraInfo getCameraInfoForName(String name) {
        for (CameraInfo c : mCameraList) {
            System.out.println("Compare "+name+" "+c.name);
            if (name.equals(c.name))
                return c;
        }

        return null;
    }
}
