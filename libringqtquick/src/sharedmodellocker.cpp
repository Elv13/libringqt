/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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
#include "sharedmodellocker.h"

// Qt
#include <QtCore/QMimeData>
#include <QtCore/QTimer>
#include <QtCore/QSortFilterProxyModel>

// LibRingQt
#include <peerstimelinemodel.h>
#include <individualdirectory.h>
#include <contactmethod.h>
#include <libcard/eventaggregate.h>
#include <individual.h>
#include <person.h>
#include <call.h>
#include <callmodel.h>
#include <session.h>

// Remove inactive calls from the CallModel
class ActiveCallProxy2 : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ActiveCallProxy2(QObject* parent) : QSortFilterProxyModel(parent) {}

protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex & srcParent ) const override;
};

class SharedModelLockerPrivate final : public QObject
{
    Q_OBJECT
public:
    QSharedPointer<QAbstractItemModel> m_CallsModel;
    Individual* m_Invididual {nullptr};
    QSharedPointer<QAbstractItemModel> m_TimelineModel;
    bool m_IsLocked{false};

    QList< QTimer* > m_lTimers;

    SharedModelLocker* q_ptr;
};

SharedModelLocker::SharedModelLocker(QObject* parent) :
    QObject(parent), d_ptr(new SharedModelLockerPrivate)
{
    d_ptr->q_ptr = this;
    auto cp = new ActiveCallProxy2(Session::instance()->callModel());
    cp->setSourceModel(Session::instance()->callModel());

    connect(Session::instance()->callModel(), &CallModel::callAttentionRequest,
        this, &SharedModelLocker::showVideo);

    // Wait a bit, the timeline will change while it is loading, so before
    // picking something at random, give it a chance to load some things.
    QTimer::singleShot(500, [this]() {
        if (d_ptr->m_Invididual)
            return;

        auto i = Session::instance()->peersTimelineModel()->mostRecentIndividual();

        // Select whatever is first (if any)
        if ((!i) && Session::instance()->peersTimelineModel()->rowCount())
            if (auto rawI = qvariant_cast<Individual*>(
                Session::instance()->peersTimelineModel()->index(0,0).data((int)Ring::Role::Object)
            ))
                i = rawI;

        // There is nothing yet, wait
        if (!i) {
            QMetaObject::Connection c;

            c = connect(Session::instance()->peersTimelineModel(), &QAbstractItemModel::rowsInserted, this, [c, this]() {
                if (d_ptr->m_Invididual) {
                    disconnect(c);
                    return;
                }

                if (auto i = Session::instance()->peersTimelineModel()->mostRecentIndividual()) {
                    disconnect(c);
                    setIndividual(i);
                    const auto idx = Session::instance()->peersTimelineModel()->individualIndex(i);
                    emit suggestSelection(i, idx);
                }
            });

            return;
        }

        setIndividual(i);

        if (i) {
            const auto idx = Session::instance()->peersTimelineModel()->individualIndex(i);
            emit suggestSelection(i, idx);
        }
    });
}

SharedModelLocker::~SharedModelLocker()
{
    d_ptr->m_TimelineModel.clear();
    d_ptr->m_Invididual = nullptr;
    d_ptr->m_CallsModel.clear();

    // Release the shared pointer reference from the timer.
    for (auto t : qAsConst(d_ptr->m_lTimers)) {
        disconnect(t);
        delete t;
    }

    delete d_ptr;
}

void SharedModelLocker::setIndividual(Individual* ind)
{
    if (!ind)
        return;

    d_ptr->m_IsLocked = true;
    emit changed();

    d_ptr->m_Invididual = ind;
    d_ptr->m_TimelineModel = ind->timelineModel();
    d_ptr->m_CallsModel = ind->eventAggregate()->unsortedListView();
    Q_ASSERT(d_ptr->m_CallsModel);

    d_ptr->m_IsLocked = false;
    emit changed();
}

void SharedModelLocker::setContactMethod(ContactMethod* cm)
{
    if ((!cm))
        return;

    cm = Session::instance()->individualDirectory()->fromTemporary(cm);

    // Keep a reference for 5 minutes to avoid double free from QML
    for (auto ptr : {d_ptr->m_CallsModel, d_ptr->m_TimelineModel}) {
        if (ptr) {
            auto t = new QTimer(this);
            t->setInterval(5 * 60 * 1000);
            t->setSingleShot(true);
            connect(t, &QTimer::timeout, this, [t, this, ptr]() {
                this->d_ptr->m_lTimers.removeAll(t);
            });
            t->start();
            d_ptr->m_lTimers << t;
        }
    }

    // Keep a strong reference because QML won't
    d_ptr->m_Invididual = cm->individual();
    d_ptr->m_CallsModel = cm->individual()->eventAggregate()->unsortedListView();
    Q_ASSERT(d_ptr->m_CallsModel);

    d_ptr->m_TimelineModel = cm->individual()->timelineModel();
    emit changed();
}

void SharedModelLocker::setPerson(Person* p)
{
    auto ind = p->individual();
    d_ptr->m_Invididual = ind;

    d_ptr->m_CallsModel = ind->eventAggregate()->unsortedListView();
    Q_ASSERT(d_ptr->m_CallsModel);

    emit changed();
}

bool ActiveCallProxy2::filterAcceptsRow(int row, const QModelIndex& srcParent ) const
{
    // Conferences are always active
    if (srcParent.isValid())
        return true;

    return sourceModel()->index(row, 0)
        .data((int)Call::Role::LifeCycleState) == QVariant::fromValue(Call::LifeCycleState::PROGRESS);
}

void SharedModelLocker::showVideo(Call* c)
{
    // Ignore clicks on dialing calls
    if ((!c) || c->state() == Call::State::NEW || c->state() == Call::State::DIALING)
        return;

    setContactMethod(c->peerContactMethod());
}

QModelIndex SharedModelLocker::suggestedTimelineIndex() const
{
    if (d_ptr->m_Invididual)
        return Session::instance()->peersTimelineModel()->individualIndex(
            qobject_cast<Individual*>(d_ptr->m_Invididual)
        );

    return {};
}

Individual* SharedModelLocker::currentIndividual() const
{
    return d_ptr->m_Invididual;
}

QAbstractItemModel* SharedModelLocker::timelineModel() const
{
    return d_ptr->m_IsLocked ? nullptr : d_ptr->m_TimelineModel.data();
}

QAbstractItemModel* SharedModelLocker::unsortedListView() const
{
    return d_ptr->m_IsLocked ? nullptr : d_ptr->m_CallsModel.data();
}

Person* SharedModelLocker::currentPerson() const
{
    return d_ptr->m_Invididual ? d_ptr->m_Invididual->person() : nullptr;
}

ContactMethod* SharedModelLocker::currentContactMethod() const
{
    Q_ASSERT(false); //TODO
    return nullptr;
}

#include <sharedmodellocker.moc>
