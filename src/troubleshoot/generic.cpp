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
#include "generic.h"

#include <call.h>

namespace Troubleshoot {

class GenericPrivate
{
public:
    QString m_Message;
};

}

Troubleshoot::Generic::Generic(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new GenericPrivate())
{}

Troubleshoot::Generic::~Generic()
{
    delete d_ptr;
}

QString Troubleshoot::Generic::headerText() const
{
    return d_ptr->m_Message;
}

Troubleshoot::Base::Severity Troubleshoot::Generic::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::Generic::setSelection(const QModelIndex& idx, Call* c)
{
    return false;
}

bool Troubleshoot::Generic::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    if (c->state() == Call::State::ERROR || c->state() == Call::State::FAILURE) {
        if (c->lastErrorCode() >= 200 && c->lastErrorCode() < 300)
            return false;

        auto cself = static_cast<Troubleshoot::Generic*>(self);
        cself->d_ptr->m_Message = c->lastErrorMessage()
            + " (" + QString::number(c->lastErrorCode()) + ")";
        return true;
    }

    return false;
}

int Troubleshoot::Generic::timeout()
{
    return 0;
}

void Troubleshoot::Generic::reset()
{
    d_ptr->m_Message.clear();
}
