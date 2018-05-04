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
#include "dispatcher.h"

// Qt
#include <QtCore/QTimer>

// Ring
#include <call.h>

// Troubleshoot
#include "absent.h"
#include "handshake.h"
#include "videostuck.h"
#include "generic.h"
#include "callstate.h"

namespace Troubleshoot {


class DispatcherPrivate : public QObject
{
    Q_OBJECT
public:

    struct Holder {
        std::function<bool(Call*, time_t t)> m_fProbe;
        Base*   m_pInstance {nullptr};
        QTimer* m_pTimer    {nullptr};
        Holder* m_pNext     {nullptr};
        time_t  m_Timeout   {   0   };
        int     m_Index     {  -1   };
    };

    Holder* m_pFirstHolder   {nullptr};
    Holder* m_pLastHolder    {nullptr};
    Holder* m_pCurrentHolder {nullptr};

    Call* m_pCall {nullptr};

    QTimer* m_pAutoDismiss {new QTimer(this)};

    Dispatcher* q_ptr;

    template<class T> void registerAdapter();

    static int m_Count;

    // Helpers
    void setCurrentHolder(Holder* h);

public Q_SLOTS:
    void slotStateChanged(Call::State newState, Call::State previousState);
    void slotTimeout();
};

int DispatcherPrivate::m_Count = 0;

}

Troubleshoot::Dispatcher::Dispatcher(QObject* parent) : QIdentityProxyModel(parent),
    d_ptr(new DispatcherPrivate())
{
    d_ptr->q_ptr = this;

    connect(d_ptr->m_pAutoDismiss, &QTimer::timeout, this, &Dispatcher::dismiss);

    // The order *is* important. The first has the highest priority
    d_ptr->registerAdapter<Troubleshoot::VideoStuck>();
    d_ptr->registerAdapter<Troubleshoot::Handshake> ();
    d_ptr->registerAdapter<Troubleshoot::Absent>    ();
    d_ptr->registerAdapter<Troubleshoot::CallState> ();
    d_ptr->registerAdapter<Troubleshoot::Generic>   ();
}

Troubleshoot::Dispatcher::~Dispatcher()
{
    delete d_ptr;
}

bool Troubleshoot::Dispatcher::isActive() const
{
    return d_ptr->m_pCurrentHolder;
}

QString Troubleshoot::Dispatcher::headerText() const
{
    return d_ptr->m_pCurrentHolder ?
        d_ptr->m_pCurrentHolder->m_pInstance->headerText() : QString();
}

Troubleshoot::Base::Severity Troubleshoot::Dispatcher::severity() const
{
    return d_ptr->m_pCurrentHolder ?
        d_ptr->m_pCurrentHolder->m_pInstance->severity() : Base::Severity::NONE ;
}

Troubleshoot::Base* Troubleshoot::Dispatcher::currentModule() const
{
    return d_ptr->m_pCurrentHolder ? d_ptr->m_pCurrentHolder->m_pInstance : nullptr;
}

Call* Troubleshoot::Dispatcher::call() const
{
    return d_ptr->m_pCall;
}

void Troubleshoot::DispatcherPrivate::setCurrentHolder(Holder* h)
{
    if (h == m_pCurrentHolder)
        return;

    if (m_pAutoDismiss->isActive())
        m_pAutoDismiss->stop();

    if (m_pCurrentHolder) {
        m_pCurrentHolder->m_pInstance->deactivate();
        m_pCurrentHolder->m_pInstance->reset();
    }

    m_pCurrentHolder = h;

    if (m_pCurrentHolder) {
        m_pCurrentHolder->m_pInstance->activate();
        const int autoDismiss = m_pCurrentHolder->m_pInstance->autoDismissDelay();

        if (autoDismiss != -1) {
            m_pAutoDismiss->setInterval(autoDismiss * 1000);
            m_pAutoDismiss->start();
        }
    }

    q_ptr->setSourceModel(h ? h->m_pInstance : nullptr);

    emit q_ptr->activechanged();
    emit q_ptr->textChanged();

}

void Troubleshoot::Dispatcher::setCall(Call* call)
{
    if (call == d_ptr->m_pCall)
        return;

    // It will be dismissed later
    if ((!call) && d_ptr->m_pCurrentHolder && d_ptr->m_pAutoDismiss->isActive())
        return;

    if (d_ptr->m_pCall)
        disconnect(d_ptr->m_pCall, &Call::stateChanged,
            d_ptr, &DispatcherPrivate::slotStateChanged);

    d_ptr->m_pCall = call;

    if (d_ptr->m_pCall) {
        connect(d_ptr->m_pCall, &Call::stateChanged,
            d_ptr, &DispatcherPrivate::slotStateChanged);

        for (auto h = d_ptr->m_pFirstHolder; h; h = h->m_pNext)
            h->m_pTimer->start();
    }
    else for (auto h = d_ptr->m_pFirstHolder; h; h = h->m_pNext)
        h->m_pTimer->stop();

    d_ptr->setCurrentHolder(nullptr);
}

void Troubleshoot::DispatcherPrivate::slotStateChanged(Call::State newState, Call::State previousState)
{
    Q_UNUSED(newState)
    Q_UNUSED(previousState)

    if (!m_pCall)
        return;

    //HACK The signal is sent quite early and many pieces of code are either
    // connected to the signal or executed later in the Call internal state
    // machine. These changes affect many error handling corner cases so they
    // have to be executed first rather than at a random point after this
    // class changes them.
    QTimer::singleShot(0, [this]() {
        if (!m_pCall)
            return;

        Holder* affected = nullptr;

        for (auto h = m_pFirstHolder; h; h = h->m_pNext) {
            h->m_pTimer->stop();
            h->m_pTimer->start();
            affected = affected ? affected : h->m_fProbe(m_pCall, 0) ? h : nullptr;
        }

        setCurrentHolder(affected);
    });
}

void Troubleshoot::DispatcherPrivate::slotTimeout()
{
    auto t = qobject_cast<QTimer*>(sender());
    Holder* holder = nullptr;

    for (auto h = m_pFirstHolder; h; h = h->m_pNext) {
        if (h->m_pTimer == t) {
            holder = h;
            break;
        }
    }

    Q_ASSERT(holder);
    t->stop();

    if (m_pCurrentHolder && holder->m_Index <= m_pCurrentHolder->m_Index)
        return;

    if (!holder->m_fProbe(m_pCall, holder->m_Timeout))
        return;

    setCurrentHolder(holder);
}

template<class T>
void Troubleshoot::DispatcherPrivate::registerAdapter()
{
    auto i = new T(q_ptr);

    auto h = new Holder {
        [i](Call* c, time_t t) -> bool { return T::isAffected(c, t, i); },
        i,
        new QTimer( q_ptr ),
        nullptr            ,
        T::timeout(       ),
        m_Count++
    };

    if (auto t = T::timeout()) {
        connect(h->m_pTimer, &QTimer::timeout, this, &DispatcherPrivate::slotTimeout);
        h->m_pTimer->setInterval(t * 1000);
    }

    m_pFirstHolder = m_pFirstHolder ? m_pFirstHolder : h;

    if (m_pLastHolder)
        m_pLastHolder->m_pNext = h;

    m_pLastHolder = h;
}

bool Troubleshoot::Dispatcher::setSelection(const QModelIndex& idx)
{
    if ((!d_ptr->m_pCurrentHolder) || !idx.isValid())
        return false;

    return d_ptr->m_pCurrentHolder->m_pInstance->setSelection(idx, call());
}

bool Troubleshoot::Dispatcher::setSelection(int idx)
{
    return setSelection(index(idx, 0));
}

void Troubleshoot::Dispatcher::dismiss()
{
    d_ptr->setCurrentHolder(nullptr);
}

#include <dispatcher.moc>
