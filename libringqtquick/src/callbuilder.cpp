/***************************************************************************
 *   Copyright (C) 2019 by Blue Systems                                    *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "callbuilder.h"

#include <callmodel.h>
#include <session.h>
#include <call.h>
#include <individual.h>
#include <contactmethod.h>

class CallBuilderPrivate final : public QObject
{
    Q_OBJECT
public:
    Individual*    m_pIndividual      {nullptr};
    ContactMethod* m_pContactMethod   {nullptr};
    Call*          m_pActiveCall      {nullptr};
    bool           m_HasAudio         { true  };
    bool           m_HasVideo         { false };
    bool           m_HasScreenSharing { false };

    CallBuilder* q_ptr;
public Q_SLOTS:
    void slotCallChanged(Call* c);
};

CallBuilder::CallBuilder(QObject* parent) : QObject(parent), d_ptr(new CallBuilderPrivate())
{
    d_ptr->q_ptr = this;

    connect(Session::instance()->callModel(), &CallModel::callStateChanged,
        d_ptr, &CallBuilderPrivate::slotCallChanged);
}

CallBuilder::~CallBuilder()
{
    delete d_ptr;
}

Individual* CallBuilder::individual() const
{
    return d_ptr->m_pIndividual;
}

void CallBuilder::setIndividual(Individual* i)
{
    if (i == d_ptr->m_pIndividual)
        return;

    d_ptr->m_pIndividual = i;
    d_ptr->m_pContactMethod = nullptr;

    emit changed();
}

ContactMethod* CallBuilder::contactMethod() const
{
    return d_ptr->m_pContactMethod;
}

void CallBuilder::setContactMethod(ContactMethod* cm)
{
    if (cm == d_ptr->m_pContactMethod)
        return;

    d_ptr->m_pContactMethod = cm;

    if (cm && d_ptr->m_pIndividual != cm->individual())
        setIndividual(cm->individual());

    emit changed();
}

bool CallBuilder::hasAudio() const
{
    return d_ptr->m_HasAudio;
}

void CallBuilder::setAudio(bool v)
{
    d_ptr->m_HasAudio = v;
    emit changed();
}

bool CallBuilder::hasVideo() const
{
    return d_ptr->m_HasVideo;
}

void CallBuilder::setVideo(bool v)
{
    d_ptr->m_HasVideo = v;
    emit changed();
}

bool CallBuilder::hasScreenSharing() const
{
    return d_ptr->m_HasScreenSharing;
}

void CallBuilder::setScreenSharing(bool v)
{
    d_ptr->m_HasScreenSharing = v;
    emit changed();
}

Call* CallBuilder::activeCall() const
{
    return nullptr;//TODO
}

bool CallBuilder::isChoiceRequired() const
{
    return (!d_ptr->m_pContactMethod) && d_ptr->m_pIndividual &&
        d_ptr->m_pIndividual->requireUserSelection();
}

void CallBuilder::reset()
{
    d_ptr->m_pContactMethod = nullptr;
    emit changed();
}

Call* CallBuilder::update()
{
    //TODO
    return activeCall();
}

Call* CallBuilder::commit()
{
    if (activeCall())
        return update();

    if (!individual()) {
        qWarning() << "Trying to build a call without specifying who to call";
        return nullptr;
    }

    auto cm = contactMethod();

    /**
     * This case is easy, there is no ambiguity
     */
    if (!cm)
        cm = individual()->mainContactMethod();

    /**
     * This is an acceptable automatic fallback. Often the correct choice can
     * be deduced with near perfect accuracy.
     */
    if (!cm) {
        qDebug() << "Calling" << individual()
            << "is ambiguous, please set the ContactMethod";

        cm = individual()->preferredContactMethod(hasVideo() ?
            Media::Media::Type::VIDEO : Media::Media::Type::AUDIO
        );
    }

    /**
     * If there is no contact method, it is not going to work
     */
    if (!cm) {
        qWarning() << "Failed to build a call using" << individual()
            << ", no contact methods were found";
        return nullptr;
    }

    auto c = Session::instance()->callModel()->dialingCall(cm);

    if ((!d_ptr->m_HasVideo) && (!d_ptr->m_HasScreenSharing))
        c->removeMedia(Media::Media::Type::VIDEO);

    //TODO this does nothing
    if (!d_ptr->m_HasAudio)
        c->removeMedia(Media::Media::Type::AUDIO);

    c->performAction(Call::Action::ACCEPT);

    return c;
}


void CallBuilderPrivate::slotCallChanged(Call* c)
{
    emit q_ptr->changed();
}

#include <callbuilder.moc>
