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
#include <QtCore/QMutex>

static constexpr const char uri[] = "net/lvindustries/libringqt/HardwareProxy";

// struct AndroidFormat : public Interfaces::VideoFormatI::AbstractFormat
// {
//     virtual QString name() override;
// };
//
// struct AndroidRate : public Interfaces::VideoFormatI::AbstractRate
// {
//     virtual int value() override;
// };
//
// struct AndroidDevice : public Interfaces::VideoFormatI::AbstractDevice
// {
//     virtual QString name() const override;
//     virtual QVector<QSize> sizes() override;
//     virtual QVector<Interfaces::VideoFormatI::AbstractFormat*> formats() const override;
//     virtual QVector<Interfaces::VideoFormatI::AbstractRate*> rates() const override;
//
//     virtual void setSize(const QSize& s) override;
//     virtual void setRate(Interfaces::VideoFormatI::AbstractRate* r) override;
//
//     virtual void select() override;
// };

void Interfaces::AndroidVideoFormat::init()
{
    QMutex m;
    m.lock();

    // First, set the context in the Java class.
    QtAndroid::runOnAndroidThread([&m]() {
        int counter = QAndroidJniObject::callStaticMethod<jint>(
            uri, "cameraCount", "()I"
        );

        for (int i = 0; i < counter; i++) {
            QAndroidJniObject name = QAndroidJniObject::callStaticMethod<jstring>(
                uri, "cameraCount", "(I)S", i
            );

            DRing::addVideoDevice(name.toString().toStdString(), nullptr);
        }

        m.unlock();
    });

    // Wait until the context is set
    m.lock();
}

QVector<VideoFormatI::AbstractDevice*> Interfaces::AndroidVideoFormat::devices() const
{
    return {};
}
