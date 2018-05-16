/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
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

//Internal
#include "usagestatistics.h"
#include "uri.h"
#include <contactmethod.h>

class Call;
class Person;
class NumberCategory;
class EventAggregate;

namespace Media {
    class TextRecording;
}

class IndividualTimelineModel;

struct ContactMethodEvents; // defined in eventmodel.cpp
struct ContactMethodIndividualData; // defined in individual.cpp

class ContactMethodPrivate final: public QObject
{
    Q_OBJECT
public:
   ContactMethodPrivate(const URI& number, NumberCategory* cat, ContactMethod::Type st,
                        ContactMethod* q);
   virtual ~ContactMethodPrivate();

   NumberCategory*    m_pCategory         {nullptr};
   bool               m_Present           { false };
   QString            m_PresentMessage             ;
   bool               m_Tracked           { false };
   Person*            m_pPerson           {nullptr};
   Account*           m_pAccount          {nullptr};
   QString            m_MostCommonName             ;
   QHash<QString,QPair<int,time_t>> m_hNames       ;
   bool               m_hasType           { false };
   bool               m_IsBookmark        { false };
   QString            m_Uid                        ;
   mutable QString    m_PrimaryName_cache          ;
   URI                m_Uri                        ;
   QByteArray         m_Sha1                       ;
   ContactMethod::Type  m_Type                     ;
   QList<URI>         m_lOtherURIs                 ;
   Media::TextRecording* m_pTextRecording {nullptr};
   Certificate*       m_pCertificate      {nullptr};
   QString            m_RegisteredName             ;
   UsageStatistics*   m_pUsageStats       {nullptr};
   QVector<Media::TextRecording*> m_lAltTR;

   /**
    * Cache some relevant events to make insertion and eventually sorting
    * faster.
    *
    * The newest and oldest and kept regardless whether or not all events
    * are sorted for this CM.
    *
    * The unsorted pair are managed by the EventModel to allow it to eventually
    * run a merge sort on the linked list and generate the aggregates.
    */
   ContactMethodEvents* m_pEvents {nullptr};
   void addTimeRange(time_t start, time_t end, Event::EventCategory c);

   /**
    * The individual own properties (opaque pointer to the CM)
    */
   ContactMethodIndividualData* m_pIndividualData {nullptr};

   QWeakPointer<QAbstractItemModel> m_CallsModel;
   QWeakPointer<Individual> m_pIndividual;
   QSharedPointer<EventAggregate> m_pEventAggregate;

   //Parents
   QSet<ContactMethod*> m_lParents;
   ContactMethod* m_pOriginal {nullptr};

   //Emit proxies
   void callAdded(Call* call);
   void changed  (          );
   void mediaAvailabilityChanged();
   void presentChanged(bool);
   void presenceMessageChanged(const QString&);
   void trackedChanged(bool);
   void primaryNameChanged(const QString& name);
   void rebased(ContactMethod* other);
   void registeredNameSet(const QString& registeredName);
   void bookmarkedChanged(bool);
   void accountChanged();
   void contactChanged(Person*,Person*);

   //Helpers
   void addActiveCall(Call* c);
   void removeActiveCall(Call* c);
   void addInitCall(Call* c);
   void removeInitCall(Call* c);
   void setTextRecording(Media::TextRecording* r);
   void addAlternativeTextRecording(Media::TextRecording* recording);
   void setCertificate (Certificate*);
   void setRegisteredName(const QString& registeredName);
   void setLastUsed(time_t t);
   bool setType(ContactMethod::Type t);
   void setUid(const QString& uri);
   void setPresent(bool present);

public Q_SLOTS:
   void slotAccountDestroyed(QObject* o);
   void slotContactRebased(Person* other);

 private:
   ContactMethod* q_ptr;
};
