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
#include "androidvideoformat.h"

// Ring
#include <videomanager_interface.h>

// Qt
#include <QtAndroid>
#include <QAndroidJniObject>
#include <QtCore/QMutex>
#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtCore/QVector>

// FFMPEG (note, this is provided in the dring archive)
//extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
//}

static constexpr const char uri [] = "net/lvindustries/libringqt/HardwareProxy";
static constexpr const char curi[] = "net/lvindustries/libringqt/CameraControl";

static JavaVM* vm = nullptr;

extern "C" {
    JNIEXPORT jint JNI_OnLoad(JavaVM *vm_, void *reserved) 
    {
        vm = vm_;
        return JNI_VERSION_1_6;
    }
}

//BEGIN GPLv3 CODE
int AndroidFormatToAVFormat(int androidformat) {
    switch (androidformat) {
    case 17: // ImageFormat.NV21
        return AV_PIX_FMT_NV21;
    case 35: // ImageFormat.YUV_420_888
        return AV_PIX_FMT_YUV420P;
    case 39: // ImageFormat.YUV_422_888
        return AV_PIX_FMT_YUV422P;
    case 41: // ImageFormat.FLEX_RGB_888
        return AV_PIX_FMT_GBRP;
    case 42: // ImageFormat.FLEX_RGBA_8888
        return AV_PIX_FMT_GBRAP;
    default:
        return AV_PIX_FMT_NONE;
    }
}

void rotateNV21(uint8_t* yinput, uint8_t* uvinput, unsigned ystride, unsigned uvstride, unsigned width, unsigned height, int rotation, uint8_t* youtput, uint8_t* uvoutput)
{
    if (rotation == 0) {
        std::copy_n(yinput, ystride * height, youtput);
        std::copy_n(uvinput, uvstride * height, uvoutput);
        return;
    }

    if (rotation % 90 != 0 || rotation < 0 || rotation > 270) {
        qDebug() << "videomanager.i" << width << height << rotation;
        return;
    }

    bool swap      = rotation % 180 != 0;
    bool xflip     = rotation % 270 != 0;
    bool yflip     = rotation >= 180;
    unsigned wOut  = swap ? height : width;
    unsigned hOut  = swap ? width  : height;

    for (unsigned j = 0; j < height; j++) {
        for (unsigned i = 0; i < width; i++) {
            unsigned yIn = j * ystride + i;
            unsigned uIn = (j >> 1) * uvstride + (i & ~1);
            unsigned vIn = uIn + 1;
            unsigned iSwapped = swap ? j : i;
            unsigned jSwapped = swap ? i : j;
            unsigned iOut     = xflip ? wOut - iSwapped - 1 : iSwapped;
            unsigned jOut     = yflip ? hOut - jSwapped - 1 : jSwapped;
            unsigned yOut = jOut * wOut + iOut;
            unsigned uOut = (jOut >> 1) * wOut + (iOut & ~1);
            unsigned vOut = uOut + 1;
            youtput[yOut] = yinput[yIn];
            uvoutput[uOut] = uvinput[uIn];
            uvoutput[vOut] = uvinput[vIn];
        }
    }
    return;
}

extern "C" {
JNIEXPORT void JNICALL Java_net_lvindustries_libringqt_CameraControlNative_captureVideoFrame(JNIEnv *jenv, jclass jcls, jobject image, jint rotation);
};

JNIEXPORT void JNICALL Java_net_lvindustries_libringqt_CameraControlNative_captureVideoFrame(JNIEnv *jenv, jclass jcls, jobject image, jint rotation)
{
    jclass imageClass = jenv->GetObjectClass(image);

    auto frame = DRing::getNewFrame();

    if (!frame) {
        jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
        return;
    }
    
    auto avframe = frame->pointer();

    avframe->format = AndroidFormatToAVFormat(jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getFormat", "()I")));
    avframe->width = jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getWidth", "()I"));
    avframe->height = jenv->CallIntMethod(image, jenv->GetMethodID(imageClass, "getHeight", "()I"));
    jobject crop = jenv->CallObjectMethod(image, jenv->GetMethodID(imageClass, "getCropRect", "()Landroid/graphics/Rect;"));
    if (crop) {
        jclass rectClass = jenv->GetObjectClass(crop);
        avframe->crop_top = jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "top", "I"));
        avframe->crop_left = jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "left", "I"));
        avframe->crop_bottom = avframe->height - jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "bottom", "I"));
        avframe->crop_right = avframe->width - jenv->GetIntField(crop, jenv->GetFieldID(rectClass, "right", "I"));
    }

    bool directPointer = true;

    jobjectArray planes = (jobjectArray)jenv->CallObjectMethod(image, jenv->GetMethodID(imageClass, "getPlanes", "()[Landroid/media/Image$Plane;"));
    jsize planeCount = jenv->GetArrayLength(planes);

    if (avframe->format == AV_PIX_FMT_YUV420P) {
        jobject yplane = jenv->GetObjectArrayElement(planes, 0);
        jobject uplane = jenv->GetObjectArrayElement(planes, 1);
        jobject vplane = jenv->GetObjectArrayElement(planes, 2);
        jclass planeClass = jenv->GetObjectClass(yplane);
        jmethodID getBuffer = jenv->GetMethodID(planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;");
        jmethodID getRowStride = jenv->GetMethodID(planeClass, "getRowStride", "()I");
        jmethodID getPixelStride = jenv->GetMethodID(planeClass, "getPixelStride", "()I");

        auto ydata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(yplane, getBuffer));
        auto udata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(uplane, getBuffer));
        auto vdata = (uint8_t*)jenv->GetDirectBufferAddress(jenv->CallObjectMethod(vplane, getBuffer));
        auto ystride = jenv->CallIntMethod(yplane, getRowStride);
        auto uvstride = jenv->CallIntMethod(uplane, getRowStride);
        auto uvpixstride = jenv->CallIntMethod(uplane, getPixelStride);

        if (uvpixstride == 1) {
            avframe->data[0] = ydata;
            avframe->linesize[0] = ystride;
            avframe->data[1] = udata;
            avframe->linesize[1] = uvstride;
            avframe->data[2] = vdata;
            avframe->linesize[2] = uvstride;
        }
        else if (uvpixstride == 2) {
            // False YUV422, actually NV12 or NV21
            auto uvdata = std::min(udata, vdata);
            avframe->format = uvdata == udata ? AV_PIX_FMT_NV12 : AV_PIX_FMT_NV21;
            if (rotation == 0) {
                avframe->data[0] = ydata;
                avframe->linesize[0] = ystride;
                avframe->data[1] = uvdata;
                avframe->linesize[1] = uvstride;
            } 
            else {
                directPointer = false;
                bool swap = rotation != 0 && rotation != 180;
                auto ow = avframe->width;
                auto oh = avframe->height;
                avframe->width = swap ? oh : ow;
                avframe->height = swap ? ow : oh;
                //av_frame_get_buffer(avframe, 1); //FIXME prevent this from being stripped for the compined .so
                rotateNV21(ydata, uvdata, ystride, uvstride, ow, oh, rotation, avframe->data[0], avframe->data[1]);
                jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
            }
        }
    } 
    else {
        for (int i=0; i < planeCount; i++) {
            jobject plane = jenv->GetObjectArrayElement(planes, i);
            jclass planeClass = jenv->GetObjectClass(plane);
            jint stride = jenv->CallIntMethod(plane, jenv->GetMethodID(planeClass, "getRowStride", "()I"));
            jint pxStride = jenv->CallIntMethod(plane, jenv->GetMethodID(planeClass, "getPixelStride", "()I"));
            jobject buffer = jenv->CallObjectMethod(plane, jenv->GetMethodID(planeClass, "getBuffer", "()Ljava/nio/ByteBuffer;"));
            avframe->data[i] = (uint8_t *)jenv->GetDirectBufferAddress(buffer);
            avframe->linesize[i] = stride;
        }
    }

    if (directPointer) {
        image = jenv->NewGlobalRef(image);
        imageClass = (jclass)jenv->NewGlobalRef(imageClass);
        frame->setReleaseCb([jenv, image, imageClass](uint8_t *) mutable {
            bool justAttached = false;

            if (vm) { 
                int envStat = vm->GetEnv((void**)&jenv, JNI_VERSION_1_6);
                if (envStat == JNI_EDETACHED) {
                    justAttached = true;
                    if (vm->AttachCurrentThread(&jenv, nullptr) != 0)
                        return;
                } else if (envStat == JNI_EVERSION) {
                    return;
                }
            }

            jenv->CallVoidMethod(image, jenv->GetMethodID(imageClass, "close", "()V"));
            jenv->DeleteGlobalRef(image);
            jenv->DeleteGlobalRef(imageClass);
            if (vm && justAttached)
                vm->DetachCurrentThread();
        });
    }

    DRing::publishFrame();
}
//END GPLv3 CODE

struct AndroidFormat : public Interfaces::AndroidVideoFormat::AbstractFormat 
{
public:
    virtual int value() override {
        return m_Format;
    }

    int m_Format {};
};

struct AndroidRate : public Interfaces::AndroidVideoFormat::AbstractRate 
{
public:
    virtual int value() override {
        return m_Rate;
    }

    int m_Rate {0};
};

struct AndroidDevice : public Interfaces::AndroidVideoFormat::AbstractDevice 
{
public:
    virtual QString name() const override {return m_Name;}
    virtual QVector<QSize> sizes() override {return m_lSizes;}
    virtual QVector<Interfaces::AndroidVideoFormat::AbstractFormat*> formats() const override {return m_lFormats;}
    virtual QVector<Interfaces::AndroidVideoFormat::AbstractRate*> rates() const override {return m_lRates;}
    virtual void setSize(const QSize& s) override {}
    virtual void setRate(Interfaces::AndroidVideoFormat::AbstractRate* r) override {}
    virtual void select() override {}

    virtual void startCapture() override;

    QString m_Name;
    QVector<QSize> m_lSizes;
    QVector<Interfaces::AndroidVideoFormat::AbstractFormat*> m_lFormats;
    QVector<Interfaces::AndroidVideoFormat::AbstractRate*> m_lRates;
};

static QVector<Interfaces::AndroidVideoFormat::AbstractDevice*> s_lDevices;

void Interfaces::AndroidVideoFormat::init()
{
    QMutex m;
    m.lock();
    volatile int spin = 0;

    // First, set the context in the Java class.
    QtAndroid::runOnAndroidThread([&m, &spin]() {
        QAndroidJniObject::callStaticMethod<jint>(
            uri, "initCamera", "()I"
        );

        int counter = QAndroidJniObject::callStaticMethod<jint>(
            uri, "cameraCount", "()I"
        );

        for (int i = 0; i < counter; i++) {
            auto d = new AndroidDevice();

            QAndroidJniObject name = QAndroidJniObject::callStaticObjectMethod(
                uri, "getCameraName", "(I)Ljava/lang/String;", i
            );

            d->m_Name = name.toString();

            // Right now there's always one per device.
            const int rateCount = QAndroidJniObject::callStaticMethod<jint>(
                uri, "getFrameRate", "(II)I", i, 0
            );

            auto r = new AndroidRate();
            r->m_Rate = rateCount;

            const int sizesCount = QAndroidJniObject::callStaticMethod<jint>(
                uri, "getSizeCount", "(I)I", i, 0
            );

            for (int j = 0; j < sizesCount/2; j+=2) {
                const int width = QAndroidJniObject::callStaticMethod<jint>(
                    uri, "getSize", "(II)I", i, j
                );

                const int height = QAndroidJniObject::callStaticMethod<jint>(
                    uri, "getSize", "(II)I", i, j+1
                );

                d->m_lSizes << QSize{width, height};
            }

            auto f = new AndroidFormat;
            f->m_Format = QAndroidJniObject::callStaticMethod<jint>(
                uri, "getFormat", "()I"
            );

            d->m_lFormats << f;


            s_lDevices << d;
        }

        m.unlock();
        spin = 1;
    });

    // It is totally critical that this function returns only once the list is
    // complete. QMutex seems unreliable.
    while (!spin) {}
    // Wait until the context is set
    m.lock();

    for(auto d : qAsConst(s_lDevices))
        DRing::addVideoDevice(d->name().toStdString(), nullptr);

}

QVector<Interfaces::VideoFormatI::AbstractDevice*> Interfaces::AndroidVideoFormat::devices() const
{
    return s_lDevices;
}

void AndroidDevice::startCapture()
{
    qDebug() << "============C++ START CAPTURE";
    volatile int spin = 0;

    // First, set the context in the Java class.
    QtAndroid::runOnAndroidThread([this, &spin]() {
        QAndroidJniObject o = QAndroidJniObject::fromString(m_Name);
        auto res = QAndroidJniObject::callStaticObjectMethod(
            curi, "startCapture", "(Ljava/lang/String;)Ljava/lang/String;", o.object<jstring>()
        );

        spin = 1;
    });

    // Wait for this to take effect or else there's more race conditions
    while (!spin) {}
}


void Interfaces::AndroidVideoFormat::stopCapture()
{
    qDebug() << "=======C++ stop capture";

    // There is a lot of threads involved here, better than than try
    // to fix all races.
    QTimer::singleShot(100, []() {
        // No need to locking here
        QtAndroid::runOnAndroidThread([]() {
            QAndroidJniObject::callStaticMethod<jint>(
                uri, "stopCapture", "()I"
            );
        });
    });
}
