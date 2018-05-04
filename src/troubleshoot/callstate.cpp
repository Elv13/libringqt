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
    } m_State {State::NORMAL};

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
    static QString refused = tr("This contact refused the call.");

    switch(d_ptr->m_State) {
        case CallStatePrivate::State::NORMAL:
            return {};
        case CallStatePrivate::State::MISSED:
            return d_ptr->m_MissedText;
        case CallStatePrivate::State::BUSY:
            return busy;
        case CallStatePrivate::State::REFUSED:
            return refused;
    }
    return {};
}

Troubleshoot::Base::Severity Troubleshoot::CallState::severity() const
{
    return Base::Severity::ERROR;
}

bool Troubleshoot::CallState::setSelection(const QModelIndex& idx, Call* c)
{
    return false;
}

bool Troubleshoot::CallState::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    if (!c)
        return false;

    auto cself = static_cast<Troubleshoot::CallState*>(self);
    cself->d_ptr->m_MissedText.clear();

    switch(c->direction()) {
        case Call::Direction::INCOMING:
            switch(c->state()) {
                case Call::State::OVER:
                    if (c->isMissed()) {
                        cself->d_ptr->m_State = CallStatePrivate::State::MISSED;
                        cself->d_ptr->m_MissedText = tr("<center><b>Missed call</b></center> <br />from: ")
                            + c->formattedName() + "<br /><br />"+c->dateTime().toString();

                        return true;
                    }
                default:
                    break;
            }
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
    }

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
    return d_ptr->m_State == CallStatePrivate::State::MISSED ?
        std::numeric_limits<int>::max() : -1;
}


