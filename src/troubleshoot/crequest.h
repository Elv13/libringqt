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

class CRequestPrivate;

/**
 * When a ContactRequest should be sent first.
 */
class LIB_EXPORT CRequest : public Base
{
    Q_OBJECT
public:
    explicit CRequest(Dispatcher* parent = nullptr);
    virtual ~CRequest();

    virtual QString headerText() const override;
    virtual Base::Severity severity() const override;

    virtual bool setSelection(const QModelIndex& idx, Call* c) override;

    virtual void reset() override;

    /**
     * Called when the state or error code changes.
     */
    static bool isAffected(Call* c, time_t elapsedTime = 0, Troubleshoot::Base* self = nullptr);

    /**
     * The time it takes in a state before `isAffected` has to be called again.
     */
    static int timeout();

private:
    CRequestPrivate* d_ptr;
    Q_DECLARE_PRIVATE(CRequest)
};

}
