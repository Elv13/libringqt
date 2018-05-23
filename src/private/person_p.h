/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Jérémy Quentin <jeremy.quentin@savoirfairelinux.com>          *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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

//std
#include <time.h>

//Ring
#include "person.h"
class ContactMethod;
class IndividualTimelineModel;
class PersonStatistics;

class PersonPrivate final : public QObject
{
    Q_OBJECT
    friend class ContactMethod;
public:
    explicit PersonPrivate(Person* contact);
    virtual ~PersonPrivate();
    QString                  m_FirstName           ;
    QString                  m_SecondName          ;
    QString                  m_NickName            ;
    QVariant                 m_vPhoto              ;
    QString                  m_FormattedName       ;
    QString                  m_PreferredEmail      ;
    QString                  m_Organization        ;
    QByteArray               m_Uid                 ;
    QString                  m_Group               ;
    QString                  m_Department          ;
    bool                     m_DisplayPhoto        {false};
    bool                     m_isPlaceHolder       {false};
    QList<Address*>          m_lAddresses          ;
    PersonStatistics*        m_pStats              {nullptr};

    QMultiMap<QByteArray, QByteArray>  m_lCustomAttributes;

    Individual* m_pIndividual {nullptr};

    QWeakPointer<QAbstractItemModel> m_pAddressModel;

    Person* q_ptr;

    /*
    * NOTE If new attributes are added, please update the explicit Person copy
    * constructor as Qt force QObject copy via serialization (to force developers
    * to use references, copy-on-write based containers and smart pointers
    * instead), which is overkill for this scenario and would detach all the
    * containers causing useless increase in memory usage.
    */

    //Cache
    QString m_CachedFilterString;

    QString filterString();

    //Helper code to help handle multiple parents
    QList<Person*> m_lParents;

    //As a single D-Pointer can have multiple parent (when merged), all emit need
    //to use a proxy to make sure everybody is notified
    void presenceChanged          ( ContactMethod* );
    void trackedChanged           ( ContactMethod* );
    void statusChanged            ( bool           );
    void changed                  (                );
    void photoChanged             (                );

public Q_SLOTS:
    void slotTrackedChanged();
    void slotPresenceChanged();
};
