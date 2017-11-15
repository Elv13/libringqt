/****************************************************************************
 *   Copyright (C) 2012-2016 by Savoir-faire Linux                          *
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

// Qt
#include <QtCore/QByteArray>
class QMimeData;

// LRC
class Call;
class ContactMethod;
class Person;

namespace RingMimes {
   //TODO use QStringLiteral
   constexpr static const char* CALLID      = "x-ring/callid"            ;
   constexpr static const char* CONTACT     = "x-ring/contactid"         ;
   constexpr static const char* HISTORYID   = "x-ring/historyid"         ;
   constexpr static const char* PHONENUMBER = "x-ring/contactmethodid"   ;
   constexpr static const char* XRINGACTION = "x-ring/dropaction"        ;
   constexpr static const char* PLAIN_TEXT  = "text/plain"               ;
   constexpr static const char* HTML_TEXT   = "text/html"                ;
   constexpr static const char* PROFILE     = "text/ring.profile.id"     ;
   constexpr static const char* ACCOUNT     = "text/sflphone.account.id" ;
   constexpr static const char* AUDIO_CODEC = "text/ring.codec.audio"    ;
   constexpr static const char* VIDEO_CODEC = "text/ring.codec.video"    ;
   constexpr static const char* PROFILE_VCF = "x-ring/ring.profile.vcard";
   constexpr static const char* SNAPSHOT    = "x-ring/snapshot"          ;
   constexpr static const char* VCF         = "text/vcard"               ;
   constexpr static const char* XVCF        = "text/x-vcard"             ;
   constexpr static const char* MAC_VCF     = "application/vcard"        ;
   constexpr static const char* URI_LIST    = "text/uri-list"            ;

   QMimeData* payload(const Call* c, const ContactMethod* cm, const Person* p);

   enum class Actions {
      INVALID  = 0  ,
      JOIN     = 100,
      TRANSFER = 101,
   };

   namespace ActionNames {
      constexpr static const char* INVALID  = "invalid";
      constexpr static const char* JOIN     = "join";
      constexpr static const char* TRANSFER = "transfer";
   };

   QByteArray LIB_EXPORT toActionName(Actions action);
   Actions LIB_EXPORT fromActionName(const QByteArray& name);
}
