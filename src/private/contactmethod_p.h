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

namespace Media {
    class TextRecording;
}

class PeerTimelineModel;

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

   QWeakPointer<QAbstractItemModel> m_CallsModel;
   QWeakPointer<Individual> m_pIndividual;

   //Parents
   QSet<ContactMethod*> m_lParents;
   ContactMethod* m_pOriginal {nullptr};

   //Emit proxies
   void callAdded(Call* call);
   void changed  (          );
   void canSendTextsChanged();
   void presentChanged(bool);
   void presenceMessageChanged(const QString&);
   void trackedChanged(bool);
   void primaryNameChanged(const QString& name);
   void rebased(ContactMethod* other);
   void registeredNameSet(const QString& registeredName);
   void bookmarkedChanged(bool);
   void accountChanged();

   //Helpers
   void addActiveCall(Call* c);
   void removeActiveCall(Call* c);
   void addInitCall(Call* c);
   void removeInitCall(Call* c);
   void setTextRecording(Media::TextRecording* r);
   void addAlternativeTextRecording(Media::TextRecording* recording);
   void setCertificate (Certificate*);
   void setRegisteredName(const QString& registeredName);

public Q_SLOTS:
   void slotAccountDestroyed(QObject* o);
   void slotContactRebased(Person* other);

 private:
   ContactMethod* q_ptr;
};
