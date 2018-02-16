/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
 *   Authors : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>*
 *             Alexandre Lision <alexandre.lision@savoirfairelinux.com>     *
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
#include "profilemodel.h"

//Qt
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>
#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QMimeData>
#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

//Ring
#include "accountmodel.h"
#include "collectioninterface.h"
#include "collectioneditor.h"
#include "personmodel.h"
#include "callmodel.h"
#include "contactmethod.h"
#include "person.h"
#include "globalinstances.h"
#include "private/vcardutils.h"
#include "mime.h"

struct AccountStruct final {
    Account*             m_pAccount        {nullptr};
    QItemSelectionModel* m_pSelectionModel {nullptr};
};

struct ProfileNode final
{
    explicit ProfileNode(): m_pPerson(nullptr) {}

    virtual ~ProfileNode() {
        QObject::disconnect(m_ChangedConn);
        //if (m_pPerson && type == Type::PROFILE)
        //    delete m_pPerson; //FIXME leak

        qDeleteAll(children);
    }

    enum class Type : bool {
        PROFILE,
        ACCOUNT,
    };

    using Nodes = QVector<ProfileNode*>;

    Nodes        children      {                                  };
    ProfileNode* parent        {              nullptr             };
    Type         type          {    ProfileNode::Type::ACCOUNT    };
    int          m_Index       {                 0                };
    uint         m_ParentIndex { std::numeric_limits<uint>::max() };
    union {
        Person*       m_pPerson;
        AccountStruct m_AccData;
    };

    QMetaObject::Connection m_ChangedConn;
};

class ProfileModelPrivate final : public QObject
{
    Q_OBJECT
public:
    ProfileModelPrivate(ProfileModel* parent);
    QVector<ProfileNode*> m_lProfiles;
    QStringList m_lMimes;
    QItemSelectionModel* m_pSelectionModel {nullptr};
    QItemSelectionModel* m_pSortedProxySelectionModel {nullptr};
    QSortFilterProxyModel* m_pSortedProxyModel {nullptr};
    ProfileNode* m_pDefaultProfile {nullptr};

    //Helpers
    void updateIndexes();
    void regenParentIndexes();
    inline bool addProfile(Person* person, const QString& name, CollectionInterface* col);

    void  slotAccountAdded(Account* acc);
    void setProfile(ProfileNode* accNode, ProfileNode* proNode);
    ProfileNode* profileNodeById(const QByteArray& id);
    ProfileNode* profileNodeForAccount(const Account* a);
    ProfileNode* nodeForAccount(const Account* a);

    //Constants
    constexpr static const int c_OrderRole = 9999;

private Q_SLOTS:
    void slotDataChanged(const QModelIndex& tl,const QModelIndex& br);
    void slotLayoutchanged();
    void slotDelayedInit();
    void slotRowsInserted(const QModelIndex& index, int first, int last);
    void slotRowsMoved   (const QModelIndex& index, int first, int last, const QModelIndex& newPar, int newIdx);
    void slotAccountRemoved(Account* a);

private:
    ProfileModel* q_ptr;
};

void ProfileModelPrivate::slotAccountAdded(Account* acc)
{
    auto currentProfile = acc->profile();
    qDebug() << "Account added" << acc << currentProfile;

    // Add the profile when the account is created
    if (!currentProfile) {
        qWarning() << "No profile selected or none exists, fall back tot the default";

        if (Q_UNLIKELY(!m_pDefaultProfile)) {
            qWarning() << "Failed to set a profile: there is none";
            return;
        }

        m_pDefaultProfile->m_pPerson->addCustomField(
            VCardUtils::Property::X_RINGACCOUNT, acc->id()
        );
        m_pDefaultProfile->m_pPerson->save();

        currentProfile = m_pDefaultProfile->m_pPerson;
    }

    auto currentNode =  profileNodeById(currentProfile->uid());

    if (!currentNode) {
        qWarning() << "Account must have a profile parent, doing nothing";
        return;
    }

    auto account_pro = new ProfileNode;
    account_pro->type    = ProfileNode::Type::ACCOUNT;
    account_pro->parent  = currentNode;
    account_pro->m_AccData.m_pSelectionModel = nullptr;
    account_pro->m_AccData.m_pAccount = acc;
    account_pro->m_Index = currentNode->children.size();
    account_pro->m_ParentIndex = acc->index().row();

    q_ptr->beginInsertRows(ProfileModel::instance().index(currentNode->m_Index,0), currentNode->children.size(), currentNode->children.size());
    currentNode->children << account_pro;
    q_ptr->endInsertRows();
}

ProfileNode* ProfileModelPrivate::profileNodeById(const QByteArray& id)
{
    foreach(auto p, m_lProfiles) {
        if(p->m_pPerson->uid() == id)
            return p;
    }
    return nullptr;
}

ProfileModel& ProfileModel::instance()
{
    static auto instance = new ProfileModel(QCoreApplication::instance());
    return *instance;
}

ProfileModelPrivate::ProfileModelPrivate(ProfileModel* parent) : QObject(parent), q_ptr(parent)
{

}

///Avoid creating an initialization loop
void ProfileModelPrivate::slotDelayedInit()
{
    connect(&AccountModel::instance(), &QAbstractItemModel::dataChanged  , this, &ProfileModelPrivate::slotDataChanged   );
    connect(&AccountModel::instance(), &QAbstractItemModel::rowsInserted , this, &ProfileModelPrivate::slotRowsInserted  );
    connect(&AccountModel::instance(), &QAbstractItemModel::rowsMoved    , this, &ProfileModelPrivate::slotRowsMoved     );
    connect(&AccountModel::instance(), &QAbstractItemModel::layoutChanged, this, &ProfileModelPrivate::slotLayoutchanged );
    connect(&AccountModel::instance(), &AccountModel::accountRemoved     , this, &ProfileModelPrivate::slotAccountRemoved);
    connect(&AccountModel::instance(), &AccountModel::accountAdded       , this, &ProfileModelPrivate::slotAccountAdded);

    // Load existing accounts
    for (int i = 0; i < AccountModel::instance().rowCount(); i++)
        slotAccountAdded(AccountModel::instance()[i]);
}

ProfileModel::ProfileModel(QObject* parent) : QAbstractItemModel(parent),
CollectionManagerInterface<Person>(this), d_ptr(new ProfileModelPrivate(this))
{
    d_ptr->m_lMimes << RingMimes::PLAIN_TEXT << RingMimes::HTML_TEXT << RingMimes::ACCOUNT << RingMimes::PROFILE;
    //Once LibRingClient is ready, start listening
    QTimer::singleShot(0,d_ptr,SLOT(slotDelayedInit()));
}

ProfileModel::~ProfileModel()
{
    while (d_ptr->m_lProfiles.size()) {
        auto proNode = d_ptr->m_lProfiles[0];
        d_ptr->m_lProfiles.removeAt(0);
        delete proNode;
    }
    delete d_ptr;
}

QHash<int,QByteArray> ProfileModel::roleNames() const
{
    static QHash<int, QByteArray> roles = AccountModel::instance().roleNames();
    return roles;
}

void ProfileModelPrivate::updateIndexes()
{
    for (int i = 0; i < m_lProfiles.size(); ++i) {
        m_lProfiles[i]->m_Index = i;
        for (int j = 0; j < m_lProfiles[i]->children.size(); ++j) {
            m_lProfiles[i]->children[j]->m_Index = j;
        }
    }
}

void ProfileModelPrivate::slotDataChanged(const QModelIndex& tl,const QModelIndex& br)
{
    if (!tl.isValid() || (!br.isValid()))
        return;

    const auto idx = q_ptr->mapFromSource(tl);
    emit q_ptr->dataChanged(idx, idx);
}

void ProfileModelPrivate::slotLayoutchanged()
{
    emit q_ptr->layoutChanged();
}

void ProfileModelPrivate::regenParentIndexes()
{
    foreach(auto proNode, m_lProfiles) {
        foreach(auto a, proNode->children) {
            a->m_ParentIndex = a->m_AccData.m_pAccount->index().row();
        }
        const QModelIndex par = q_ptr->index(proNode->m_Index,0,QModelIndex());
        emit q_ptr->dataChanged(q_ptr->index(0,0,par),q_ptr->index(proNode->children.size()-1,0,par));
    }
}

void ProfileModelPrivate::slotAccountRemoved(Account* a)
{
    auto n = nodeForAccount(a);

    if (n && n->parent) {
        const QModelIndex profIdx = q_ptr->index(n->parent->m_Index, 0);
        if (profIdx.isValid()) {
            const int accIdx = n->m_Index;
            q_ptr->beginRemoveRows(profIdx, accIdx, accIdx);
            n->parent->children.removeAt(accIdx);
            for (int i = accIdx; i < n->parent->children.size(); i++)
                n->parent->children[i]->m_Index--;
            n->parent->m_pPerson->save();
            delete n;
            q_ptr->endRemoveRows();
        }
    }
    regenParentIndexes();
}

ProfileNode* ProfileModelPrivate::nodeForAccount(const Account* a)
{
    for (auto pro : m_lProfiles) {
        for (auto accNode : pro->children) {
            if (accNode->m_AccData.m_pAccount == a) {
                return accNode;
            }
        }
    }
    return nullptr;
}

ProfileNode* ProfileModelPrivate::profileNodeForAccount(const Account* a)
{
    for (auto pro : m_lProfiles) {
        for (auto accNode : pro->children) {
            if (accNode->m_AccData.m_pAccount == a) {
                return pro;
            }
        }
    }
    return nullptr;
}

void ProfileModelPrivate::slotRowsInserted(const QModelIndex& index, int first, int last)
{
    Q_UNUSED(index)
    Q_UNUSED(first)
    Q_UNUSED(last)
    regenParentIndexes();
}

void ProfileModelPrivate::slotRowsMoved(const QModelIndex& index, int first, int last, const QModelIndex& newPar, int newIdx)
{
    Q_UNUSED(index)
    Q_UNUSED(first)
    Q_UNUSED(last)
    Q_UNUSED(newPar)
    Q_UNUSED(newIdx)
    regenParentIndexes();
}

QModelIndex ProfileModel::mapToSource(const QModelIndex& idx) const
{
    if (!idx.isValid() || !idx.parent().isValid() || idx.model() != this)
        return QModelIndex();

    ProfileNode* profile = static_cast<ProfileNode*>(idx.parent().internalPointer());
    return profile->children[idx.row()]->m_AccData.m_pAccount->index();
}

QModelIndex ProfileModel::mapFromSource(const QModelIndex& idx) const
{
    if (!idx.isValid() || idx.model() != &AccountModel::instance())
        return QModelIndex();

    auto acc = AccountModel::instance().getAccountByModelIndex(idx);
    auto accNode = d_ptr->nodeForAccount(acc);

    //Something is wrong, there is an orphan
    if (!accNode)
        return QModelIndex();

    return index(accNode->m_Index, 0, index(accNode->parent->m_Index, 0, QModelIndex()));
}

QVariant ProfileModel::data(const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return QVariant();

    auto currentNode = static_cast<ProfileNode*>(index.internalPointer());

    switch (currentNode->type) {
        case ProfileNode::Type::PROFILE:
            return currentNode->m_pPerson->roleData(role);
        case ProfileNode::Type::ACCOUNT:
            switch(role) {
                case ProfileModelPrivate::c_OrderRole:
                    return currentNode->m_ParentIndex;
                default:
                    return currentNode->m_AccData.m_pAccount->roleData(role);
            }
    }

   return QVariant();
}

int ProfileModel::rowCount(const QModelIndex& parent ) const
{
    if (parent.isValid()) {
       auto proNode = static_cast<ProfileNode*>(parent.internalPointer());
       return proNode->children.size();
    }
    return d_ptr->m_lProfiles.size();
}

int ProfileModel::columnCount(const QModelIndex& parent ) const
{
   Q_UNUSED(parent)
   return 1;
}

QModelIndex ProfileModel::parent(const QModelIndex& idx ) const
{
    ProfileNode* current = static_cast<ProfileNode*>(idx.internalPointer());

    if (!current)
        return QModelIndex();
    switch (current->type) {
        case ProfileNode::Type::PROFILE:
            return QModelIndex();
        case ProfileNode::Type::ACCOUNT:
            return index(current->parent->m_Index, 0, QModelIndex());
    }
    return QModelIndex();
}

QModelIndex ProfileModel::index( int row, int column, const QModelIndex& parent ) const
{
    ProfileNode* current = static_cast<ProfileNode*>(parent.internalPointer());
    if(parent.isValid() && current && !column && row >= 0 && row < current->children.size()) {
        return createIndex(row, 0, current->children[row]);
    }
    else if(row < d_ptr->m_lProfiles.size() && row >= 0 && !column) {
        return createIndex(row, 0, d_ptr->m_lProfiles[row]);
    }
    return QModelIndex();
}

Qt::ItemFlags ProfileModel::flags(const QModelIndex& index ) const
{
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    ProfileNode* current = static_cast<ProfileNode*>(index.internalPointer());

    if (current && current->parent)
        return QAbstractItemModel::flags(index)
            | Qt::ItemIsUserCheckable
            | Qt::ItemIsEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsDragEnabled
            | Qt::ItemIsDropEnabled;

    return QAbstractItemModel::flags(index)
            | Qt::ItemIsEnabled
            | Qt::ItemIsSelectable
            | Qt::ItemIsDragEnabled
            | Qt::ItemIsDropEnabled;
}

QStringList ProfileModel::mimeTypes() const
{
    return d_ptr->m_lMimes;
}

QMimeData* ProfileModel::mimeData(const QModelIndexList &indexes) const
{
    auto mMimeData = new QMimeData();

    //FIXME: this won't work for multiple indexes
    for (const QModelIndex& index : indexes) {
        ProfileNode* current = static_cast<ProfileNode*>(index.internalPointer());

        if (!index.isValid() || !current) {
            qWarning() << "invalid index to create mimeData, ignoring";
            continue;
        }

        switch (current->type) {
            case ProfileNode::Type::PROFILE:
                mMimeData->setData(RingMimes::PROFILE , current->m_pPerson->uid());
                break;
            case ProfileNode::Type::ACCOUNT:
                mMimeData->setData(RingMimes::ACCOUNT , current->m_AccData.m_pAccount->id());
                break;
            default:
                qWarning() << "Unknown node type to create mimedata";
                return nullptr;
        }

    }
    return mMimeData;
}

///Return valid payload types
int ProfileModel::acceptedPayloadTypes() const
{
    return CallModel::DropPayloadType::ACCOUNT;
}

QItemSelectionModel* ProfileModel::selectionModel() const
{
    if (!d_ptr->m_pSelectionModel) {
        d_ptr->m_pSelectionModel = new QItemSelectionModel(const_cast<ProfileModel*>(this));

        connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& i) {
            const auto accIdx = mapToSource(i);
            AccountModel::instance().selectionModel()->setCurrentIndex(accIdx, QItemSelectionModel::ClearAndSelect);
        });
    }

    return d_ptr->m_pSelectionModel;
}

QItemSelectionModel* ProfileModel::sortedProxySelectionModel() const
{
    if (!d_ptr->m_pSortedProxySelectionModel) {
        d_ptr->m_pSortedProxySelectionModel = new QItemSelectionModel(static_cast<QSortFilterProxyModel*>(sortedProxyModel()));

        connect(d_ptr->m_pSortedProxySelectionModel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& i) {
            const auto accIdx = mapToSource(
                static_cast<QSortFilterProxyModel*>(sortedProxyModel())->mapToSource(i)
            );
            AccountModel::instance().selectionModel()->setCurrentIndex(accIdx, QItemSelectionModel::ClearAndSelect);
        });
    }

    return d_ptr->m_pSortedProxySelectionModel;
}

QAbstractItemModel* ProfileModel::sortedProxyModel() const
{
    if (!d_ptr->m_pSortedProxyModel) {
        d_ptr->m_pSortedProxyModel = new QSortFilterProxyModel(&ProfileModel::instance());
        d_ptr->m_pSortedProxyModel->setSourceModel(const_cast<ProfileModel*>(this));
        d_ptr->m_pSortedProxyModel->setSortRole(ProfileModelPrivate::c_OrderRole);
        d_ptr->m_pSortedProxyModel->sort(0);
    }

    return d_ptr->m_pSortedProxyModel;
}

void ProfileModelPrivate::setProfile(ProfileNode* accNode, ProfileNode* proNode)
{
    Q_ASSERT(accNode->type == ProfileNode::Type::ACCOUNT);
    Q_ASSERT(proNode->type == ProfileNode::Type::PROFILE );

    auto oldProfile = accNode->parent;

    const auto oldParentIdx = q_ptr->index(oldProfile->m_Index, 0);

    const auto newParentIdx = q_ptr->index(proNode->m_Index, 0);

    if(!q_ptr->beginMoveRows(oldParentIdx, accNode->m_Index, accNode->m_Index, newParentIdx, 0))
        return;

    Q_ASSERT(accNode == oldProfile->children.at(accNode->m_Index));

    qDebug() << "Moving:" << accNode->m_AccData.m_pAccount->alias();

    const bool ret = oldProfile->m_pPerson->removeCustomField(
        VCardUtils::Property::X_RINGACCOUNT,
        accNode->m_AccData.m_pAccount->id()
    );

    if (Q_UNLIKELY(!ret)) {
        qWarning() << "Removing from the old profile failed";
    }

    oldProfile->children.remove(accNode->m_Index);
    accNode->parent = proNode;
    proNode->children.insert(0, accNode);

    updateIndexes();

    proNode->m_pPerson->addCustomField(
        VCardUtils::Property::X_RINGACCOUNT,
        accNode->m_AccData.m_pAccount->id()
    );

    proNode->m_pPerson->save();
    oldProfile->m_pPerson->save();

    q_ptr->endMoveRows();
}

bool ProfileModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
   Q_UNUSED(action)

   QModelIndex accountIdx, profileIdx;

   // For some reasons, it seem that row and column are {-1,-1} when using a
   // proxy model. The proxy is required to properly sort the accounts, so
   // this code "fix" the values
   if (parent.parent().isValid()) {
       accountIdx = parent;
       profileIdx = parent.parent();
       row        = accountIdx.row();
       column     = accountIdx.column();
   }
   else {
       accountIdx = index(row, column, parent);
       profileIdx = parent;
   }

   if(((!profileIdx.isValid()) && row < 0) || column > 0) {
      qDebug() << "Row or column invalid";
      return false;
   }

   if (data->hasFormat(RingMimes::ACCOUNT)) {
      qDebug() << "Dropping account";

      const QByteArray accountId = data->data(RingMimes::ACCOUNT);
      ProfileNode* newProfile = nullptr;

      // Dropped on a profile index, append it at the end
      if(profileIdx.isValid()) {
         qDebug() << "Dropping on profile title";
         newProfile = static_cast<ProfileNode*>(profileIdx.internalPointer());
      }
      // Dropped on an account
      else if (profileIdx.isValid()) {
         newProfile = static_cast<ProfileNode*>(profileIdx.internalPointer());
      }

      if ((!newProfile) || (!newProfile->m_pPerson)) {
         qDebug() << "Invalid profile";
         return false;
      }

      // Use the account ID to locate the original location
      auto acc = AccountModel::instance().getById(accountId);

      if (!acc)
         return false;

      auto accountToMove = d_ptr->profileNodeForAccount(acc);\

      d_ptr->setProfile(accountToMove, newProfile);
   }
   else if (data->hasFormat(RingMimes::PROFILE)) {
      qDebug() << "Dropping profile on row" << row;

      int destinationRow = -1;
      if(row < 0) {
         // drop on bottom of the list
         destinationRow = d_ptr->m_lProfiles.size();
      }
      else {
         destinationRow = row;
      }

      auto moving = d_ptr->profileNodeById(data->data(RingMimes::PROFILE));
      if(!moving)
         return false;

      if(!beginMoveRows(QModelIndex(), moving->m_Index, moving->m_Index, QModelIndex(), destinationRow)) {
         return false;
      }

      d_ptr->m_lProfiles.removeAt(moving->m_Index);
      d_ptr->m_lProfiles.insert(destinationRow, moving);
      d_ptr->updateIndexes();
      endMoveRows();

      return true;
   }
   return false;
}

bool ProfileModel::setData(const QModelIndex& index, const QVariant &value, int role )
{
   if (!index.isValid())
      return false;

   ProfileNode* current = static_cast<ProfileNode*>(index.internalPointer());

   if (current->parent) {
      return AccountModel::instance().setData(mapToSource(index),value,role);
   }
   return false;
}

QVariant ProfileModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
   Q_UNUSED(section)
   Q_UNUSED(orientation)
   if (role == Qt::DisplayRole) return tr("Profiles");
   return QVariant();
}

void ProfileModel::collectionAddedCallback(CollectionInterface* backend)
{
   Q_UNUSED(backend)
}

bool ProfileModel::addItemCallback(const Person* pro)
{
    auto proNode       = new ProfileNode           ;
    proNode->type      = ProfileNode::Type::PROFILE;
    proNode->m_pPerson = const_cast<Person*>(pro);
    proNode->m_Index   = d_ptr->m_lProfiles.size();

    if (pro->hasCustomField(VCardUtils::Property::X_RINGDEFAULTACCOUNT))
        d_ptr->m_pDefaultProfile = proNode;

    beginInsertRows({}, d_ptr->m_lProfiles.size(), d_ptr->m_lProfiles.size());
    d_ptr->m_lProfiles << proNode;
    endInsertRows();

    const auto accountIds = pro->getCustomFields(VCardUtils::Property::X_RINGACCOUNT);

    for (const auto& accId : qAsConst(accountIds)) {
        if (auto a = AccountModel::instance().getById(accId)) {
            qDebug() << "I GOT AN ACCOUNT" << a << (pro == a->profile());
            a->setProfile(const_cast<Person*>(pro));
        }
    }

    selectionModel()->setCurrentIndex(index(proNode->m_Index, 0), QItemSelectionModel::ClearAndSelect);

    proNode->m_ChangedConn = connect(pro, &Person::changed, this, [this, proNode]() {
        if (proNode->m_pPerson->isActive()) {
            const QModelIndex idx = index(proNode->m_Index, 0);
            emit dataChanged(idx, idx);
        }
    });
    return true;
}

bool ProfileModel::removeItemCallback(const Person* item)
{
    int nodeIdx = -1;
    bool isFound = false;

    while (not isFound && ++nodeIdx < d_ptr->m_lProfiles.size()) {
       isFound = (d_ptr->m_lProfiles[nodeIdx]->m_pPerson == item);
    }

    // Remove the node and update all other indices
    if (isFound) {
        // Remove the profile
        beginRemoveRows(QModelIndex(), nodeIdx, nodeIdx);
        auto toDelete = d_ptr->m_lProfiles[nodeIdx];
        d_ptr->m_lProfiles.removeAt(nodeIdx);
        d_ptr->updateIndexes();
        delete toDelete;
        endRemoveRows();
    }
    return true;
}

/**
 * Remove an unused profile
 *
 * @return If removing the profile has been successful
 */
bool ProfileModel::remove(const QModelIndex& idx)
{
    QModelIndex realIdx = idx;

    // Helper to unwind the proxies
    while(realIdx.isValid() && realIdx.model() != this) {
        if (qobject_cast<const QAbstractProxyModel*>(realIdx.model()))
            realIdx = qobject_cast<const QAbstractProxyModel*>(realIdx.model())->mapToSource(realIdx);
        else {
            realIdx = QModelIndex();
            break;
        }
    }

    // Ensure it is a profile that can be removed
    if (!realIdx.isValid()) {
        qDebug() << "Failed to remove profile: invalid index";
        return false;
    }

    ProfileNode* n = static_cast<ProfileNode*>(realIdx.internalPointer());

    if (n->type != ProfileNode::Type::PROFILE) {
        qDebug() << "Failed to remove profile: It is not a profile"
                    << (int)n->type << n->m_pPerson
                    << realIdx.data(Qt::DisplayRole);
        return false;
    }

    if (n->children.size()) {
        qDebug() << "Failed to remove profile: It is in use";
        return false;
    }

    n->m_pPerson->remove();

    return true;
}

bool ProfileModelPrivate::addProfile(Person* person, const QString& name, CollectionInterface* col)
{
    if (!col) {
        qWarning() << "Can't add profile, no collection specified";
        return false;
    }

    if (!person) {
        person = new Person();
        QString profileName = name;
        if (profileName.isEmpty()) {
            profileName = tr("New profile");
        }
        person->setFormattedName(profileName);
    }

    col->editor<Person>()->addNew(person);

    return true;
}

/**
 * Create a new profile
 *
 * @param person an optional person to use for the vCard template
 */
bool ProfileModel::add(Person* person)
{
    if (collections(CollectionInterface::SupportedFeatures::ADD).size())
        return d_ptr->addProfile(person, QString(), collections(CollectionInterface::SupportedFeatures::ADD)[0]);
    return false;
}

/**
 * Create a new profile
 *
 * @param name The new profile name
 */
bool ProfileModel::add(const QString& name)
{
    if (collections(CollectionInterface::SupportedFeatures::ADD).size())
        return d_ptr->addProfile(nullptr, name, collections(CollectionInterface::SupportedFeatures::ADD)[0]);
    return false;
}

Person* ProfileModel::getProfile(const QModelIndex& idx) const
{
    if ((!idx.isValid()) || (idx.model() != this))
        return nullptr;

    const auto current = static_cast<ProfileNode*>(idx.internalPointer());
    switch (current->type) {
        case ProfileNode::Type::PROFILE:
            return current->m_pPerson;
        case ProfileNode::Type::ACCOUNT:
            return current->parent->m_pPerson;
        default:
            qWarning() << "Unknown node type to create mimedata";
            return nullptr;
    }
}

QItemSelectionModel* ProfileModel::getAccountSelectionModel(Account* a) const
{
    auto an = d_ptr->nodeForAccount(a);

    if (Q_UNLIKELY(!an)) {
        qWarning() << "Account not found";
        return nullptr;
    }

    Q_ASSERT(an->type == ProfileNode::Type::ACCOUNT);
    Q_ASSERT(an->parent);

    if (an->m_AccData.m_pSelectionModel)
        return an->m_AccData.m_pSelectionModel;

    an->m_AccData.m_pSelectionModel = new QItemSelectionModel(const_cast<ProfileModel*>(this));

    an->m_AccData.m_pSelectionModel->setCurrentIndex(
        index(an->parent->m_Index, 0),
        QItemSelectionModel::ClearAndSelect
    );

    connect(an->m_AccData.m_pSelectionModel, &QItemSelectionModel::currentChanged, a, [an, this](const QModelIndex& newIdx) {
        if (!newIdx.isValid())
            return;

        ProfileNode* profile = static_cast<ProfileNode*>(newIdx.internalPointer());

        if (profile->type != ProfileNode::Type::PROFILE)
            return;

        d_ptr->setProfile(an, profile);
    });

    return an->m_AccData.m_pSelectionModel;
}

#include "profilemodel.moc"
