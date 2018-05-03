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
#include "base.h"

#include <troubleshoot/dispatcher.h>

namespace Troubleshoot {

class BasePrivate
{
public:
    Dispatcher* m_pParent;
};

}

Troubleshoot::Base::Base(Dispatcher* parent) :
    QStringListModel(parent), d_ptr(new Troubleshoot::BasePrivate)
{
    d_ptr->m_pParent = parent;
}

Troubleshoot::Base::~Base()
{}

bool Troubleshoot::Base::setSelection(const QModelIndex& idx, Call* c)
{
    return false;
}

bool Troubleshoot::Base::setSelection(int idx, Call* c)
{
    return false;
}

Troubleshoot::Dispatcher* Troubleshoot::Base::dispatcher() const
{
    return d_ptr->m_pParent;
}

void Troubleshoot::Base::reset()
{}

void Troubleshoot::Base::activate()
{}

void Troubleshoot::Base::deactivate()
{}

