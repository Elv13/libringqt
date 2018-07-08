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
#include "handshake.h"

#include <call.h>

namespace Troubleshoot {

class HandshakePrivate
{
    //
};

}

/*
 *   * Let the user know he/she/it can try to tweak various settings when
 *     specific aspects of the handshake failed
 *       * UPnP (then eventually open ports in the router)
 *       * There is a codec mismatch
 *          * Patch libring to disclose the peer codec list
 *          * Add a button to enable some codecs and renegotiate the call
 *          * Know when the peer voluntarily disabled video
*/

Troubleshoot::Handshake::Handshake(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new HandshakePrivate())
{}

Troubleshoot::Handshake::~Handshake()
{
    delete d_ptr;
}

QString Troubleshoot::Handshake::headerText() const
{
    return {};
}

Troubleshoot::Base::Severity Troubleshoot::Handshake::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::Handshake::setSelection(const QModelIndex& idx, Call* c)
{
    Q_UNUSED(idx)
    Q_UNUSED(c)
    return false;
}

bool Troubleshoot::Handshake::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    Q_UNUSED(self)
    return c->lifeCycleState() == Call::LifeCycleState::INITIALIZATION && elapsedTime >= 15;
}

int Troubleshoot::Handshake::timeout()
{
    return 10;
}
