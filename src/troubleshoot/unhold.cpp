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
#include "unhold.h"

#include <call.h>

namespace Troubleshoot {

class UnholdPrivate
{
    //
};

}

Troubleshoot::Unhold::Unhold(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new UnholdPrivate())
{
    static QStringList options {
        tr( "Hang up this call" ),
        tr( "Try again"         ),
    };

    setStringList(options);
}

Troubleshoot::Unhold::~Unhold()
{
    delete d_ptr;
}

QString Troubleshoot::Unhold::headerText() const
{
    static auto message = tr("The call failed to be removed from hold. This usually means the peer is no longer present");
    return message;
}

Troubleshoot::Base::Severity Troubleshoot::Unhold::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::Unhold::setSelection(const QModelIndex& idx, Call* c)
{
    Q_UNUSED(idx)
    Q_UNUSED(c)
    return false;
}

bool Troubleshoot::Unhold::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    Q_UNUSED(self)
    Q_UNUSED(elapsedTime)
    return c->liveMediaIssues() & Call::LiveMediaIssues::UNHOLD_FAILED;
}

int Troubleshoot::Unhold::timeout()
{
    return 10;
}
