/****************************************************************************
 *   Copyright (C) 2014-2016 by Savoir-faire Linux                          *
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

//Parent
#include "persondirectory.h"

//Std
#include <memory>
#include <vector>

//Ring library
#include "person.h"
#include "call.h"
#include "uri.h"
#include "contactmethod.h"
#include "picocms/collectioninterface.h"
#include "picocms/collectionmodel.h"
#include "picocms/collectioneditor.h"
#include "individual.h"
#include "collections/transitionalpersonbackend.h"
#include "collections/peerprofilecollection2.h"

//Qt
#include <QtCore/QHash>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>

class PersonItemNode
{
public:

   enum class NodeType {
      PERSON,
      NUMBER,
   };

   PersonItemNode(Person* p, const NodeType type);
   PersonItemNode(ContactMethod* cm, const NodeType type);
   Person* m_pPerson;
   ContactMethod* m_pContactMethod {nullptr};
   int m_Index;
   std::vector<std::unique_ptr<PersonItemNode>> m_lChildren;
   PersonItemNode* m_pParent {nullptr};
   NodeType m_Type;

};

class PersonDirectoryPrivate final : public QObject
{
   Q_OBJECT
public:
   PersonDirectoryPrivate(PersonDirectory* parent);

   //Attributes
//    QVector<CollectionInterface*> m_lBackends;
   QHash<QByteArray,PersonPlaceHolder*> m_hPlaceholders;

   //Indexes
   QHash<QByteArray,Person*> m_hPersonsByUid;
   std::vector<std::unique_ptr<PersonItemNode>> m_lPersons;

private:
   PersonDirectory* q_ptr;
//    void slotPersonAdded(Person* c);

public Q_SLOTS:
   void slotLastUsedTimeChanged(time_t t) const;
};

PersonItemNode::PersonItemNode(Person* p, const NodeType type) :
m_Type(type),m_pPerson(p)
{

}

PersonItemNode::PersonItemNode(ContactMethod* cm, const NodeType type) :
m_Type(type),m_pContactMethod(cm)
{

}

PersonDirectoryPrivate::PersonDirectoryPrivate(PersonDirectory* parent) : QObject(parent), q_ptr(parent)
{

}

///Constructor
PersonDirectory::PersonDirectory(QObject* par) : QAbstractItemModel(par?par:QCoreApplication::instance()), CollectionManagerInterface<Person>(this),
d_ptr(new PersonDirectoryPrivate(this))
{
   setObjectName(QStringLiteral("PersonDirectory"));
}

///Destructor
PersonDirectory::~PersonDirectory()
{
   d_ptr->m_hPersonsByUid.clear();
}

/*****************************************************************************
 *                                                                           *
 *                                   Model                                   *
 *                                                                           *
 ****************************************************************************/

QHash<int,QByteArray> PersonDirectory::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;
    if (!initRoles) {
      QHash<int, QByteArray>::const_iterator i;
      for (i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); ++i)
         roles[i.key()] = i.value();

      initRoles = true;
      roles[ (int)Person::Role::Organization      ] = "organization";
      roles[ (int)Person::Role::Group             ] = "group";
      roles[ (int)Person::Role::Department        ] = "department";
      roles[ (int)Person::Role::PreferredEmail    ] = "preferredEmail";
      roles[ (int)Person::Role::FormattedLastUsed ] = "formattedLastUsed";
      roles[ (int)Person::Role::IndexedLastUsed   ] = "indexedLastUsed";
      roles[ (int)Person::Role::DatedLastUsed     ] = "datedLastUsed";
      roles[ (int)Person::Role::FormattedName     ] = "formattedName";
      roles[ (int)Person::Role::LastName          ] = "lastName";
      roles[ (int)Person::Role::PrimaryName       ] = "primaryName";
      roles[ (int)Person::Role::NickName          ] = "nickName";
      roles[ (int)Person::Role::Individual        ] = "individual";
      roles[ (int)Person::Role::Filter            ] = "filter"; //All roles, all at once
      roles[ (int)Person::Role::DropState         ] = "dropState"; //State for drag and drop
   }
   return roles;
}

bool PersonDirectory::setData( const QModelIndex& idx, const QVariant &value, int role)
{
   Q_UNUSED(idx)
   Q_UNUSED(value)
   Q_UNUSED(role)
   return false;
}

QVariant PersonDirectory::data( const QModelIndex& idx, int role) const
{
   if (!idx.isValid())
      return QVariant();
   const PersonItemNode* c = static_cast<PersonItemNode*>(idx.internalPointer());

   switch(c->m_Type) {
      case PersonItemNode::NodeType::PERSON:
         return c->m_pPerson->roleData(role);
      case PersonItemNode::NodeType::NUMBER:
         return c->m_pContactMethod->roleData(role);
   }
   return QVariant();
}

QVariant PersonDirectory::headerData(int section, Qt::Orientation orientation, int role) const
{
   Q_UNUSED(section)
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant(tr("Persons"));
   return QVariant();
}

int PersonDirectory::rowCount( const QModelIndex& par ) const
{
   if (!par.isValid()) {
      return d_ptr->m_lPersons.size();
   }
   else if (!par.parent().isValid() && static_cast<unsigned>(par.row()) < d_ptr->m_lPersons.size()) {
      return d_ptr->m_lPersons[par.row()]->m_lChildren.size();
   }
   return 0;
}

Qt::ItemFlags PersonDirectory::flags( const QModelIndex& idx ) const
{
   if (!idx.isValid())
      return Qt::NoItemFlags;
   return Qt::ItemIsEnabled | ((idx.parent().isValid())?Qt::ItemIsSelectable:Qt::ItemIsEnabled);
}

int PersonDirectory::columnCount ( const QModelIndex& par) const
{
   Q_UNUSED(par)
   return 1;
}

QModelIndex PersonDirectory::parent( const QModelIndex& idx) const
{
   if (!idx.isValid())
      return QModelIndex();
   PersonItemNode* modelItem = (PersonItemNode*)idx.internalPointer();
   if (modelItem && modelItem->m_pParent) {
      return createIndex(modelItem->m_pParent->m_Index,0,modelItem->m_pParent);
   }
   return QModelIndex();
}

QModelIndex PersonDirectory::index( int row, int column, const QModelIndex& par) const
{
   if (row >= 0 && column >= 0) {
      if (!par.isValid() && d_ptr->m_lPersons.size() > static_cast<unsigned>(row)) {
         return createIndex(row,column,d_ptr->m_lPersons[row].get());
      } else if (par.isValid() && d_ptr->m_lPersons[par.row()]->m_lChildren.size() > static_cast<unsigned>(row)) {
         PersonItemNode* modelItem = (PersonItemNode*)par.internalPointer();
         if (modelItem && static_cast<unsigned>(row) < modelItem->m_lChildren.size())
            return createIndex(row,column,modelItem->m_lChildren[row].get());
      }
   }
   return QModelIndex();
}

/*****************************************************************************
 *                                                                           *
 *                                  Mutator                                  *
 *                                                                           *
 ****************************************************************************/


///Find contact by UID
Person* PersonDirectory::getPersonByUid(const QByteArray& uid)
{
   return d_ptr->m_hPersonsByUid[uid];
}

/**
 * Create a temporary contact or return the existing one for an UID
 * This temporary contact should eventually be merged into the real one
 */
Person* PersonDirectory::getPlaceHolder(const QByteArray& uid )
{
   Person* ct = d_ptr->m_hPersonsByUid[uid];

   //Do not create a placeholder if the real deal exist
   if (ct) {
      return ct;
   }

   //Do not re-create if it already exist
   ct = d_ptr->m_hPlaceholders[uid];
   if (ct)
      return ct;

   PersonPlaceHolder* ct2 = new PersonPlaceHolder(uid);

   d_ptr->m_hPlaceholders[ct2->uid()] = ct2;
   return ct2;
}

void PersonDirectory::collectionAddedCallback(CollectionInterface* backend)
{
   Q_UNUSED(backend)
}

bool PersonDirectory::addItemCallback(const Person* c)
{
   //Add to the model
   beginInsertRows(QModelIndex(),d_ptr->m_lPersons.size(),d_ptr->m_lPersons.size());
   d_ptr->m_lPersons.emplace_back(new PersonItemNode {const_cast<Person*>(c), PersonItemNode::NodeType::PERSON});
   auto& inode = *d_ptr->m_lPersons.back();
   inode.m_Index = d_ptr->m_lPersons.size() - 1;
   d_ptr->m_hPersonsByUid[c->uid()] = const_cast<Person*>(c);
   endInsertRows();
   emit newPersonAdded(c);

   //Add the contact method nodes
   const auto idx = index(inode.m_Index,0);
   beginInsertRows(idx,0,c->individual()->phoneNumbers().size());
   inode.m_lChildren.reserve(c->individual()->phoneNumbers().size());
   const auto cms = c->individual()->phoneNumbers();
   for (auto& m : qAsConst(cms)) {
      inode.m_lChildren.emplace_back(new PersonItemNode {m, PersonItemNode::NodeType::NUMBER});
      auto& child = *inode.m_lChildren.back();
      child.m_Index = inode.m_lChildren.size() - 1;
      child.m_pParent = &inode; //TODO support adding new contact methods on the fly
   }
   endInsertRows();

   //Deprecate the placeholder
   if (d_ptr->m_hPlaceholders.contains(c->uid())) {
      PersonPlaceHolder* c2 = d_ptr->m_hPlaceholders[c->uid()];
      if (c2) {
         c2->merge(const_cast<Person*>(c));
         d_ptr->m_hPlaceholders[c->uid()] = nullptr;
      }
   }

   connect(c, &Person::lastUsedTimeChanged, d_ptr.data(), &PersonDirectoryPrivate::slotLastUsedTimeChanged);

   if (c->lastUsedTime())
      emit lastUsedTimeChanged(const_cast<Person*>(c), c->lastUsedTime());

   return true;
}

bool PersonDirectory::removeItemCallback(const Person* item)
{
   for (unsigned int nodeIdx = 0; nodeIdx < d_ptr->m_lPersons.size(); ++nodeIdx) {
      auto person = d_ptr->m_lPersons[nodeIdx]->m_pPerson;
      if (person == item) {

          const auto cms = person->individual()->phoneNumbers();
          for ( const auto cm : qAsConst(cms) )
              // cm is not linked to any person anymore
              cm->setPerson(nullptr);

          // Remove contact
          beginRemoveRows(QModelIndex(), nodeIdx, nodeIdx);
          d_ptr->m_lPersons[nodeIdx].release();
          d_ptr->m_lPersons.erase(d_ptr->m_lPersons.begin() + nodeIdx);

          // update indexes
          for (unsigned int i = 0; i < d_ptr->m_lPersons.size(); ++i) {
             d_ptr->m_lPersons[i]->m_Index = i;
             for (unsigned int j = 0; j < d_ptr->m_lPersons[i]->m_lChildren.size(); ++j)
                d_ptr->m_lPersons[i]->m_lChildren[j]->m_Index = j;
          }
          endRemoveRows();

          //Deprecate the placeholder
          if (d_ptr->m_hPlaceholders.contains(item->uid())) {
             PersonPlaceHolder* placeholder = d_ptr->m_hPlaceholders[item->uid()];
             if (placeholder)
                d_ptr->m_hPlaceholders[item->uid()] = nullptr;
          }
          break;
      }
   }

   emit personRemoved(item);
   return item;
}

///@deprecated
bool PersonDirectory::addNewPerson(Person* c, CollectionInterface* backend)
{
   if ((!backend) && (!collections().size()))
      return false;

   bool ret = false;

   if (backend) {
      ret |= backend->editor<Person>()->addNew(c);
   }
   else for (CollectionInterface* col :collections(CollectionInterface::SupportedFeatures::ADD)) {
      if (col->id() != "trcb") //Do not add to the transitional contact backend
         ret |= col->editor<Person>()->addNew(c);
   }

   return ret;
}

void PersonDirectoryPrivate::slotLastUsedTimeChanged(time_t t) const
{
   emit q_ptr->lastUsedTimeChanged(static_cast<Person*>(QObject::sender()), t);
}


#include <persondirectory.moc>
