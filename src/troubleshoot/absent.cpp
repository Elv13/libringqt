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
#include "absent.h"

// Ring
#include <call.h>
#include <callmodel.h>
#include <account.h>
#include <contactmethod.h>
#include <troubleshoot/dispatcher.h>

// stdc++
#include <system_error>

namespace Troubleshoot {

class AbsentPrivate
{
public:
    enum class State {
        NORMAL,
        NO_DEVICE,
        OFFLINE,
    } m_State {State::NO_DEVICE};

    enum Buttons {
        HANG_UP,
        TRY_AGAIN,
    };
};

}

Troubleshoot::Absent::Absent(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new AbsentPrivate())
{}

Troubleshoot::Absent::~Absent()
{
    delete d_ptr;
}

QString Troubleshoot::Absent::headerText() const
{
    if (dispatcher()->currentModule() != this)
        return {};

    static QString mess = tr("This contact doesn't seem to be online or has no "
        "compatible devices current connected. Please try again later.");

    static QString device = tr("This contact isn't connected."
        " Please try again later.");

    static QString offline = tr("This contact doesn't broadcast its presence status. "
        "The contact is probably offline, but may have enabled stealth mode.");

    switch (d_ptr->m_State) {
        case AbsentPrivate::State::NORMAL:
            return mess;
        case AbsentPrivate::State::NO_DEVICE:
            return device;
        case AbsentPrivate::State::OFFLINE:
            return offline;
    }

    return {};
}

Troubleshoot::Base::Severity Troubleshoot::Absent::severity() const
{
    return d_ptr->m_State == AbsentPrivate::State::NO_DEVICE ?
        Base::Severity::ERROR : Base::Severity::WARNING;
}

bool Troubleshoot::Absent::setSelection(const QModelIndex& idx, Call* c)
{
    auto cm = c->peerContactMethod();

    switch((AbsentPrivate::Buttons) idx.row()) {
        case AbsentPrivate::Buttons::HANG_UP:
            c << Call::Action::REFUSE;
            break;
        case AbsentPrivate::Buttons::TRY_AGAIN:
            c << Call::Action::REFUSE;
            c = CallModel::instance().dialingCall(cm);
            c << Call::Action::ACCEPT;
            break;
        default:
            Q_ASSERT(false);
    }

    dispatcher()->dismiss();

    return false;
}

bool Troubleshoot::Absent::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    if (c->direction() == Call::Direction::INCOMING)
        return false;

    auto cself = static_cast<Troubleshoot::Absent*>(self);

    cself->d_ptr->m_State = c->peerContactMethod()->isTracked() && !c->peerContactMethod()->isPresent() ?
        AbsentPrivate::State::OFFLINE : AbsentPrivate::State::NORMAL;

    if (c->lifeCycleState() == Call::LifeCycleState::INITIALIZATION
      && c->state() != Call::State::RINGING && elapsedTime >= 5)
        return true;

    if (!c->account())
        return false;

    const bool isRing   = c->account()->protocol() == Account::Protocol::RING;
    const bool isAbsent = c->lastErrorCode() == (int) std::errc::no_such_device_or_address;

    if (c->state() == Call::State::FAILURE && isAbsent) {
        cself->d_ptr->m_State = AbsentPrivate::State::NO_DEVICE;
        return true;
    }

    return false;
}

void Troubleshoot::Absent::reset()
{
    d_ptr->m_State = AbsentPrivate::State::NORMAL;
    emit textChanged();
}

void Troubleshoot::Absent::activate()
{
    static QStringList options {
        tr("Hang up this call"),
        tr("Try again now"),
    };

    switch (d_ptr->m_State) {
        case AbsentPrivate::State::NORMAL:
            setStringList({});
        case AbsentPrivate::State::NO_DEVICE:
        case AbsentPrivate::State::OFFLINE:
            setStringList(options);
    }

    emit textChanged();
}

void Troubleshoot::Absent::deactivate()
{
    d_ptr->m_State = AbsentPrivate::State::NORMAL;
    emit textChanged();
}

int Troubleshoot::Absent::timeout()
{
    return 10;
}

int Troubleshoot::Absent::autoDismissDelay() const
{
    return 30;
}
