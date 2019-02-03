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
#include <individualtimelinemodel.h>
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
    QSharedPointer<QAbstractItemModel> m_TimelineModel;
    bool m_IsLocked{false};

    QSharedPointer<QAbstractItemModel> m_LinksLens;
    QSharedPointer<QAbstractItemModel> m_BookmarksLens;
    QSharedPointer<QAbstractItemModel> m_FilesLens;

    QList< QTimer* > m_lTimers;

    // Helpers
    void replaceIndividual(Individual* i);
    void replaceContactMethod(ContactMethod* cm);
    void replaceCall(Call* c);

    SharedModelLocker* q_ptr;

    inline Individual*    individual   () const {return m_pIndividual;   }
    inline ContactMethod* contactMethod() const {return m_pContactMethod;}
    inline Call*          call         () const {return m_pCall;         }

private:
    Individual*    m_pIndividual    {nullptr};
    ContactMethod* m_pContactMethod {nullptr};
    Call*          m_pCall          {nullptr};

public Q_SLOTS:
    void slotAvailableLensesChanged();
    void slotCallAdded(Call* c);
    void slotCallRemoved();
};

SharedModelLocker::SharedModelLocker(QObject* parent) :
    QObject(parent), d_ptr(new SharedModelLockerPrivate)
{
    d_ptr->q_ptr = this;
    auto cp = new ActiveCallProxy2(Session::instance()->callModel());
    cp->setSourceModel(Session::instance()->callModel());

    connect(Session::instance()->callModel(), &CallModel::callAttentionRequest,
        this, &SharedModelLocker::showCall);

    // Wait a bit, the timeline will change while it is loading, so before
    // picking something at random, give it a chance to load some things.
    QTimer::singleShot(500, [this]() {
        if (d_ptr->individual())
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
                if (d_ptr->individual()) {
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
    d_ptr->m_CallsModel.clear();
    d_ptr->m_LinksLens.clear();
    d_ptr->m_BookmarksLens.clear();
    d_ptr->m_FilesLens.clear();

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

    if (ind && d_ptr->contactMethod() && d_ptr->contactMethod()->individual()->d() != ind->d())
        d_ptr->replaceContactMethod(nullptr);

    d_ptr->m_TimelineModel = ind->timelineModel();

    if (d_ptr->call() && d_ptr->call()->peer()->d() != d_ptr->individual()->d())
        d_ptr->replaceCall(nullptr);

    d_ptr->replaceIndividual(ind);

    d_ptr->m_IsLocked = false;
    emit changed();
}

void SharedModelLocker::setContactMethod(ContactMethod* cm)
{
    if ((!cm) || cm == d_ptr->contactMethod())
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

    if (d_ptr->individual() && d_ptr->individual() == cm->individual()) {
        emit changed();
        return;
    }

    if (cm && d_ptr->call() && d_ptr->call()->peerContactMethod()->d() != cm->d())
        d_ptr->replaceCall(nullptr);

    d_ptr->m_TimelineModel = cm->individual()->timelineModel();

    d_ptr->replaceIndividual(cm->individual());
    d_ptr->replaceContactMethod(cm);

    emit changed();
}

void SharedModelLocker::setPerson(Person* p)
{
    auto ind = p->individual();
    d_ptr->replaceIndividual(ind);
    d_ptr->replaceContactMethod(nullptr);

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

void SharedModelLocker::showCall(Call* c)
{
    // Ignore clicks on dialing calls
    if ((!c) || c->state() == Call::State::NEW || c->state() == Call::State::DIALING)
        return;

    setCall(c);

    emit changed();
}

QModelIndex SharedModelLocker::suggestedTimelineIndex() const
{
    if (d_ptr->individual())
        return Session::instance()->peersTimelineModel()->individualIndex(
            qobject_cast<Individual*>(d_ptr->individual())
        );

    return {};
}

Individual* SharedModelLocker::currentIndividual() const
{
    return d_ptr->individual();
}

QAbstractItemModel* SharedModelLocker::timelineModel() const
{
    return d_ptr->m_IsLocked ? nullptr : d_ptr->m_TimelineModel.data();
}

QAbstractItemModel* SharedModelLocker::unsortedListView() const
{
    if (d_ptr->individual() && !d_ptr->m_CallsModel) {
        d_ptr->m_CallsModel = d_ptr->individual()->eventAggregate()
            ->unsortedListView();

        Q_ASSERT(d_ptr->m_CallsModel);
    }

    return d_ptr->m_IsLocked ? nullptr : d_ptr->m_CallsModel.data();
}

Person* SharedModelLocker::currentPerson() const
{
    return d_ptr->individual() ? d_ptr->individual()->person() : nullptr;
}

ContactMethod* SharedModelLocker::currentContactMethod() const
{
    return nullptr;
}

void SharedModelLockerPrivate::slotAvailableLensesChanged()
{
    emit q_ptr->changed();
}

bool SharedModelLocker::hasLinks() const
{
    if (!d_ptr->m_TimelineModel)
        return false;

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    return rm->lens(IndividualTimelineModel::LensType::LINKS);
}

bool SharedModelLocker::hasBookmark() const
{    if (!d_ptr->m_TimelineModel)
        return false;

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    return rm->lens(IndividualTimelineModel::LensType::BOOKMARKS);
}

bool SharedModelLocker::hasFiles() const
{    if (!d_ptr->m_TimelineModel)
        return false;

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    return rm->lens(IndividualTimelineModel::LensType::FILES);
}

QAbstractItemModel* SharedModelLocker::linksLens() const
{
    if (!d_ptr->individual())
        return nullptr;

    if (!d_ptr->m_TimelineModel)
        d_ptr->m_TimelineModel = d_ptr->individual()->timelineModel();

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    if (!d_ptr->m_LinksLens)
        d_ptr->m_LinksLens = rm->lens(IndividualTimelineModel::LensType::LINKS);

    return d_ptr->m_LinksLens.data();
}

QAbstractItemModel* SharedModelLocker::bookmarksLens() const
{
    if (!d_ptr->individual())
        return nullptr;

    if (!d_ptr->m_TimelineModel)
        d_ptr->m_TimelineModel = d_ptr->individual()->timelineModel();

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    if (!d_ptr->m_BookmarksLens)
        d_ptr->m_BookmarksLens = rm->lens(IndividualTimelineModel::LensType::BOOKMARKS);

    return d_ptr->m_BookmarksLens.data();
}

QAbstractItemModel* SharedModelLocker::filesLens() const
{
    if (!d_ptr->individual())
        return nullptr;

    if (!d_ptr->m_TimelineModel)
        d_ptr->m_TimelineModel = d_ptr->individual()->timelineModel();

    QSharedPointer<QAbstractItemModel> sp = d_ptr->m_TimelineModel;
    const auto rm = qobject_cast<IndividualTimelineModel*>(sp.data());

    if (!d_ptr->m_FilesLens)
        d_ptr->m_FilesLens = rm->lens(IndividualTimelineModel::LensType::FILES);

    return d_ptr->m_FilesLens.data();
}

Call* SharedModelLocker::call() const
{
    return d_ptr->call();
}

void SharedModelLocker::setCall(Call* c)
{
    d_ptr->replaceCall(c);

    if (c && ((!d_ptr->individual()) || c->peer()->d() != d_ptr->individual()->d()))
        setIndividual(c->peer());

    setContactMethod(c->peerContactMethod());

    emit changed();
}

void SharedModelLockerPrivate::replaceCall(Call* c)
{
    m_pCall = c;

    if (!c) {
        emit q_ptr->callChanged();
        return;
    }

    if (c && ((!m_pIndividual) || c->peer()->d() != m_pIndividual->d()))
        replaceContactMethod(c->peerContactMethod());
    else if (!m_pContactMethod)
        replaceContactMethod(c->peerContactMethod());

    m_pCall = c;

    emit q_ptr->callChanged();
}

void SharedModelLockerPrivate::replaceIndividual(Individual* i)
{
    if (m_pIndividual && m_pIndividual->d() == i)
        return;

    if (m_pIndividual) {
        disconnect(m_pIndividual, &Individual::callAdded,
            this, &SharedModelLockerPrivate::slotCallAdded);
    }

    m_pIndividual = i;

    if (!i) {
        emit q_ptr->individualChanged();
        return;
    }

    connect(m_pIndividual, &Individual::callAdded,
        this, &SharedModelLockerPrivate::slotCallAdded);

    if (auto c = i->firstActiveCall())
        replaceCall(c);

    emit q_ptr->individualChanged();
}

void SharedModelLockerPrivate::replaceContactMethod(ContactMethod* cm)
{
    if (m_pContactMethod && cm && m_pContactMethod->d() == cm->d())
        return;

    m_pContactMethod = cm;

    emit q_ptr->contactMethodChanged();
}

void SharedModelLockerPrivate::slotCallAdded(Call* c)
{
    connect(c, &Call::stateChanged,
        this, &SharedModelLockerPrivate::slotCallRemoved);

    if (m_pCall && m_pCall->lifeCycleState() == Call::LifeCycleState::PROGRESS)
        return;

    switch(c->lifeCycleState()) {
        case Call::LifeCycleState::CREATION:
        case Call::LifeCycleState::FINISHED:
            return;
        case Call::LifeCycleState::PROGRESS:
        case Call::LifeCycleState::INITIALIZATION:
            replaceCall(c);
    }
}

void SharedModelLockerPrivate::slotCallRemoved()
{
    if (!m_pCall)
        return;

    if (m_pCall->lifeCycleState() == Call::LifeCycleState::FINISHED) {
        replaceCall(nullptr);
        emit q_ptr->changed();
    }
}

#include <sharedmodellocker.moc>
