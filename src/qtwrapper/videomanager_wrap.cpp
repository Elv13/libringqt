/******************************************************************************
 *   Copyright (C) 2014-2017 Savoir-faire Linux                                 *
 *   Author : Philippe Groarke <philippe.groarke@savoirfairelinux.com>        *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>        *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Lesser General Public               *
 *   License as published by the Free Software Foundation; either             *
 *   version 2.1 of the License, or (at your option) any later version.       *
 *                                                                            *
 *   This library is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *   Lesser General Public License for more details.                          *
 *                                                                            *
 *   You should have received a copy of the Lesser GNU General Public License *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *****************************************************************************/
#include "videomanager_wrap.h"

#ifdef Q_OS_ANDROID
 #include <QtCore/QSize>
 #include <globalinstances.h>
 #include <interfaces/videoformati.h>
 #include <interfaces/android/androidvideoformat.h>
#endif

VideoManagerInterface::VideoManagerInterface()
{
#ifdef ENABLE_VIDEO

    proxy = new VideoManagerSignalProxy(this);
    sender = new VideoManagerProxySender();

    QObject::connect(sender,&VideoManagerProxySender::deviceEvent,proxy,&VideoManagerSignalProxy::slotDeviceEvent, Qt::QueuedConnection);
    QObject::connect(sender,&VideoManagerProxySender::startedDecoding,proxy,&VideoManagerSignalProxy::slotStartedDecoding, Qt::QueuedConnection);
    QObject::connect(sender,&VideoManagerProxySender::stoppedDecoding,proxy,&VideoManagerSignalProxy::slotStoppedDecoding, Qt::QueuedConnection);

#ifdef Q_OS_ANDROID
    // This assumes the context has already been set.
    // It needs to be called just after the videoHandler is set
    //Interfaces::AndroidVideoFormat::init();
#endif

    using DRing::exportable_callback;
    using DRing::VideoSignal;
    videoHandlers = {
        exportable_callback<VideoSignal::DeviceEvent>(
            [this] () {
                emit sender->deviceEvent();
        }),
        exportable_callback<VideoSignal::DecodingStarted>(
            [this] (const std::string &id, const std::string &shmPath, int width, int height, bool isMixer) {
                emit sender->startedDecoding(QString(id.c_str()), QString(shmPath.c_str()), width, height, isMixer);
        }),
        exportable_callback<VideoSignal::DecodingStopped>(
            [this] (const std::string &id, const std::string &shmPath, bool isMixer) {
                emit sender->stoppedDecoding(QString(id.c_str()), QString(shmPath.c_str()), isMixer);
        }),
#ifdef Q_OS_ANDROID
        exportable_callback<VideoSignal::StartCapture>(
            [](const std::string& device) {
                static auto devices = GlobalInstances::videoFormatHandler().devices();

                Interfaces::VideoFormatI::AbstractDevice* d = nullptr;

                for (auto d_ : qAsConst(devices)) {
                    if (d_->name() == device.c_str()) {
                        d = d_;
                        break;
                    }
                }

                if (!d)
                    return;

               d->startCapture(); 

        }),
        exportable_callback<VideoSignal::StopCapture>(
            [](void) {
                GlobalInstances::videoFormatHandler().stopCapture();
        }),
        exportable_callback<VideoSignal::SetParameters>(
            [](const std::string& device, const int format, const int width, const int height, const int rate) {
                //

            }
        ),
        exportable_callback<VideoSignal::GetCameraInfo>(
            [](const std::string& device, std::vector<int> *formats, std::vector<unsigned> *sizes, std::vector<unsigned> *rates) {
                static auto devices = GlobalInstances::videoFormatHandler().devices();

                Interfaces::VideoFormatI::AbstractDevice* d = nullptr;

                for (auto d_ : qAsConst(devices)) {
                    if (d_->name() == device.c_str()) {
                        d = d_;
                        break;
                    }
                }

                if (!d)
                    return;

                formats->push_back( 842094169 ); //HACK just ****ing do it
                //formats->push_back( (int) d->formats().constFirst()->value());

                const auto drates = d->rates();
                for (auto rate : qAsConst(drates))
                    rates->push_back(rate->value());

                const auto dsizes = d->sizes();
                for (const QSize& s : qAsConst(dsizes)) {
                    sizes->push_back(s.width());
                    sizes->push_back(s.height());
                }
            }
        ),
#endif

    };
#endif
}

VideoManagerInterface::~VideoManagerInterface()
{

}

VideoManagerSignalProxy::VideoManagerSignalProxy(VideoManagerInterface* parent) : QObject(parent),
m_pParent(parent)
{}

void VideoManagerSignalProxy::slotDeviceEvent()
{
    emit m_pParent->deviceEvent();
}

void VideoManagerSignalProxy::slotStartedDecoding(const QString &id, const QString &shmPath, int width, int height, bool isMixer)
{
    emit m_pParent->startedDecoding(id,shmPath,width,height,isMixer);
}

void VideoManagerSignalProxy::slotStoppedDecoding(const QString &id, const QString &shmPath, bool isMixer)
{
    emit m_pParent->stoppedDecoding(id,shmPath,isMixer);
}

#ifdef Q_OS_ANDROID
void VideoManagerInterface::addVideoDevice(const QString& node, const QVector<QHash<QString, QString>>& devInfo)
{
    //TODO add the settings
    DRing::addVideoDevice(node.toStdString(), nullptr);
}

void VideoManagerInterface::removeVideoDevice(const QString& node)
{
    DRing::removeVideoDevice(node.toStdString());
}

void* VideoManagerInterface::obtainFrame(int length)
{
    return DRing::obtainFrame(length);
}

void VideoManagerInterface::releaseFrame(void* frame)
{
   DRing::releaseFrame(frame);
}

void VideoManagerInterface::publishFrame()
{
    DRing::publishFrame();
}
#endif
