/****************************************************************************
 *   Copyright (C) 2018 by Bluesystems                                      *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                    *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "availabilitytracker.h"

// ring
#include "contactmethod.h"
#include "accountmodel.h"
#include "individual.h"
#include "video/renderer.h"
#include "video/sourcemodel.h"
#include "call.h"
#include "callmodel.h"
#include "libcard/matrixutils.h"

// libstdc++
#include <algorithm>

namespace Media {

class AvailabilityTrackerPrivate : public QObject
{
    Q_OBJECT
public:
    enum class CallMode {
        NONE,
        AUDIO,
        VIDEO,
        SCREEN,
    };

    Individual* m_pIndividual {nullptr};

    // Helpers
    ContactMethod::MediaAvailailityStatus getFirstIssue() const;
    CallMode getCallMode() const;

    static const Matrix1D<ContactMethod::MediaAvailailityStatus ,QString> m_mCMMASNames;

    AvailabilityTracker* q_ptr;
public Q_SLOTS:
    void slotCanCallChanged();
    void slotRegistrationChanged();
    void slotCanVideoCallChanged();
    void slotIndividualChanged();
};

const Matrix1D<ContactMethod::MediaAvailailityStatus ,QString> AvailabilityTrackerPrivate::m_mCMMASNames = {{
    { ContactMethod::MediaAvailailityStatus::NO_CALL     , QObject::tr("Sending text messages can only happen during an audio call in SIP accounts")},
    { ContactMethod::MediaAvailailityStatus::UNSUPPORTED , QObject::tr("This account doesn't support all media")},
    { ContactMethod::MediaAvailailityStatus::SETTINGS    , QObject::tr("Video isn't available because it's disabled for this account")},
    { ContactMethod::MediaAvailailityStatus::NO_ACCOUNT  , QObject::tr("There is no account capable of reaching this person")},
    { ContactMethod::MediaAvailailityStatus::CODECS      , QObject::tr("All video codecs are disabled, video call isn't possible")},
    { ContactMethod::MediaAvailailityStatus::ACCOUNT_DOWN, QObject::tr("All accounts capable of reaching this person are currently unavailable")},
    { ContactMethod::MediaAvailailityStatus::NETWORK     , QObject::tr("Ring-KDE is experiencing a network issue, please try later")},
    { ContactMethod::MediaAvailailityStatus::AVAILABLE   , QObject::tr("")}
}};

AvailabilityTracker::AvailabilityTracker(QObject* parent) : QObject(parent),
    d_ptr(new AvailabilityTrackerPrivate())
{
    d_ptr->q_ptr = this;
    connect(&AccountModel::instance(), &AccountModel::canCallChanged, d_ptr, &AvailabilityTrackerPrivate::slotCanCallChanged);
    connect(&AccountModel::instance(), &AccountModel::registrationChanged, d_ptr, &AvailabilityTrackerPrivate::slotRegistrationChanged);
    connect(&AccountModel::instance(), &AccountModel::canVideoCallChanged, d_ptr, &AvailabilityTrackerPrivate::slotCanVideoCallChanged);

    connect(&CallModel::instance(), &CallModel::callStateChanged , d_ptr, &AvailabilityTrackerPrivate::slotCanCallChanged);
    connect(&CallModel::instance(), &CallModel::rendererAdded    , d_ptr, &AvailabilityTrackerPrivate::slotCanCallChanged);
    connect(&CallModel::instance(), &CallModel::rendererRemoved  , d_ptr, &AvailabilityTrackerPrivate::slotCanCallChanged);
}

AvailabilityTracker::~AvailabilityTracker()
{
    delete d_ptr;
}

bool AvailabilityTracker::canCall() const
{
    const Individual* i = d_ptr->m_pIndividual;
    return i ? i->canCall() : false;
}

bool AvailabilityTracker::canVideoCall() const
{
    const Individual* i = d_ptr->m_pIndividual;
    return i ? i->canVideoCall() : false;
}

bool AvailabilityTracker::canSendTexts() const
{
    const Individual* i = d_ptr->m_pIndividual;
    return i ? i->canSendTexts() : false;
}

bool AvailabilityTracker::hasWarning() const
{
    const Individual* i = d_ptr->m_pIndividual;
    return !(
        (i) && canCall() && canVideoCall() && canSendTexts() && !i->isOffline()
    );
}

ContactMethod::MediaAvailailityStatus AvailabilityTrackerPrivate::getFirstIssue() const
{
    if (!m_pIndividual)
        return ContactMethod::MediaAvailailityStatus::AVAILABLE;

    auto ret = ContactMethod::MediaAvailailityStatus::AVAILABLE;

    const Individual* i = m_pIndividual;

//     bool hasFineCM = false;

    // The order of the issue type is related to the severity
    i->forAllNumbers([&ret/*, &hasFineCM*/](ContactMethod* cm) {
        auto textsIssue = cm->canSendTexts();
        auto videoIssue = cm->canVideoCall();
        auto audioIssue = cm->canCall();

        ret = (ContactMethod::MediaAvailailityStatus) std::max(
            (int)textsIssue, std::max( (int)videoIssue, (int)audioIssue)
        );

//         if (newRet != ContactMethod::MediaAvailailityStatus::AVAILABLE)
//             hasFineCM = true;

        Q_ASSERT(ret != ContactMethod::MediaAvailailityStatus::COUNT__);
    }, false);

    return ret;
}

AvailabilityTrackerPrivate::CallMode AvailabilityTrackerPrivate::getCallMode() const
{
    if (!m_pIndividual)
        return CallMode::NONE;

    const auto c = CallModel::instance().firstActiveCall(m_pIndividual);

    if (!c)
        return CallMode::NONE;

    if (c->lifeCycleState() != Call::LifeCycleState::PROGRESS
      && c->lifeCycleState() != Call::LifeCycleState::INITIALIZATION)
        return CallMode::NONE;

    const auto r = c->videoRenderer();

    if (!r)
        return CallMode::AUDIO;

    const auto sm = c->sourceModel();

    if (!r)
        return CallMode::AUDIO;

    return sm->matches(::Video::SourceModel::ExtendedDeviceList::SCREEN) ?
        CallMode::SCREEN : CallMode::VIDEO;
}

QString AvailabilityTracker::warningMessage() const
{
    if (!hasWarning())
        return {};

    const auto ma = d_ptr->getFirstIssue();

    const Individual* i = d_ptr->m_pIndividual;

    if (!i)
        return tr("No contact has been selected");

    if (ma == ContactMethod::MediaAvailailityStatus::AVAILABLE && i->isOffline())
        return tr("This contact doesn't appears to be online, stealth mode may be enabled.");

    return d_ptr->m_mCMMASNames[d_ptr->getFirstIssue()];
}

Individual* AvailabilityTracker::individual() const
{
    return d_ptr->m_pIndividual;
}

void AvailabilityTracker::setIndividual(Individual* ind)
{
    if (d_ptr->m_pIndividual) {
        const Individual* i = d_ptr->m_pIndividual;
        disconnect(i, &Individual::mediaAvailabilityChanged,
            d_ptr, &AvailabilityTrackerPrivate::slotIndividualChanged);
    }

    d_ptr->m_pIndividual = ind;

    if (ind)
        connect(ind, &Individual::mediaAvailabilityChanged,
            d_ptr, &AvailabilityTrackerPrivate::slotIndividualChanged);

    d_ptr->slotIndividualChanged();
}

void AvailabilityTrackerPrivate::slotCanCallChanged()
{
    emit q_ptr->changed();
}

void AvailabilityTrackerPrivate::slotRegistrationChanged()
{
    emit q_ptr->changed();
}

void AvailabilityTrackerPrivate::slotCanVideoCallChanged()
{
    emit q_ptr->changed();
}

void AvailabilityTrackerPrivate::slotIndividualChanged()
{
    emit q_ptr->changed();
}


AvailabilityTracker::ControlState AvailabilityTracker::audioCallControlState() const
{
    typedef AvailabilityTrackerPrivate::CallMode CallMode;

    if (!canCall())
        return ControlState::HIDDEN;

    switch (d_ptr->getCallMode()) {
        case CallMode::AUDIO:
            return ControlState::CHECKED;
        case CallMode::VIDEO:
        case CallMode::SCREEN:
        case CallMode::NONE:
            return ControlState::NORMAL;
    }

    return ControlState::NORMAL;
}

AvailabilityTracker::ControlState AvailabilityTracker::videoCallControlState() const
{
    typedef AvailabilityTrackerPrivate::CallMode CallMode;

    if (!canVideoCall())
        return ControlState::HIDDEN;

    switch (d_ptr->getCallMode()) {
        case CallMode::VIDEO:
            return ControlState::CHECKED;
        case CallMode::AUDIO:
        case CallMode::SCREEN:
        case CallMode::NONE:
            return ControlState::NORMAL;
    }

    return ControlState::NORMAL;
}

AvailabilityTracker::ControlState AvailabilityTracker::screenSharingControlState() const
{
    typedef AvailabilityTrackerPrivate::CallMode CallMode;

    if (!canVideoCall())
        return ControlState::HIDDEN;

    switch (d_ptr->getCallMode()) {
        case CallMode::SCREEN:
            return ControlState::CHECKED;
        case CallMode::AUDIO:
        case CallMode::VIDEO:
        case CallMode::NONE:
            return ControlState::NORMAL;
    }

    return ControlState::NORMAL;
}

AvailabilityTracker::ControlState AvailabilityTracker::textMessagesControlState() const
{
    if (!canSendTexts())
        return ControlState::HIDDEN;

    return ControlState::NORMAL;
}


AvailabilityTracker::ControlState AvailabilityTracker::hangUpControlState() const
{
    return CallModel::instance().firstActiveCall(d_ptr->m_pIndividual) ?
        ControlState::NORMAL : ControlState::HIDDEN;
}


}

#include <availabilitytracker.moc>
