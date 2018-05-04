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
#include "videostuck.h"

#include <call.h>

namespace Troubleshoot {

class VideoStuckPrivate
{
    //
};

}

Troubleshoot::VideoStuck::VideoStuck(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new VideoStuckPrivate())
{}

Troubleshoot::VideoStuck::~VideoStuck()
{
    delete d_ptr;
}

QString Troubleshoot::VideoStuck::headerText() const
{
    return {};
}

Troubleshoot::Base::Severity Troubleshoot::VideoStuck::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::VideoStuck::setSelection(const QModelIndex& idx, Call* c)
{
    return false;
}

bool Troubleshoot::VideoStuck::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    return c->lifeCycleState() == Call::LifeCycleState::INITIALIZATION && elapsedTime >= 15;
}

int Troubleshoot::VideoStuck::timeout()
{
    return 10;
}
