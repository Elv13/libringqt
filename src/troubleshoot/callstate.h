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

class CallStatePrivate;

/**
 * Handle "normal" call failure such as "the line is currently busy",
 * "missed calls" and manually dismissed calls.
 */
class LIB_EXPORT CallState : public Base
{
    Q_OBJECT
public:
    explicit CallState(Dispatcher* parent = nullptr);
    virtual ~CallState();

    virtual QString headerText() const override;
    virtual Base::Severity severity() const override;

    virtual bool setSelection(const QModelIndex& idx, Call* c) override;

    virtual void reset() override;

    virtual QVariant icon() const override;

    virtual int autoDismissDelay() const override;

    /**
     * Called when the state or error code changes.
     */
    static bool isAffected(Call* c, time_t elapsedTime = 0, Troubleshoot::Base* self = nullptr);

    /**
     * The time it takes in a state before `isAffected` has to be called again.
     */
    static int timeout();

private:
    CallStatePrivate* d_ptr;
    Q_DECLARE_PRIVATE(CallState)
};

}
