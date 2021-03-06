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
#pragma once


namespace Interfaces {

/**
 * To implement on plarforms where the media frames needs to be processed by
 * the client (Android and iOS)
 */
class Q_DECL_EXPORT VideoFormatI
{
public:
    struct AbstractFormat {
        virtual int value() = 0;
    };

    struct AbstractRate {
        virtual int value() = 0;
    };

    struct AbstractDevice {
        virtual QString name() const = 0;
        virtual QVector<QSize> sizes() = 0;
        virtual QVector<AbstractFormat*> formats() const = 0;
        virtual QVector<AbstractRate*> rates() const = 0;

        virtual void setSize(const QSize& s) = 0;
        virtual void setRate(AbstractRate* r) = 0;

        virtual void select() = 0;
        virtual void startCapture() = 0;
    };

    virtual void stopCapture() = 0;
    virtual QVector<AbstractDevice*> devices() const = 0;
};

}
