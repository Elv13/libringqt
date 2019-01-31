/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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
#include "itembase.h"

#include <QtCore/QTimer>

 /**
  * All object need AT ALL TIME a parent or QtQuick GC will reap them.
  *
  * The parent has to be in the same thread or QtQuick GC will still reap them
  * because reparenting failed.
  *
  * Make sure they are force-reparented when the thread is destroyed or
  * qDeleteAll will reap them.
  *
  * What else have failed:
  *
  * * Adding a dumb QTimer::singleShot: Qt will reap the objects if the thread
  *   hangs on some slot I/O
  *
  * * Not setting a parent: GC will reap it
  *
  * * Adding a QList and batching the transfer: GC will reap the objects if the
  *   queue has not been dealt with yet
  *
  * * Having a global object per thread and assigning it as parent while waiting
  *   to transfer them: The transfer will still have a window of opportunity
  *   when the setParent has to be reset before calling moveToThread and the
  *   GC will reap them.
  *
  * The idea here is that the `setParent` is always called in the same thread
  * and wont ever get corrupted. Reparenting can happen later on in a queued
  * slot.
  *
  */
class TemporaryParentHolder
{
public:
    QHash<QThread*, QObject*> m_hHolder;
    void hold(QObject* o, QObject* p);
    static TemporaryParentHolder* instance();
};

TemporaryParentHolder* TemporaryParentHolder::instance()
{
    static TemporaryParentHolder h;
    return &h;
}

void TemporaryParentHolder::hold(QObject* o, QObject* p)
{
    Q_ASSERT(p && o);
    static QMutex m;
    QMutexLocker l(&m);

    if (!m_hHolder.contains(p->thread())) {
        auto n = new QObject();
        n->moveToThread(p->thread());
        n->setParent(p);
        m_hHolder[p->thread()] = p;
    }

    Q_ASSERT(m_hHolder[p->thread()]->thread() == p->thread());
    o->moveToThread(QCoreApplication::instance()->thread());
    o->setParent(m_hHolder[p->thread()]);

    auto t = new QTimer();
    t->setInterval(0);

    // Call setParent from the correct thread
    QObject::connect(t, &QTimer::timeout, p, [t, o, p]() {
        o->setParent(p);
        t->stop();
        t->deleteLater();
    }, Qt::QueuedConnection);
}

ItemBase::ItemBase(QObject* parent) :QObject(nullptr), d_ptr(new ItemBasePrivate())
{
    // If the parent isn't in the main thread, it will eventually crash, it isn't
    // safe and isn't supported.
    Q_ASSERT((!parent) || parent->thread() == QCoreApplication::instance()->thread());

    // Otherwise QML will claim and destroy them
    //Q_ASSERT(parent);

    // Because after moveToThread, the code is still running in the old thread,
    // setting the parent may be done in multiple threads in parallel on the
    // same parent. In that case the parent object children list is potentially
    // being resized in *another thread context*. This avoids causing such issue.
    if (parent->thread() != thread()) {
        // Set a temporary parent to be certain that if a thread event loop
        // iteration takes a long time, there is no window of opportunity for
        // QtQuick to claim and free the object.
        TemporaryParentHolder::instance()->hold(this, parent);
    }
    else
        QObject::setParent(parent);
}

ItemBase::~ItemBase()
{
   delete d_ptr;
}

CollectionInterface* ItemBase::collection() const
{
   return d_ptr->m_pBackend;
}

void ItemBase::setCollection(CollectionInterface* backend)
{
   Q_ASSERT(backend->metaObject()->className() == this->metaObject()->className()
       || inherits(backend->metaObject()->className())
   );
   d_ptr->m_pBackend = backend;
}

///Save the contact
bool ItemBase::save() const
{
   if (!d_ptr->m_pBackend)
      return false;

   return d_ptr->m_pBackend->save(this);
}

///Show an implementation dependant dialog to edit the contact
bool ItemBase::edit()
{
   if (!d_ptr->m_pBackend)
      return false;

   return d_ptr->m_pBackend->edit(this);
}

///Remove the contact from the backend
bool ItemBase::remove()
{
   if (!d_ptr->m_pBackend)
      return false;

   return d_ptr->m_pBackend->remove(this);
}

bool ItemBase::isActive() const
{
   return d_ptr->m_pBackend->isEnabled() && d_ptr->m_isActive;
}
