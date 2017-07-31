/****************************************************************************
 *   Copyright (C) 2017 by Savoir-faire Linux                               *
 *   Author : Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>      *
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

#include <QtCore/QObject>

//Std
#include <time.h>

class UsageStatisticsPrivate;

class UsageStatistics : public QObject
{
    friend class ContactMethod; //factory
    friend class ContactMethodPrivate; //factory

    Q_OBJECT
public:
    Q_PROPERTY(int totalSeconds   READ totalSeconds  NOTIFY changed)
    Q_PROPERTY(int lastWeekCount  READ lastWeekCount NOTIFY changed)
    Q_PROPERTY(int lastTrimCount  READ lastTrimCount NOTIFY changed)
    Q_PROPERTY(time_t lastUsed    READ lastUsed      NOTIFY changed)
    Q_PROPERTY(bool hasBeenCalled READ hasBeenCalled NOTIFY changed)


    // Getters
    virtual int totalSeconds  () const;
    virtual int lastWeekCount () const;
    virtual int lastTrimCount () const;
    virtual time_t lastUsed   () const;
    virtual bool hasBeenCalled() const;

Q_SIGNALS:
    void changed();

protected:
    explicit UsageStatistics(QObject* parent);
    virtual ~UsageStatistics();

private:
    UsageStatisticsPrivate* d_ptr;
    Q_DECLARE_PRIVATE(UsageStatistics);
};

Q_DECLARE_METATYPE(UsageStatistics*)
