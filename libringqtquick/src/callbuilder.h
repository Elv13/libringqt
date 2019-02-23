/***************************************************************************
 *   Copyright (C) 2019 by Blue Systems                                    *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef CALLBUILDER_H
#define CALLBUILDER_H

#include <QtCore/QObject>

// LibRingQt
class Individual;
class ContactMethod;
class Call;

class CallBuilderPrivate;

/**
 * Creating a call from an individual has many corner case.
 *
 * Doing so in JavaScript is error prone because the bindings are not so great.
 *
 * Use this class along with a SharedModelLocker and a Media.AvailabilityTracker
 * to handle the different aspects of the problem.
 */
class Q_DECL_EXPORT CallBuilder : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(Individual*    individual     READ individual       WRITE setIndividual    NOTIFY changed)
    Q_PROPERTY(ContactMethod* contactMethod  READ contactMethod    WRITE setContactMethod NOTIFY changed)
    Q_PROPERTY(bool           audio          READ hasAudio         WRITE setAudio         NOTIFY changed)
    Q_PROPERTY(bool           video          READ hasVideo         WRITE setVideo         NOTIFY changed)
    Q_PROPERTY(bool           screenSharing  READ hasScreenSharing WRITE setScreenSharing NOTIFY changed)
    Q_PROPERTY(Call*          activeCall     READ activeCall       NOTIFY changed)
    Q_PROPERTY(bool           choiceRequired READ isChoiceRequired NOTIFY changed)

    Q_INVOKABLE explicit CallBuilder(QObject* parent = nullptr);
    virtual ~CallBuilder();

    Individual* individual() const;
    void setIndividual(Individual* i);

    ContactMethod* contactMethod() const;
    void setContactMethod(ContactMethod* cm);

    bool hasAudio() const;
    void setAudio(bool v);

    bool hasVideo() const;
    void setVideo(bool v);

    bool hasScreenSharing() const;
    void setScreenSharing(bool v);

    Call* activeCall() const;

    bool isChoiceRequired() const;

    Q_INVOKABLE void reset();

    /**
     * Create a new active call with the selected properties.
     */
    Q_INVOKABLE Call* commit();

    /**
     * Update an active call (if any) with the selected properties.
     */
    Q_INVOKABLE Call* update();

Q_SIGNALS:
    void changed();

private:
    CallBuilderPrivate* d_ptr;
};

#endif
