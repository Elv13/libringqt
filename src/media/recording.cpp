/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
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
#include "recording.h"

#include "recordingmodel.h"
#include "libcard/matrixutils.h"

namespace Media {
typedef void (Recording::*RedFct)();

class RecordingPrivate {
public:
   RecordingPrivate(Recording* r);

   // Attributes
   Recording::Type        m_Type   { Recording::Type::TEXT  };
   Recording::Status      m_Status { Recording::Status::NEW };
   Call*                  m_pCall  {         nullptr        };

   static const Matrix2D<Recording::Status, Recording::Action, Recording::Status> m_mStateMap;
   static const Matrix2D<Recording::Status, Recording::Action, RedFct> m_mStateMachine;
private:
   Recording* q_ptr;
};

#define ST Recording::Status::
const Matrix2D<Recording::Status, Recording::Action, Recording::Status> RecordingPrivate::m_mStateMap ={{
   /*                                    CONSUME         DISCARD       UNCONSUME   */
   { Recording::Status::NEW       , {{ ST CONSUMED , ST DISCARDED, ST UNCONSUMED }}},
   { Recording::Status::UNCONSUMED, {{ ST CONSUMED , ST DISCARDED, ST UNCONSUMED }}},
   { Recording::Status::UPDATED   , {{ ST CONSUMED , ST DISCARDED, ST UNCONSUMED }}},
   { Recording::Status::CONSUMED  , {{ ST CONSUMED , ST DISCARDED, ST UNCONSUMED }}},
   { Recording::Status::DISCARDED , {{ ST ERROR    , ST DISCARDED, ST ERROR      }}},
   { Recording::Status::ERROR     , {{ ST ERROR    , ST DISCARDED, ST ERROR      }}},
}};
#undef ST

#define REC &Recording::
const Matrix2D<Recording::Status, Recording::Action, RedFct> RecordingPrivate::m_mStateMachine ={{
   /*                                    CONSUME        DISCARD       UNCONSUME    */
   { Recording::Status::NEW       , {{ REC consume , REC discard , REC unconsume }}},
   { Recording::Status::UNCONSUMED, {{ REC consume , REC discard , REC unconsume }}},
   { Recording::Status::UPDATED   , {{ REC consume , REC discard , REC unconsume }}},
   { Recording::Status::CONSUMED  , {{ REC consume , REC discard , REC unconsume }}},
   { Recording::Status::DISCARDED , {{ REC error   , REC error   , REC error     }}},
   { Recording::Status::ERROR     , {{ REC nothing , REC nothing , REC nothing   }}},
}};
#undef REC

RecordingPrivate::RecordingPrivate(Recording* r) : q_ptr(r)
{}

Recording::Recording(const Recording::Type type, const Recording::Status status) : ItemBase(nullptr),
d_ptr(new RecordingPrivate(this))
{
   //FIXME setParent(&RecordingModel::instance());
   d_ptr->m_Type = type;
   d_ptr->m_Status = status;
}

Recording::~Recording()
{
   delete d_ptr;
}

} //Media::

// Default mutator functions
void Media::Recording::consume  () {}
void Media::Recording::unconsume() {}
void Media::Recording::discard  () {}
void Media::Recording::error    () {}
void Media::Recording::nothing  () {}

bool Media::Recording::performAction(const Media::Recording::Action action)
{
   (this->*(d_ptr->m_mStateMachine[d_ptr->m_Status][action]))();
   d_ptr->m_Status = d_ptr->m_mStateMap[d_ptr->m_Status][action];

   return d_ptr->m_Status == Media::Recording::Status::ERROR;
}

Media::Recording* Media::Recording::operator<<(Media::Recording::Action& action)
{
   performAction(action);
   return this;
}

///Return this Recording type
Media::Recording::Type Media::Recording::type() const
{
   return d_ptr->m_Type;
}

Media::Recording::Status Media::Recording::status() const
{
   return d_ptr->m_Status;
}

Call* Media::Recording::call() const
{
   return d_ptr->m_pCall;
}

void Media::Recording::setCall(Call* call)
{
   d_ptr->m_pCall = call;
}

QVariant Media::Recording::roleData(int role) const
{
    Q_UNUSED(role)
    return {};
}

#include <recording.moc>
