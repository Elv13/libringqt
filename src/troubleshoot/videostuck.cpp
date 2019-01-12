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

// Qt
#include <QtCore/QTimer>

// Ring
#include <call.h>
#include <session.h>
#include <callmodel.h>
#include <video/renderer.h>
#include <media/video.h>

namespace Troubleshoot {

class VideoStuckPrivate
{
public:
    enum class Mitigations {
        HANG_UP,
        MUTE_UNMUTE,
        HOLD,
        HOLD_UNHOLD,
        CALL_AGAIN
    };
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
    static QString message = tr("The video seems to have stopped. It may resume at any time, but the following options may speed up recovery:");

    return message;
}

Troubleshoot::Base::Severity Troubleshoot::VideoStuck::severity() const
{
    return Base::Severity::WARNING;
}

bool Troubleshoot::VideoStuck::setSelection(const QModelIndex& idx, Call* c)
{
    if ((!c) || !idx.isValid())
        return false;

    auto cm = c->peerContactMethod();

    switch((VideoStuckPrivate::Mitigations) idx.row()) {
        case VideoStuckPrivate::Mitigations::HANG_UP:
            c << Call::Action::REFUSE;
            break;
        case VideoStuckPrivate::Mitigations::MUTE_UNMUTE:
            // This mutes the **LOCAL** media, but the idea is that it will cause SIP to wake up anyway
            if (auto videoOut = c->firstMedia<Media::Video>(Media::Media::Direction::OUT))
                if (videoOut->mute())
                    QTimer::singleShot(1000, [c]() {
                        if (c->lifeCycleState() == Call::LifeCycleState::PROGRESS) {
                            if (auto videoOut = c->firstMedia<Media::Video>(Media::Media::Direction::OUT))
                                videoOut->unmute();
                        }
                    });

            break;
        case VideoStuckPrivate::Mitigations::HOLD:
            c << Call::Action::HOLD;
            break;
        case VideoStuckPrivate::Mitigations::HOLD_UNHOLD:
            c << Call::Action::HOLD;
            QTimer::singleShot(1000, [c]() {
                if (c->state() == Call::State::HOLD)
                    c << Call::Action::HOLD;
            });
            break;
        case VideoStuckPrivate::Mitigations::CALL_AGAIN:
            c << Call::Action::REFUSE;
            c = Session::instance()->callModel()->dialingCall(cm);
            c << Call::Action::ACCEPT;
            break;
    }

    return true;
}

bool Troubleshoot::VideoStuck::isAffected(Call* c, time_t elapsedTime, Troubleshoot::Base* self)
{
    Q_UNUSED(elapsedTime)
    Q_UNUSED(self)
    return c->state() == Call::State::CURRENT && c->videoRenderer() &&(
        c->liveMediaIssues() & Call::LiveMediaIssues::VIDEO_ACQUISITION_FAILED
    ) && c->videoRenderer()->hasAcquired();
}

int Troubleshoot::VideoStuck::timeout()
{
    return 5;
}

void Troubleshoot::VideoStuck::activate()
{
    static QStringList options {
        tr( "Hang up this call"               ),
        tr( "Try to mute and unmute video"    ),
        tr( "Put the call on hold"            ),
        tr( "Renegotiate video (may hang up)" ),
        tr( "Hang up and call again"          ),
    };

    setStringList(options);

    emit textChanged();
}

void Troubleshoot::VideoStuck::deactivate()
{
    setStringList({});
}
