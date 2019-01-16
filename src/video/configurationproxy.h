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
#pragma once

#include <typedefs.h>

class QAbstractItemModel;
class QItemSelectionModel;

class ConfigurationProxyPrivate;

namespace Video {

/**
 * This class is used to simplify the configuration process.
 * Currently, every devices have their own model tree. This
 * proxy flatten the three to the clients don't have to
 * implement the managing logic.
 */
class LIB_EXPORT ConfigurationProxy final : public QObject
{
    Q_OBJECT
    friend class DeviceModel; // factory
public:
    Q_PROPERTY(bool decodingAccellerated READ isDecodingAccelerated WRITE setDecodingAccelerated NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* deviceModel     READ deviceModel     CONSTANT)
    Q_PROPERTY(QAbstractItemModel* channelModel    READ channelModel    CONSTANT)
    Q_PROPERTY(QAbstractItemModel* resolutionModel READ resolutionModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* rateModel       READ rateModel       CONSTANT)
    Q_PROPERTY(QItemSelectionModel* deviceSelectionModel     READ deviceSelectionModel     CONSTANT)
    Q_PROPERTY(QItemSelectionModel* channelSelectionModel    READ channelSelectionModel    CONSTANT)
    Q_PROPERTY(QItemSelectionModel* resolutionSelectionModel READ resolutionSelectionModel CONSTANT)
    Q_PROPERTY(QItemSelectionModel* rateSelectionModel       READ rateSelectionModel       CONSTANT)

    QAbstractItemModel* deviceModel    ();
    QAbstractItemModel* channelModel   ();
    QAbstractItemModel* resolutionModel();
    QAbstractItemModel* rateModel      ();

    QItemSelectionModel* deviceSelectionModel    ();
    QItemSelectionModel* channelSelectionModel   ();
    QItemSelectionModel* resolutionSelectionModel();
    QItemSelectionModel* rateSelectionModel      ();

    bool isDecodingAccelerated();
    void setDecodingAccelerated(bool state);

Q_SIGNALS:
    void changed();

private:
    explicit ConfigurationProxy();
    virtual ~ConfigurationProxy();

    ConfigurationProxyPrivate* d_ptr;
    Q_DECLARE_PRIVATE(ConfigurationProxy)
};

} //namespace Video
