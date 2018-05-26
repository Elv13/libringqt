/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                            *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "crequest.h"

#include <call.h>
#include <contactmethod.h>
#include <account.h>

namespace Troubleshoot {

class CRequestPrivate
{
public:
};

}

Troubleshoot::CRequest::CRequest(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new CRequestPrivate())
{
    setStringList({tr("Send a friend request now")});
}

Troubleshoot::CRequest::~CRequest()
{
    delete d_ptr;
}

QString Troubleshoot::CRequest::headerText() const
{
    static QString m = tr("You did not send a friend request to this person yet. If this person has disabled receiving calls from unknown peers, this call will fail.");
    return m;
}

Troubleshoot::Base::Severity Troubleshoot::CRequest::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::CRequest::setSelection(const QModelIndex& idx, Call* c)
{
    if (!idx.isValid())
        return false;

    c->account()->sendContactRequest(c->peerContactMethod());

    return true;
}

bool Troubleshoot::CRequest::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    const auto st = c->peerContactMethod()->confirmationStatus();

    if (!c->account())
        return false;

    return (elapsedTime >= 5 && c->state() == Call::State::ERROR
      || c->state() == Call::State::FAILURE
      || c->lifeCycleState() == Call::LifeCycleState::INITIALIZATION
    ) && c->direction() == Call::Direction::OUTGOING &&
        c->state() != Call::State::RINGING &&
        c->account()->protocol() == Account::Protocol::RING &&
        !(
            st == ContactMethod::ConfirmationStatus::CONFIRMED ||
            st == ContactMethod::ConfirmationStatus::PENDING
         );
}

int Troubleshoot::CRequest::timeout()
{
    return 5;
}

void Troubleshoot::CRequest::reset()
{
}
