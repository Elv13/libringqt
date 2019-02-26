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
#include <QtCore/QVector>

static constexpr const char uri[] = "net/lvindustries/libringqt/HardwareProxy";

    struct AndroidFormat : public Interfaces::AndroidVideoFormat::AbstractFormat {
    public:
        virtual int value() override {
            return m_Format;
        }

        int m_Format {};
    };

    struct AndroidRate : public Interfaces::AndroidVideoFormat::AbstractRate {
    public:
        virtual int value() override {
            return m_Rate;
        }

        int m_Rate {0};
    };

    struct AndroidDevice : public Interfaces::AndroidVideoFormat::AbstractDevice {
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

void Interfaces::AndroidVideoFormat::stopCapture()
{
    //
}
