/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "configurationproxy.h"

//Qt
#include <QtCore/QIdentityProxyModel>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>

//Ring
#include <video/sourcemodel.h>
#include <video/devicemodel.h>
#include <video/channel.h>
#include <video/resolution.h>
#include <video/rate.h>
#include <session.h>
#include <dbus/videomanager.h>

class ConfigurationProxyPrivate : public QObject
{
    Q_OBJECT
public:
    QIdentityProxyModel* m_spDeviceModel    = nullptr;
    QIdentityProxyModel* m_spChannelModel   = nullptr;
    QIdentityProxyModel* m_spResolutionModel= nullptr;
    QIdentityProxyModel* m_spRateModel      = nullptr;

    QItemSelectionModel* m_spDeviceSelectionModel    = nullptr;
    QItemSelectionModel* m_spChannelSelectionModel   = nullptr;
    QItemSelectionModel* m_spResolutionSelectionModel= nullptr;
    QItemSelectionModel* m_spRateSelectionModel      = nullptr;

    Video::SourceModel*  m_sourceModel = nullptr;

    //Helper
    Video::Device*     currentDevice    ();
    Video::Channel*    currentChannel   ();
    Video::Resolution* currentResolution();
    //    static Video::Rate*       currentRate      ();

    ConfigurationProxy* q_ptr;

public Q_SLOTS:
    void changeDevice    ();
    void changeChannel   ();
    void changeResolution();
    void changeRate      ();

    void updateDeviceSelection    ();
    void updateChannelSelection   ();
    void updateResolutionSelection();
    void updateRateSelection      ();
};

ConfigurationProxy::ConfigurationProxy() : QObject(QCoreApplication::instance()),
    d_ptr(new ConfigurationProxyPrivate())
{
    d_ptr->q_ptr = this;
}

ConfigurationProxy::~ConfigurationProxy()
{
    delete d_ptr;
}

QAbstractItemModel* ConfigurationProxy::deviceModel()
{
    if (!d_ptr->m_spDeviceModel) {
        d_ptr->m_spDeviceModel = new QIdentityProxyModel(d_ptr->m_sourceModel);
        d_ptr->m_spDeviceModel->setSourceModel(Session::instance()->deviceModel());
        d_ptr->updateDeviceSelection();
    }

    return d_ptr->m_spDeviceModel;
}

Video::Device* ConfigurationProxyPrivate::currentDevice()
{
    return Session::instance()->deviceModel()->activeDevice();
}

Video::Channel* ConfigurationProxyPrivate::currentChannel()
{
    if (Session::instance()->deviceModel()->activeDevice()
      && Session::instance()->deviceModel()->activeDevice()->activeChannel())
        return Session::instance()->deviceModel()->activeDevice()->activeChannel();

    return nullptr;
}

Video::Resolution* ConfigurationProxyPrivate::currentResolution()
{
    if (Session::instance()->deviceModel()->activeDevice()
      && Session::instance()->deviceModel()->activeDevice()->activeChannel()
      && Session::instance()->deviceModel()->activeDevice()->activeChannel()->activeResolution()
    )
        return Session::instance()->deviceModel()->activeDevice()->activeChannel()->activeResolution();
    return nullptr;
}

/*static Video::Rate* ConfigurationProxyPrivate::currentRate()
{
   if (Session::instance()->deviceModel()->activeDevice()
    && Session::instance()->deviceModel()->activeDevice()->activeChannel()
    && Session::instance()->deviceModel()->activeDevice()->activeChannel()->activeResolution()
   )
      return Session::instance()->deviceModel()->activeDevice()->activeChannel()->activeResolution()->activeRate();

   return nullptr;
}*/

void ConfigurationProxyPrivate::changeDevice()
{
    q_ptr->deviceSelectionModel();

    Session::instance()->deviceModel()->setActive(m_spDeviceSelectionModel->currentIndex());

    reinterpret_cast<QIdentityProxyModel*>(q_ptr->channelModel())->setSourceModel(currentDevice());
    changeChannel();
}

void ConfigurationProxyPrivate::changeChannel()
{
    q_ptr->channelSelectionModel();

    Video::Device* dev = currentDevice();

    if (dev)
        dev->setActiveChannel(m_spChannelSelectionModel->currentIndex().row());

    reinterpret_cast<QIdentityProxyModel*>(q_ptr->resolutionModel())->setSourceModel(currentChannel());

    updateChannelSelection();

    changeResolution();
}

void ConfigurationProxyPrivate::changeResolution()
{
    q_ptr->resolutionSelectionModel();

    Video::Channel* chan = currentChannel();

    if (chan)
        chan->setActiveResolution(m_spResolutionSelectionModel->currentIndex().row());

    reinterpret_cast<QIdentityProxyModel*>(q_ptr->rateModel())->setSourceModel(currentResolution());

    updateResolutionSelection();

    changeRate();
}

void ConfigurationProxyPrivate::changeRate()
{
    q_ptr->rateSelectionModel();

    Video::Resolution* res = currentResolution();

    if (res)
        res->setActiveRate(m_spRateSelectionModel->currentIndex().row());

    updateRateSelection();
}

void ConfigurationProxyPrivate::updateDeviceSelection()
{
    if (m_spDeviceModel) {
        const auto idx = m_spDeviceModel->index(Session::instance()->deviceModel()->activeIndex(),0);
        if (idx.row() != q_ptr->deviceSelectionModel()->currentIndex().row())
            q_ptr->deviceSelectionModel()->setCurrentIndex(idx , QItemSelectionModel::ClearAndSelect);
    }
}

void ConfigurationProxyPrivate::updateChannelSelection()
{
    if (auto dev = currentDevice()) {
        Video::Channel* chan = dev->activeChannel();
        if (chan) {
            const auto newIdx = dev->index(chan->relativeIndex(),0);
            if (newIdx.row() != q_ptr->channelSelectionModel()->currentIndex().row())
                q_ptr->channelSelectionModel()->setCurrentIndex(newIdx, QItemSelectionModel::ClearAndSelect );
        }
    }
}

void ConfigurationProxyPrivate::updateResolutionSelection()
{
    if (auto chan = currentChannel()) {
        Video::Resolution* res = chan->activeResolution();
        if (res) {
            const auto newIdx = chan->index(res->relativeIndex(),0);
            if (newIdx.row() != q_ptr->resolutionSelectionModel()->currentIndex().row())
                q_ptr->resolutionSelectionModel()->setCurrentIndex(newIdx, QItemSelectionModel::ClearAndSelect);
        }
    }
}

void ConfigurationProxyPrivate::updateRateSelection()
{
    if (auto res = currentResolution()) {
        Video::Rate* rate = res->activeRate();
        if (rate) {
            const auto newIdx = res->index(rate->relativeIndex(),0);
            if (newIdx.row() != q_ptr->rateSelectionModel()->currentIndex().row())
                q_ptr->rateSelectionModel()->setCurrentIndex(newIdx, QItemSelectionModel::ClearAndSelect);
        }
    }
}

QAbstractItemModel* ConfigurationProxy::channelModel()
{
    if (!d_ptr->m_spChannelModel) {
        d_ptr->m_spChannelModel = new QIdentityProxyModel(d_ptr->m_sourceModel);
        Video::Device* dev = d_ptr->currentDevice();
        if (dev) {
            d_ptr->m_spChannelModel->setSourceModel(dev);
        }
    }

    return d_ptr->m_spChannelModel;
}

QAbstractItemModel* ConfigurationProxy::resolutionModel()
{
    if (!d_ptr->m_spResolutionModel) {
        d_ptr->m_spResolutionModel = new QIdentityProxyModel(d_ptr->m_sourceModel);
        Video::Channel* chan = d_ptr->currentChannel();
        if (chan) {
            d_ptr->m_spResolutionModel->setSourceModel(chan);
        }
    }

    return d_ptr->m_spResolutionModel;
}

QAbstractItemModel* ConfigurationProxy::rateModel()
{
    if (!d_ptr->m_spRateModel) {
        d_ptr->m_spRateModel = new QIdentityProxyModel(d_ptr->m_sourceModel);
        d_ptr->m_spRateModel->setSourceModel(d_ptr->currentResolution());
    }

    return d_ptr->m_spRateModel;
}

QItemSelectionModel* ConfigurationProxy::deviceSelectionModel()
{
    if (!d_ptr->m_spDeviceSelectionModel) {
        d_ptr->m_spDeviceSelectionModel = new QItemSelectionModel(deviceModel());

        d_ptr->updateDeviceSelection();

        //Can happen if a device is removed
        QObject::connect(Session::instance()->deviceModel(), &Video::DeviceModel::currentIndexChanged, Session::instance()->deviceModel(), [this](int idx) {
            d_ptr->m_spDeviceSelectionModel->setCurrentIndex(deviceModel()->index(idx,0), QItemSelectionModel::ClearAndSelect );
        });

        QObject::connect(d_ptr->m_spDeviceSelectionModel,&QItemSelectionModel::currentChanged,
            d_ptr, &ConfigurationProxyPrivate::changeDevice);
    }

    return d_ptr->m_spDeviceSelectionModel;
}

QItemSelectionModel* ConfigurationProxy::channelSelectionModel()
{
    if (!d_ptr->m_spChannelSelectionModel) {
        d_ptr->m_spChannelSelectionModel = new QItemSelectionModel(channelModel());

        d_ptr->updateChannelSelection();

        QObject::connect(d_ptr->m_spChannelSelectionModel,&QItemSelectionModel::currentChanged,
            d_ptr, &ConfigurationProxyPrivate::changeChannel);
    }

    return d_ptr->m_spChannelSelectionModel;
}

QItemSelectionModel* ConfigurationProxy::resolutionSelectionModel()
{
    if (!d_ptr->m_spResolutionSelectionModel) {
        d_ptr->m_spResolutionSelectionModel = new QItemSelectionModel(resolutionModel());

        d_ptr->updateResolutionSelection();

        QObject::connect(d_ptr->m_spResolutionSelectionModel,&QItemSelectionModel::currentChanged,
            d_ptr, &ConfigurationProxyPrivate::changeResolution);
    }

    return d_ptr->m_spResolutionSelectionModel;
}

QItemSelectionModel* ConfigurationProxy::rateSelectionModel()
{
    if (!d_ptr->m_spRateSelectionModel) {
        d_ptr->m_spRateSelectionModel = new QItemSelectionModel(rateModel());

        d_ptr->updateRateSelection();

        QObject::connect(d_ptr->m_spRateSelectionModel,&QItemSelectionModel::currentChanged,
            d_ptr, &ConfigurationProxyPrivate::changeRate);
    }

    return d_ptr->m_spRateSelectionModel;
}

bool ConfigurationProxy::isDecodingAccelerated()
{
    VideoManagerInterface& interface = VideoManager::instance();
    return interface.getDecodingAccelerated();
}

void ConfigurationProxy::setDecodingAccelerated(bool state)
{
    VideoManagerInterface& interface = VideoManager::instance();
    interface.setDecodingAccelerated(state);
}

#include <configurationproxy.moc>
