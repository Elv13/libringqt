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
#include "recording.h"

#include "recordingmodel.h"

namespace Media {

class RecordingPrivate {
public:
   RecordingPrivate(Recording* r);

   // Attributes
   Recording::Type        m_Type   { Recording::Type::TEXT  };
   Recording::Status      m_Status { Recording::Status::NEW };
   Call*                  m_pCall  {         nullptr        };

private:
   Recording* q_ptr;
};

RecordingPrivate::RecordingPrivate(Recording* r) : q_ptr(r)
{

}

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

///Return this Recording type
Media::Recording::Type Media::Recording::type() const
{
   return d_ptr->m_Type;
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
