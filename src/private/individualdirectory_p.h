/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
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
#pragma once

#include <QtCore/QObject>

//Ring
class IndividualDirectory;
class LocalNameServiceCache;
#include "contactmethod.h"
#include "account.h"
#include "namedirectory.h"

//Internal data structures
///@struct NumberWrapper Wrap phone numbers to prevent collisions
struct NumberWrapper final {
   explicit NumberWrapper(const QString& k) : key(k) {}

   QString key;
   QVector<ContactMethod*> numbers;
};

class MostPopularNumberModel final : public QAbstractListModel //TODO remove
{
   Q_OBJECT
public:
   explicit MostPopularNumberModel();

   //Model functions
   virtual QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole     ) const override;
   virtual int           rowCount ( const QModelIndex& parent = QModelIndex()                ) const override;
   virtual Qt::ItemFlags flags    ( const QModelIndex& index                                 ) const override;
   virtual bool          setData  ( const QModelIndex& index, const QVariant &value, int role)       override;

   void addRow();
   void reload();
};

class IndividualDirectoryPrivate final : public QObject
{
   Q_OBJECT
public:
   explicit IndividualDirectoryPrivate(IndividualDirectory* parent);


   //Model columns
   enum class Columns {
      URI              = 0,
      BEST_NAME        = 1,
      TYPE             = 2,
      CONTACT          = 3,
      IS_SELF          = 4,
      ACCOUNT          = 5,
      STATE            = 6,
      CALL_COUNT       = 7,
      WEEK_COUNT       = 8,
      TRIM_COUNT       = 9,
      HAVE_CALLED      = 10,
      LAST_USED        = 11,
      NAME_COUNT       = 12,
      TOTAL_SECONDS    = 13,
      POPULARITY_INDEX = 14,
      BOOKMARED        = 15,
      TRACKED          = 16,
      HAS_CERTIFICATE  = 17,
      PRESENT          = 18,
      PRESENCE_MESSAGE = 19,
      UID              = 20,
      REGISTERED_NAME  = 21,
   };


   //Helpers
   void indexNumber(ContactMethod* number, const QStringList& names   );
   void setAccount (ContactMethod* number,       Account*     account );
   ContactMethod* fillDetails(NumberWrapper* wrap, const URI& strippedUri, Account* account, Person* contact, const QString& type);
   void registerAlternateNames(ContactMethod* number, Account* account, const URI& uri, const URI& extendedUri);

   //Attributes
   QVector<ContactMethod*>         m_lNumbers         ;
   QHash<QString,NumberWrapper*> m_hDirectory       ;
   QVector<ContactMethod*>         m_lPopularityIndex ;
   QMap<QString,NumberWrapper*>  m_lSortedNames     ;
   QMap<QString,NumberWrapper*>  m_hSortedNumbers   ;
   QHash<QString,NumberWrapper*> m_hNumbersByNames  ;
   bool                          m_CallWithAccount  ;
   MostPopularNumberModel*       m_pPopularModel    ;
   LocalNameServiceCache*        m_pNameServiceCache {nullptr};
   QMutex                        m_DirectoryAccess;

   Q_DECLARE_PUBLIC(IndividualDirectory)

private:
   IndividualDirectory* q_ptr;

private Q_SLOTS:
   void slotCallAdded(Call* call);
   void slotChanged();
   void slotLastUsedChanged(time_t t);
   void slotContactChanged(Person* newContact, Person* oldContact);
   void slotRegisteredNameFound(Account* account, NameDirectory::LookupStatus status, const QString& address, const QString& name);
   void slotContactMethodMerged(ContactMethod* other);
   void slotAccountStateChanged(Account* a, const Account::RegistrationState state);

   //From DBus
   void slotNewBuddySubscription(const QString& uri, const QString& accountId, bool status, const QString& message);
};
