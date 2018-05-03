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
#pragma once

#include <troubleshoot/base.h>

class Call;

namespace Troubleshoot {

class GenericPrivate;

/**
 * When things go wrong *during* a live media session, letting it fail is an
 * option, but very often it is possible to involve the user directly and offer
 * option and information about why it failed and how to fix it.
 *
 * This is the base class of a chain of responsibility
 */
class LIB_EXPORT Generic : public Base
{
    Q_OBJECT
public:
    explicit Generic(Dispatcher* parent = nullptr);
    virtual ~Generic();

    virtual QString headerText() const override;
    virtual Base::Severity severity() const override;

    virtual bool setSelection(const QModelIndex& idx, Call* c) override;
    virtual bool setSelection(int idx, Call* c) override;

    /**
     * Called when the state or error code changes.
     */
    static bool isAffected(Call* c, time_t elapsedTime = 0, Troubleshoot::Base* self = nullptr);

    /**
     * The time it takes in a state before `isAffected` has to be called again.
     */
    static int timeout();

private:
    GenericPrivate* d_ptr;
    Q_DECLARE_PRIVATE(Generic)
};

}
