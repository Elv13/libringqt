/****************************************************************************
 *   Copyright (C) 2018 by Bluesystems                                      *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                    *
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

#include "typedefs.h"
#include <QtCore/QObject>

class Individual;

namespace Media {

class AvailabilityTrackerPrivate;

/**
 * Track which kind of media can currently be used on an individual.
 *
 * To do this, this object watch signals and states from various objects to
 * provide real time information.
 */
class LIB_EXPORT AvailabilityTracker : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(bool canCall READ canCall NOTIFY changed)
    Q_PROPERTY(bool canVideoCall READ canVideoCall NOTIFY changed)
    Q_PROPERTY(bool canSendTexts READ canSendTexts NOTIFY changed)
    Q_PROPERTY(bool hasWarning READ hasWarning NOTIFY changed)
    Q_PROPERTY(QString warningMessage READ warningMessage NOTIFY changed)
    Q_PROPERTY(Individual* individual READ individual WRITE setIndividual)

    explicit AvailabilityTracker(QObject* parent = nullptr);
    virtual ~AvailabilityTracker();

    bool canCall() const;
    bool canVideoCall() const;
    bool canSendTexts() const;

    bool hasWarning() const;

    QString warningMessage() const;

    Individual* individual() const;
    void setIndividual(Individual* ind);

Q_SIGNALS:
    void changed();

private:
    AvailabilityTrackerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(AvailabilityTracker)
};

} //namespace
