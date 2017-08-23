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

#include <QtCore/QObject>
#include <itembase.h>

#include <typedefs.h>

class RecordingPlaybackManager;
class Call;

namespace Media {

class RecordingPrivate;

/**
 * @class Recording a representation of one or more media recording
 */
class LIB_EXPORT Recording : public ItemBase
{
   Q_OBJECT

public:

   //Properties
   Q_PROPERTY(Type  type READ type)
   Q_PROPERTY(Call* call READ call) //Prevent setting from QML

   enum class Type {
      AUDIO_VIDEO, /*!< The recording is a single file, playable by the daemon */
      TEXT       , /*!< The recording is an encoded text stream and a position */
      /*FILE*/
   };
   Q_ENUMS(Type)

   /** If the user has consumed (read, listened, watched, downloaded) the recording
    *
    * Warning: Do not break the integer values, they are serialized.
    */
   enum class Status {
       NEW        = 0, /*!< Unconsumed and recording comes from this session       */
       UNCONSUMED = 1, /*!< The recording hasn't been used by the user yet         */
       UPDATED    = 2, /*!< It was CONSUMED, but new sub-entries were added/edited */
       CONSUMED   = 3, /*!< The user has seen/listened/downloaded the recording    */
       DISCARDED  = 4, /*!< The recording shall not be saved anymore               */
       COUNT__,
   };

   /// Actions to perform on the recording.
   enum class Action {
       CONSUME   , /*!< Mark as read/watched/listened/downloaded */
       DISCARD   , /*!< Remove from the recording register       */
       UNCONSUME , /*!< Mark as unread/unwatched/unheard         */
       COUNT__
   };

   //Constructor
   explicit Recording(const Recording::Type type, const Recording::Status status);
   virtual ~Recording();

   //Getter
   Recording::Type type() const;
   Call* call() const;

   virtual QVariant roleData(int role) const;

   //Setter
   void setCall(Call* call);

private:
   RecordingPrivate* d_ptr;
   Q_DECLARE_PRIVATE(Recording)
};

}
Q_DECLARE_METATYPE(::Media::Recording*)
