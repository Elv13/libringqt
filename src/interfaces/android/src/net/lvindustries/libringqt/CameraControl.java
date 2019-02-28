package net.lvindustries.libringqt;

import android.hardware.Camera;
import android.hardware.camera2.CameraManager;
import net.lvindustries.libringqt.HardwareProxy;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;

import android.os.Handler;
import android.os.HandlerThread;

import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.graphics.ImageFormat;

import android.content.Context;

import android.util.Range;
import java.util.ArrayList;
import java.util.List;
import android.util.Size;
import java.util.Comparator;
import java.util.Collections;

import android.view.Surface;

class CameraControlNative {
    public static native void captureVideoFrame(Object image, int rotation);
}

//import net.lvindustries.libringqt.CameraControlC;

// This code is imported from the Jami Android client as-is because it is the
// only code that's going to work anyway. Everything else is unsupported by
// LibRing/LibJami
public class CameraControl {
    private static final HandlerThread t = new HandlerThread("videoHandler");
    private static CameraDevice previewCamera;

    private static final int FPS_MAX = 30;
    private static final int FPS_TARGET = 15;

    public CameraControl() {

    }
    interface CameraListener {
        void onOpened();
        void onError();
    }

    private static final ImageReader.OnImageAvailableListener mOnImageAvailableListener = new ImageReader.OnImageAvailableListener() {
        @Override
        public void onImageAvailable(ImageReader reader) {
            System.out.println("=====IN IMAGE AVAIL!");
            Image image = reader.acquireLatestImage();
            System.out.println("GOT IMAGE");
            if (image != null)
                CameraControlNative.captureVideoFrame(image, 0);
        }
    }; 

    /**
     * Compares two {@code Size}s based on their areas.
     */
    static class CompareSizesByArea implements Comparator<Size> {

        @Override
        public int compare(Size lhs, Size rhs) {
            // We cast here to ensure the multiplications won't overflow
            return Long.signum((long) lhs.getWidth() * lhs.getHeight() -
                    (long) rhs.getWidth() * rhs.getHeight());
        }

    }

    private static Range<Integer> chooseOptimalFpsRange(Range<Integer>[] ranges) {
        Range<Integer> range = null;
        if (ranges != null && ranges.length > 0) {
            for (Range<Integer> r : ranges) {
                if (r.getUpper() > FPS_MAX)
                    continue;
                if (range != null) {
                    int d = Math.abs(r.getUpper() - FPS_TARGET) - Math.abs(range.getUpper() - FPS_TARGET);
                    if (d > 0)
                        continue;
                    if (d == 0 && r.getLower() > range.getLower())
                        continue;
                }
                range = r;
            }
            if (range == null)
                range = ranges[0];
        }
        return range == null ? new Range<>(FPS_TARGET, FPS_TARGET) : range;
    }

    private static Size chooseOptimalSize(Size[] choices, int textureViewWidth, int textureViewHeight, int maxWidth, int maxHeight, Size target) {
        if (choices == null)
            return target;

        // Collect the supported resolutions that are at least as big as the preview Surface
        List<Size> bigEnough = new ArrayList<>();

        // Collect the supported resolutions that are smaller than the preview Surface
        List<Size> notBigEnough = new ArrayList<>();

        int w = target.getWidth();
        int h = target.getHeight();
        for (Size option : choices) {
            System.out.println("supportedSize: " + option);
            if (option.getWidth() <= maxWidth && option.getHeight() <= maxHeight &&
                    option.getHeight() == option.getWidth() * h / w) {
                if (option.getWidth() >= textureViewWidth &&
                        option.getHeight() >= textureViewHeight) {
                    bigEnough.add(option);
                }
                else {
                    notBigEnough.add(option);
                }
            }
        }

        // Pick the smallest of those big enough. If there is no one big enough, pick the
        // largest of those not big enough.
        if (bigEnough.size() > 0) {
            return Collections.min(bigEnough, new CompareSizesByArea());
        }
        else if (notBigEnough.size() > 0) {
            return Collections.max(notBigEnough, new CompareSizesByArea());
        }
        else {
            System.out.println("Couldn't find any suitable preview size");
            return choices[0];
        }
    }

	public static void openCamera(Context context, String id, int width, int height, int rate, CameraListener listener) {
        CameraDevice camera = previewCamera;
        if (camera != null) {
            camera.close();
        }

        if (HardwareProxy.getCameraManager() == null)
            return;

        if (t.getState() == Thread.State.NEW)
            t.start();

        final Handler handler = new Handler(t.getLooper());

        System.out.println("Attempting to open the camera");

        try {
            CameraCharacteristics cc = HardwareProxy.getCameraManager().getCameraCharacteristics(id);
            final Range<Integer> fpsRange = chooseOptimalFpsRange(cc.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES));
            System.out.println("Selected preview size: " + ", fps range: " + fpsRange + " rate: "+rate);
            
            //TODO
            /*AutoFitTextureView view = (AutoFitTextureView) surface;
            boolean flip = videoParams.rotation % 180 != 0;


            StreamConfigurationMap streamConfigs = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            final Size previewSize = chooseOptimalSize(streamConfigs == null ? null : streamConfigs.getOutputSizes(SurfaceHolder.class),
                    flip ? view.getHeight() : view.getWidth(), flip ? view.getWidth() : view.getHeight(),
                    videoParams.width, videoParams.height,
                    new Size(videoParams.width, videoParams.height));

            int orientation = context.getResources().getConfiguration().orientation;
            if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
                view.setAspectRatio(previewSize.getWidth(), previewSize.getHeight());
            } else {
                view.setAspectRatio(previewSize.getHeight(), previewSize.getWidth());
            }*/

            //SurfaceTexture texture = view.getSurfaceTexture();
            //Surface s = new Surface(texture);

            //final Pair<MediaCodec, Surface> codec = null;/*USE_HARDWARE_ENCODER ? openCameraWithEncoder(videoParams, MediaFormat.MIMETYPE_VIDEO_VP8) : null;*/

            final List<Surface> targets = new ArrayList<>(2);
            //targets.add(s);
            ImageReader tmpReader = null;

            /*if (codec != null && codec.second != null) {
                targets.add(codec.second);
            } 
            else {*/
                tmpReader = ImageReader.newInstance(width, height, ImageFormat.YUV_420_888, 8);
                tmpReader.setOnImageAvailableListener(mOnImageAvailableListener, handler);
                targets.add(tmpReader.getSurface());
            //}

            final ImageReader reader = tmpReader;

            HardwareProxy.getCameraManager().openCamera(id, new CameraDevice.StateCallback() {
                @Override
                public void onOpened(CameraDevice camera) {
                    try {
                        System.out.println("onOpened");
                        previewCamera = camera;
                        //texture.setDefaultBufferSize(previewSize.getWidth(), previewSize.getHeight());
                        CaptureRequest.Builder builder = camera.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                        //builder.addTarget(s);
                        /*if (codec != null && codec.second != null) {
                            builder.addTarget(codec.second);
                        } else */if (reader != null) {
                            builder.addTarget(reader.getSurface());
                        }
                        builder.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                        builder.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON);
                        builder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, fpsRange);
                        builder.set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_AUTO);
                        final CaptureRequest request = builder.build();

                        camera.createCaptureSession(targets, new CameraCaptureSession.StateCallback() {
                            @Override
                            public void onConfigured(CameraCaptureSession session) {
                                System.out.println("onConfigured");
                                //listener.onOpened();
                                try {
                                    session.setRepeatingRequest(request, new CameraCaptureSession.CaptureCallback() {
                                        @Override
                                        public void onCaptureStarted(CameraCaptureSession session, CaptureRequest request, long timestamp, long frameNumber) {
                                            System.out.println("==============STARTED (FOR REAL!)");
                                            //if (frameNumber == 1) {
                                            //    codec.first.start();
                                            //}
                                        }
                                    }, handler);
                                } catch (CameraAccessException e) {
                                    System.out.println("onConfigured error:"+e);
                                }
                            }

                            @Override
                            public void onConfigureFailed(CameraCaptureSession session) {
                                //listener.onError();
                                System.out.println("onConfigureFailed");
                            }
                        }, handler);
                    } catch (Exception e) {
                        System.out.println("onOpened error:" + e);
                    }
                }

                @Override
                public void onDisconnected(CameraDevice camera) {
                    System.out.println("onDisconnected");
                    if (previewCamera == camera) {
                        previewCamera = null;
                    }
                    camera.close();
                    /*if (codec != null && codec.first != null) {
                        codec.first.signalEndOfInputStream();
                        codec.first.release();
                    }*/
                }

                @Override
                public void onError(CameraDevice camera, int error) {
                    System.out.println("onError: " + error);
                    if (previewCamera == camera)
                        previewCamera = null;
                    /*if (codec != null && codec.first != null) {
                        codec.first.release();
                    }*/
                    //listener.onError();
                }
            }, handler);
        } catch (SecurityException e) {
            System.out.println("Security exception while settings preview parameters"+e);
        } catch (Exception e) {
            System.out.println("Exception while settings preview parameters"+e);
        }
    }

    public static String startCapture(String deviceName) {
        System.out.println("=========== IN JAVA START CAPTURE");
        CameraManager m = HardwareProxy.getCameraManager();
        //openCamera(HardwareProxy.getContext(), "0", 1920, 1080, 30, null);

        HardwareProxy.CameraInfo cinf = HardwareProxy.getCameraInfoForName(deviceName);

        if (cinf == null) {
            System.out.println("Cannot find the camera no video: "+deviceName);
            return "";
        }

        try {
            int fps = cinf.rates.get(0);
            int width = cinf.sizes.get(0);
            int height = cinf.sizes.get(1);
            
            openCamera(HardwareProxy.getContext(), cinf.name, width, height, fps, null);
        }
        catch(Exception e) {
            System.out.println("The camera info are invalid"); 
        }

        return "";
    }

    public static int stopCapture() {
        if (previewCamera == null)
            return 1;

        CameraDevice camera = previewCamera;
        previewCamera = null;
        camera.close();
        return 0;
    }
}

