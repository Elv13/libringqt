/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
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
#include "collectionextensionmodel.h"

// Qt
#include <QtCore/QMutex>

#include "collectionextensioninterface.h"

QMutex CollectionExtensionModelSpecific::m_Mutex = {};
QMutex CollectionExtensionModelSpecific::m_InsertMutex = {};

class CollectionExtensionModelPrivate
{
public:
};

class CollectionExtensionModelSpecificPrivate final
{
public:
    static QList<std::function<void()>>         m_slQueuedEntries;
    static QList<CollectionExtensionInterface*> m_slEntries;
};

QList<std::function<void()>> CollectionExtensionModelSpecificPrivate::m_slQueuedEntries = {};
QList<CollectionExtensionInterface*> CollectionExtensionModelSpecificPrivate::m_slEntries = {};

CollectionExtensionModel::CollectionExtensionModel() : d_ptr(new CollectionExtensionModelPrivate)
{

    while (true) {
        const auto currentEntries = CollectionExtensionModelSpecific::queuedEntries();

        // Make sure nothing is added while ::register is called
        {
           QMutexLocker l(&CollectionExtensionModelSpecific::m_Mutex);

           for (const auto& ini : qAsConst(currentEntries))
              ini();

           CollectionExtensionModelSpecificPrivate::m_slQueuedEntries.clear();
        }

        // Technically, there should be a small usleep here...
        QMutexLocker l(&CollectionExtensionModelSpecific::m_Mutex);
        if (CollectionExtensionModelSpecificPrivate::m_slQueuedEntries.isEmpty()) {
            break;
        }
    }
}

CollectionExtensionModel::~CollectionExtensionModel()
{
   delete d_ptr;
}

const QList<CollectionExtensionInterface*>& CollectionExtensionModelSpecific::entries()
{
   return CollectionExtensionModelSpecificPrivate::m_slEntries;
}

/** Will insert elements into the entries() once the model has been created.
/*
 * This avoids a static QObject initialization and those are now banned by the
 * tests.
 */
const QList<std::function<void()>>& CollectionExtensionModelSpecific::queuedEntries()
{
   return CollectionExtensionModelSpecificPrivate::m_slQueuedEntries;
}

void CollectionExtensionModelSpecific::insertEntry(CollectionExtensionInterface* e)
{
    CollectionExtensionModelSpecificPrivate::m_slEntries << e;
}

void CollectionExtensionModelSpecific::insertQueuedEntry(const std::function<void()>& e)
{
   // The ids are based on the size of this vector, so it is important to avoid
   // duplicate IDs (and thus wrong casting) during load.
   QMutexLocker l(&m_InsertMutex);
   CollectionExtensionModelSpecificPrivate::m_slQueuedEntries << e;
}

CollectionExtensionModel& CollectionExtensionModel::instance()
{
    static auto instance = new CollectionExtensionModel;
    return *instance;
}

QVariant CollectionExtensionModel::data( const QModelIndex& index, int role ) const
{
   if (!index.isValid())
      return QVariant();

   return CollectionExtensionModelSpecific::entries()[index.row()]->data(role);
}

int CollectionExtensionModel::rowCount( const QModelIndex& parent) const
{
   return parent.isValid() ? 0 : CollectionExtensionModelSpecific::entries().size();
}

Qt::ItemFlags CollectionExtensionModel::flags( const QModelIndex& index) const
{
   return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
}

bool CollectionExtensionModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
   Q_UNUSED(index)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

QHash<int,QByteArray> CollectionExtensionModel::roleNames() const
{
   return {};
}
