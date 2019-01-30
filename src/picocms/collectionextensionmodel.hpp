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


class CollectionExtensionModelSpecificPrivate;

class CollectionExtensionModelSpecific final
{
public:
   static const QList<CollectionExtensionInterface*> entries();
   static const QList<std::function<void()>> queuedEntries();
   static void insertEntry(CollectionExtensionInterface* e);
   static void insertQueuedEntry(const std::function<void()>& e);

   static QMutex m_Mutex;
   static QMutex m_InsertMutex;
};

template<class T>
int CollectionExtensionModel::registerExtension()
{
   // Make sure the ID is unique to avoid that one in a million collision that
   // causes a ~10 bytes head overflow.
   CollectionExtensionModelSpecific::m_Mutex.lock();
   CollectionExtensionModelSpecific::m_InsertMutex.lock();

   static bool typeInit = false;

   static int typeId = CollectionExtensionModelSpecific::entries().size()
      + CollectionExtensionModelSpecific::queuedEntries().size();

   CollectionExtensionModelSpecific::m_Mutex.unlock();
   CollectionExtensionModelSpecific::m_InsertMutex.unlock();

   if (!typeInit) {
      CollectionExtensionModelSpecific::insertQueuedEntry([]() {
          CollectionExtensionModelSpecific::insertEntry(new T(nullptr));
      });
   }

   return typeId;
}

template<class T>
int CollectionExtensionModel::getExtensionId()
{
   return registerExtension<T>();
}

template<class T>
T* CollectionExtensionModel::getExtension()
{
   CollectionExtensionModel::instance();
   return static_cast<T*>(CollectionExtensionModelSpecific::entries()[registerExtension<T>()]);
}
