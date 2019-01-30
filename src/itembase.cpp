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

ItemBase::ItemBase(QObject* parent) :QObject(nullptr), d_ptr(new ItemBasePrivate())
{
   // Otherwise QML will claim and destroy them
   //Q_ASSERT(parent);

   // Because after moveToThread, the code is still running in the old thread,
   // setting the parent may be done in multiple threads in parallel on the
   // same parent. In that case the parent object children list is potentially
   // being resized in *another thread context*. This avoids causing such issue.
   if (QCoreApplication::instance()->thread() != thread()) {
      // Set a temporary parent to be certain that if a thread event loop
      // iteration takes a long time, there is no window of opportunity for
      // QtQuick to claim and free the object.
      setParent(QCoreApplication::instance());

      QObject::moveToThread(QCoreApplication::instance()->thread());
      QTimer::singleShot(0, [this, parent]() {
         QObject::setParent(parent);
      });
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
