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
#include "androidaudioformat.h"

#include <QtAndroid>
#include <QAndroidJniObject>
#include <QtCore/QMutex>

static constexpr const char uri   [] = "net/lvindustries/libringqt/HardwareProxy";
static constexpr const char ctxUri[] = "(Landroid/content/Context;)I";

void Interfaces::AndroidAudioFormat::init()
{
    QMutex m;
    m.lock();

qDebug()<< "================ABOUT TO READ the content";


    // First, set the context in the Java class.
    QtAndroid::runOnAndroidThread([&m]() {
        qDebug()<< "======SETCONTEXT";
       auto ctx = QtAndroid::androidContext().object();
        QAndroidJniObject::callStaticMethod<jint>(uri, "setContext", ctxUri, QtAndroid::androidContext().object());
        m.unlock();
    });

    // Wait until the context is set
    m.lock();
}

Interfaces::AndroidAudioFormat::AndroidAudioFormat()
{}

int Interfaces::AndroidAudioFormat::bufferSize() const
{
    QMutex m;
    m.lock();

    int res = 0;

    QtAndroid::runOnAndroidThread([&m, &res]() {
        res = QAndroidJniObject::callStaticMethod<jint>(
            uri, "bufferSize", "(I)I"
        );
        m.unlock();
    });

    // Wait until the context is set
    m.lock();

    Q_ASSERT(res);

    return res;
}

int Interfaces::AndroidAudioFormat::sampleRate() const
{
    QMutex m;
    m.lock();

    int res = 0;

    QtAndroid::runOnAndroidThread([&m, &res]() {
        res = QAndroidJniObject::callStaticMethod<jint>(
            uri, "sampleRate", "(I)I"
        );
        m.unlock();
    });

    // Wait until the context is set
    m.lock();

    Q_ASSERT(res);

    return res;
}

QString Interfaces::AndroidAudioFormat::deviceModel() const
{
    QMutex m;
    m.lock();

    QString ret = 0;

    QtAndroid::runOnAndroidThread([&m, &ret]() {
        QAndroidJniObject o = QAndroidJniObject::callStaticObjectMethod(
            uri, "deviceModel", "()Ljava/lang/String;"
        );
        ret = o.toString();
        m.unlock();
    });

    // Wait until the context is set
    m.lock();

    return ret;
}

QString Interfaces::AndroidAudioFormat::deviceManufacturer() const
{
    QMutex m;
    m.lock();

    QString ret = 0;

    QtAndroid::runOnAndroidThread([&m, &ret]() {
        QAndroidJniObject o = QAndroidJniObject::callStaticObjectMethod(
            uri, "deviceManufacturer", "()Ljava/lang/String;"
        );
        ret = o.toString();
        m.unlock();
    });

    // Wait until the context is set
    m.lock();

    return ret;
}
