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
#include "callstate.h"

// Qt
#include <QtCore/QDateTime>

// Ring
#include <call.h>
#include <session.h>
#include <callmodel.h>

// POSIX
#include <errno.h>

namespace Troubleshoot {

class CallStatePrivate
{
public:
    enum class State {
        NORMAL,
        MISSED,
        BUSY,
        REFUSED,
        DO_NOT_DISTURB,
    } m_State {State::NORMAL};

    Call::Direction m_Direction;

    QString m_MissedText;
};

}

Troubleshoot::CallState::CallState(Dispatcher* parent) :
    Troubleshoot::Base(parent), d_ptr(new CallStatePrivate())
{}

Troubleshoot::CallState::~CallState()
{
    delete d_ptr;
}

QString Troubleshoot::CallState::headerText() const
{
    static QString busy = tr("This contact is currently busy and can't puck up the call.");
    static QString refused = tr("This contact refused the call. The DND (\"Do not disturb\") mode may be enabled.");
    static QString dnd = tr("This call was hung up because the \"Do not disturb\" mode is enabled");

    static QStringList options {
        tr("Hang up this call"),
        tr("Try again now"),
    };

    switch(d_ptr->m_State) {
        case CallStatePrivate::State::NORMAL:
            return {};
        case CallStatePrivate::State::MISSED:
            return d_ptr->m_MissedText;
        case CallStatePrivate::State::BUSY:
            return busy;
        case CallStatePrivate::State::REFUSED:
            return refused;
        case CallStatePrivate::State::DO_NOT_DISTURB:
            return dnd;
    }
    return {};
}

Troubleshoot::Base::Severity Troubleshoot::CallState::severity() const
{
    return Base::Severity::ERROR;
}

bool Troubleshoot::CallState::setSelection(const QModelIndex& idx, Call* c)
{
    auto cm = c->peerContactMethod();

    switch(d_ptr->m_State) {
        case CallStatePrivate::State::MISSED:
        case CallStatePrivate::State::DO_NOT_DISTURB:
            switch(d_ptr->m_Direction) {
                case Call::Direction::INCOMING:
                    if (idx.row() == 0) {
                        c << Call::Action::REFUSE;
                        c = Session::instance()->callModel()->dialingCall(cm);
                        c << Call::Action::ACCEPT;
                        return true;
                    }
                    break;
                case Call::Direction::OUTGOING:
                    return false;
            }
            break;
        case CallStatePrivate::State::REFUSED:
            switch(d_ptr->m_Direction) {
                case Call::Direction::INCOMING:
                    return false;
                case Call::Direction::OUTGOING:
                    return false;
            }
            break;
        case CallStatePrivate::State::BUSY:
            //TODO
            return false;
        case CallStatePrivate::State::NORMAL:
            return false;
    }

    return false;
}

void Troubleshoot::CallState::activate()
{
    static const QStringList busyOptions {
        tr("Remind me in 5 minutes"),
        tr("Remind me in 15 minutes"),
        tr("Remind me in 1 hour"),
    };

    static const QStringList missedInOptions {
        tr("Call back now")
    };

    static const QStringList missedOutOptions {
        tr("Try again now"),
        tr("Remind me in 5 minutes"),
        tr("Remind me in 15 minutes"),
        tr("Remind me in 1 hour"),
    };

    switch(d_ptr->m_State) {
        case CallStatePrivate::State::MISSED:
        case CallStatePrivate::State::DO_NOT_DISTURB:
            switch(d_ptr->m_Direction) {
                case Call::Direction::INCOMING:
                    setStringList(missedInOptions);
                    break;
                case Call::Direction::OUTGOING:
                    setStringList(missedOutOptions);
                    break;
            }
            break;
        case CallStatePrivate::State::REFUSED:
            switch(d_ptr->m_Direction) {
                case Call::Direction::INCOMING:
                    setStringList({});
                    break;
                case Call::Direction::OUTGOING:
                    setStringList(busyOptions);
                    break;
            }
            break;
        case CallStatePrivate::State::BUSY:
            setStringList(busyOptions);
            break;
        case CallStatePrivate::State::NORMAL:
            setStringList({});
            break;
    }

    emit textChanged();
}

bool Troubleshoot::CallState::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    Q_UNUSED(elapsedTime)
    if (!c)
        return false;

    auto cself = static_cast<Troubleshoot::CallState*>(self);
    cself->d_ptr->m_MissedText.clear();
    cself->d_ptr->m_Direction = c->direction();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
    switch(cself->d_ptr->m_Direction) {
        case Call::Direction::INCOMING:
            switch(c->state()) {
                case Call::State::OVER:
                    // Don't print a warning when the call is manually refused, it makes
                    // people feel bad about themselves.
                    if (c->isMissed() && c->failureReason() != Call::FailureReason::REFUSED) {
                        cself->d_ptr->m_State = CallStatePrivate::State::MISSED;
                        cself->d_ptr->m_MissedText = tr("<center><b>Missed call</b></center> <br />from: ")
                            + c->formattedName() + "<br /><br />"+c->dateTime().toString();

                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;
        case Call::Direction::OUTGOING:
            switch(c->state()) {
                case Call::State::BUSY:
                    cself->d_ptr->m_State = CallStatePrivate::State::BUSY;
                    return true;
                case Call::State::OVER:
                    if (c->lastErrorCode() == ECONNABORTED && c->stopTimeStamp() == c->startTimeStamp()) {
                        cself->d_ptr->m_State = CallStatePrivate::State::REFUSED;
                        return true;
                    }
                    break;
                default:
                    break;
            }
            break;
    }
#pragma GCC diagnostic pop


    return false;
}

int Troubleshoot::CallState::timeout()
{
    return 10;
}

void Troubleshoot::CallState::reset()
{
    d_ptr->m_State = CallStatePrivate::State::NORMAL;
    d_ptr->m_MissedText.clear();
    emit textChanged();
}

QVariant Troubleshoot::CallState::icon() const
{
    return {};
}

int Troubleshoot::CallState::autoDismissDelay() const
{
    switch(d_ptr->m_State) {
        case CallStatePrivate::State::MISSED:
        case CallStatePrivate::State::DO_NOT_DISTURB:
            return std::numeric_limits<int>::max();
        case CallStatePrivate::State::REFUSED:
        case CallStatePrivate::State::BUSY:
            return 30;
        case CallStatePrivate::State::NORMAL:
            return -1;
    }

    return -1;
}


