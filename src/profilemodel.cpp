/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
 *   Copyright (C) 2017-2018 by Bluesystems                                 *
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
#include <QtCore/QTimer>
#include <QtCore/QMimeData>
#include <QtCore/QCoreApplication>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QItemSelectionModel>

//Ring
#include "accountmodel.h"
#include "collectioninterface.h"
#include "collectioneditor.h"
#include "callmodel.h"
#include "individual.h"
#include "contactmethod.h"
#include "person.h"
#include "private/vcardutils.h"
#include "mime.h"

struct AccountStruct final {
    Account*             m_pAccount        {nullptr};
    QItemSelectionModel* m_pSelectionModel {nullptr};
};

class AvailableProfileModel : public QSortFilterProxyModel
{
public:
    explicit AvailableProfileModel(ProfileModelPrivate* d);
protected:
    virtual bool filterAcceptsRow(int row, const QModelIndex & srcParent ) const override;
    virtual int rowCount(const QModelIndex& parent) const override;

private:
    mutable int m_RcCache {0};
    ProfileModelPrivate* d_ptr;
};

struct ProfileNode final
{
    explicit ProfileNode(): m_pPerson(nullptr) {}

    virtual ~ProfileNode() {
        QObject::disconnect(m_ChangedConn);
        qDeleteAll(children);
    }

    enum class Type : bool {
        PROFILE,
        ACCOUNT,
    };

    using Nodes = QVector<ProfileNode*>;

    Nodes        children {                            };
    ProfileNode* parent   {           nullptr          };
    Type         type     { ProfileNode::Type::ACCOUNT };
    int          m_Index  {              0             };
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
    explicit ProfileModelPrivate(ProfileModel* parent);
    QVector<ProfileNode*>  m_lProfiles                  {       };
    QItemSelectionModel*   m_pSelectionModel            {nullptr};
    QItemSelectionModel*   m_pSortedProxySelectionModel {nullptr};
    QSortFilterProxyModel* m_pSortedProxyModel          {nullptr};
    QSortFilterProxyModel* m_pAvailableProfileModel     {nullptr};
    ProfileNode* m_pDefaultProfile                      {nullptr};

    //Helpers
    void updateIndexes();
    inline Person* addProfile(Person* person, const QString& name, CollectionInterface* col);

    void _test_validate();

    void slotAccountAdded(Account* acc);
    void setProfile(ProfileNode* accNode, ProfileNode* proNode);
    ProfileNode* profileNodeById(const QByteArray& id) const;
    ProfileNode* nodeForAccount(const Account* a) const;
    QModelIndex mapToSource  (const QModelIndex& idx) const;
    QModelIndex mapFromSource(const QModelIndex& idx) const;
    ProfileNode* createNodeForAccount(Account* a);

    //Constants
    constexpr static const int c_OrderRole = 9999;

    ProfileModel* q_ptr;

public Q_SLOTS:
    void slotDataChanged(const QModelIndex& tl,const QModelIndex& br);
    void slotAccountRemoved(Account* a);
};

AvailableProfileModel::AvailableProfileModel(ProfileModelPrivate* d) :
  QSortFilterProxyModel(d->q_ptr), d_ptr(d)
{
    setObjectName("AvailableProfileModel");
    setSourceModel(d->q_ptr);
}

ProfileNode* ProfileModelPrivate::createNodeForAccount(Account* acc)
{
    auto account_pro     = new ProfileNode;
    account_pro->type    = ProfileNode::Type::ACCOUNT;
    account_pro->parent  = nullptr;
    account_pro->m_AccData.m_pSelectionModel = nullptr;
    account_pro->m_AccData.m_pAccount = acc;
    account_pro->m_Index = -1;

    return account_pro;
}

void ProfileModelPrivate::slotAccountAdded(Account* acc)
{
    if (nodeForAccount(acc))
        return;

    auto currentProfile = acc->profile();
    qDebug() << "Account added" << acc << currentProfile;

    if (Q_UNLIKELY(!m_pDefaultProfile)) {
        if (m_lProfiles.isEmpty()) {
            qWarning() << "Failed to set a profile: there is none";
        }
        else {
            qWarning() << "No profile selected: Assigning profiles at random";
            m_pDefaultProfile = m_lProfiles.first();
        }
    }

    // Add the profile when the account is created
    if (!currentProfile) {
        qWarning() << "No profile selected or none exists, fall back tot the default";

        m_pDefaultProfile->m_pPerson->addCustomField(
            VCardUtils::Property::X_RINGACCOUNT, acc->id()
        );
        m_pDefaultProfile->m_pPerson->save();

        currentProfile = m_pDefaultProfile->m_pPerson;
    }

    auto currentNode = profileNodeById(currentProfile->uid());

    if (!currentNode) {
        if (currentProfile && currentProfile->isPlaceHolder()) {
            qWarning() << "A profile was not found, using a random fallback" << acc << currentProfile;
        }
        else if (currentProfile && currentProfile->collection()) {
            const auto collections = q_ptr->collections(CollectionInterface::SupportedFeatures::ADD);
            if (collections.indexOf(currentProfile->collection()) == -1) {
                qWarning() << "An account is attached to a system contact instead of a profile";
                currentNode = profileNodeById(m_pDefaultProfile->m_pPerson->uid());
            }
        }
        else {
            qWarning() << "Account must have a profile parent, doing nothing" << acc << currentProfile;
            return;
        }
    }

    auto account_pro     = createNodeForAccount(acc);
    account_pro->m_Index = currentNode->children.size();
    account_pro->parent  = currentNode;

    const auto parentIdx = ProfileModel::instance().index(currentNode->m_Index,0);

    q_ptr->beginInsertRows(parentIdx, currentNode->children.size(), currentNode->children.size());
    currentNode->children << account_pro;
    q_ptr->endInsertRows();

    // Update the profile as it's availability could have chnaged
    emit q_ptr->dataChanged(parentIdx, parentIdx);

    _test_validate();
}

ProfileNode* ProfileModelPrivate::profileNodeById(const QByteArray& id) const
{
    for (auto p : qAsConst(m_lProfiles))
        if(p->m_pPerson->uid() == id)
            return p;
    return nullptr;
}

ProfileModel& ProfileModel::instance()
{
    static auto instance = new ProfileModel(QCoreApplication::instance());
    return *instance;
}

ProfileModelPrivate::ProfileModelPrivate(ProfileModel* parent) : QObject(parent), q_ptr(parent)
{}

ProfileModel::ProfileModel(QObject* parent) : QAbstractItemModel(parent),
CollectionManagerInterface<Person>(this), d_ptr(new ProfileModelPrivate(this))
{
    //Once LibRingClient is ready, start listening
    QTimer::singleShot(0,d_ptr,[this]() {
        connect(&AccountModel::instance(), &QAbstractItemModel::dataChanged  , d_ptr, &ProfileModelPrivate::slotDataChanged   );
        connect(&AccountModel::instance(), &AccountModel::accountRemoved     , d_ptr, &ProfileModelPrivate::slotAccountRemoved);
        connect(&AccountModel::instance(), &AccountModel::accountAdded       , d_ptr, &ProfileModelPrivate::slotAccountAdded);

        // Load existing accounts
        for (int i = 0; i < AccountModel::instance().rowCount(); i++)
            d_ptr->slotAccountAdded(AccountModel::instance()[i]);
    });
}

ProfileModel::~ProfileModel()
{
    qDeleteAll(d_ptr->m_lProfiles);
    delete d_ptr;
}

QHash<int,QByteArray> ProfileModel::roleNames() const
{
    return AccountModel::instance().roleNames();
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

    for (int i=tl.row(); i<=br.row();i++) {
        const auto idx = mapFromSource(tl);
        emit q_ptr->dataChanged(idx, idx);
    }

    // Also update the profile as the availability could have changed
    const auto profile = mapFromSource(tl).parent();

    if (profile.isValid())
        emit q_ptr->dataChanged(profile, profile);
}

void ProfileModelPrivate::slotAccountRemoved(Account* a)
{
    auto n = nodeForAccount(a);

    if (!n)
        return;

    Q_ASSERT(n->parent);

    const auto profIdx = q_ptr->index(n->parent->m_Index, 0);

    if (!profIdx.isValid())
        return;

    const int accIdx = n->m_Index;

    q_ptr->beginRemoveRows(profIdx, accIdx, accIdx);
    n->parent->children.removeAt(accIdx);
    for (int i = accIdx; i < n->parent->children.size(); i++)
        n->parent->children[i]->m_Index--;

    n->parent->m_pPerson->save();

    delete n;

    q_ptr->endRemoveRows();


    _test_validate();
}

ProfileNode* ProfileModelPrivate::nodeForAccount(const Account* a) const
{
    for (auto pro : qAsConst(m_lProfiles)) {
        for (auto accNode : qAsConst(pro->children)) {
            if (accNode->m_AccData.m_pAccount == a) {
                return accNode;
            }
        }
    }
    return nullptr;
}

QModelIndex ProfileModelPrivate::mapToSource(const QModelIndex& idx) const
{
    if (!idx.isValid() || !idx.parent().isValid() || idx.model() != q_ptr)
        return {};

    ProfileNode* profile = static_cast<ProfileNode*>(idx.parent().internalPointer());
    return profile->children[idx.row()]->m_AccData.m_pAccount->index();
}

QModelIndex ProfileModelPrivate::mapFromSource(const QModelIndex& idx) const
{
    if (!idx.isValid() || idx.model() != &AccountModel::instance())
        return {};

    auto acc = AccountModel::instance().getAccountByModelIndex(idx);
    auto accNode = nodeForAccount(acc);

    //Something is wrong, there is an orphan
    if (!accNode)
        return {};

    return q_ptr->index(
        accNode->m_Index, 0, q_ptr->index(accNode->parent->m_Index, 0)
    );
}

QVariant ProfileModel::data(const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    const auto currentNode = static_cast<ProfileNode*>(index.internalPointer());

    switch (currentNode->type) {
        case ProfileNode::Type::PROFILE:
            if (role == (int) Ring::Role::Object)
                return QVariant::fromValue(
                    currentNode->m_pPerson->individual().data()
                );
            else if (role == (int) Ring::Role::ObjectType)
                return QVariant::fromValue(Ring::ObjectType::Individual);

            return currentNode->m_pPerson->roleData(role);
        case ProfileNode::Type::ACCOUNT:
            switch(role) {
                case ProfileModelPrivate::c_OrderRole:
                    return currentNode->parent->m_Index;
                default:
                    return currentNode->m_AccData.m_pAccount->roleData(role);
            }
    }

   return {};
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
        return {};
    switch (current->type) {
        case ProfileNode::Type::PROFILE:
            return {};
        case ProfileNode::Type::ACCOUNT:
            return index(current->parent->m_Index, 0, {});
    }
    return {};
}

QModelIndex ProfileModel::index( int row, int column, const QModelIndex& parent ) const
{
    auto current = static_cast<ProfileNode*>(parent.internalPointer());

    if(parent.isValid() && current && !column && row >= 0 && row < current->children.size())
        return createIndex(row, 0, current->children[row]);
    else if(row < d_ptr->m_lProfiles.size() && row >= 0 && !column)
        return createIndex(row, 0, d_ptr->m_lProfiles[row]);

    return {};
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
    static QStringList mimes {
        RingMimes::PLAIN_TEXT, RingMimes::HTML_TEXT,
        RingMimes::ACCOUNT   , RingMimes::PROFILE  ,
    };

    return mimes;
}

QMimeData* ProfileModel::mimeData(const QModelIndexList &indexes) const
{
    auto mMimeData = new QMimeData();

    //FIXME: this won't work for multiple indexes
    for (const QModelIndex& index : qAsConst(indexes)) {
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

/// Return a list of profile with available accounts
bool AvailableProfileModel::filterAcceptsRow(int row, const QModelIndex& srcParent ) const
{
    if (srcParent.isValid())
        return false;

    if (row < 0 || row >= d_ptr->m_lProfiles.size())
        return false;

    const auto accs = d_ptr->m_lProfiles[row]->children;

    const auto iter = std::find_if(accs.constBegin(), accs.constEnd(), [](ProfileNode* node) {
        return node->m_AccData.m_pAccount->contactMethod()->isAvailable();
    });

    const bool found = iter != accs.constEnd();

    // Emit the signal when the property may have changed
    if ((!!m_RcCache) ^ found)
        emit d_ptr->q_ptr->hasAvailableProfileChanged();

    return found;
}

/// Catch the availability changes
int AvailableProfileModel::rowCount(const QModelIndex& parent) const
{
    if ((!!m_RcCache) ^ !!(m_RcCache = QSortFilterProxyModel::rowCount(parent)))
        emit d_ptr->q_ptr->hasAvailableProfileChanged();

    return parent.isValid() ? 0 : m_RcCache;
}

QItemSelectionModel* ProfileModel::selectionModel() const
{
    if (!d_ptr->m_pSelectionModel) {
        d_ptr->m_pSelectionModel = new QItemSelectionModel(const_cast<ProfileModel*>(this));

        connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& i) {
            const auto accIdx = d_ptr->mapToSource(i);
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
            const auto accIdx = d_ptr->mapToSource(
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

QAbstractItemModel* ProfileModel::availableProfileModel() const
{
    if (!d_ptr->m_pAvailableProfileModel)
        d_ptr->m_pAvailableProfileModel = new AvailableProfileModel(d_ptr);

    return d_ptr->m_pAvailableProfileModel;
}

bool ProfileModel::hasAvailableProfiles() const
{
    return availableProfileModel()->rowCount();
}

bool ProfileModel::setProfile(Account* a, Person* p)
{
    if (!p)
        return false;

    if (p->uid().isEmpty())
        p->ensureUid();

    if (!p->collection()) {
        const auto cols = collections(CollectionInterface::SupportedFeatures::ADD);

        if (cols.isEmpty())
            return false;

        cols.first()->editor<Person>()->addNew(p);
    }

    Q_ASSERT(!p->uid().isEmpty());

    auto accNode = d_ptr->nodeForAccount(a);
    auto proNode = d_ptr->profileNodeById(p->uid());

    if (!accNode)
        accNode = d_ptr->createNodeForAccount(a);

    Q_ASSERT(accNode);

    if (Q_UNLIKELY(!proNode)) {
        qWarning() << "Cannot set a profile to account" << a << " because it doesn't exist";
        return false;
    }

    Q_ASSERT(proNode);

    Q_ASSERT(a->contactMethod()->isSelf());
    a->contactMethod()->setPerson(p);

    d_ptr->setProfile(accNode, proNode);
}

void ProfileModelPrivate::setProfile(ProfileNode* accNode, ProfileNode* proNode)
{
    Q_ASSERT(accNode->type == ProfileNode::Type::ACCOUNT);
    Q_ASSERT(proNode->type == ProfileNode::Type::PROFILE );

    auto oldProfile = accNode->parent;

    const auto oldParentIdx = oldProfile ? q_ptr->index(oldProfile->m_Index, 0) : QModelIndex();

    const auto newParentIdx = q_ptr->index(proNode->m_Index, 0);

    if (!oldParentIdx.isValid()) {
        accNode->m_Index = proNode->m_Index = 0;
        q_ptr->beginInsertRows(newParentIdx, accNode->m_Index, accNode->m_Index);
    }
    else if(!q_ptr->beginMoveRows(oldParentIdx, accNode->m_Index, accNode->m_Index, newParentIdx, 0))
        return;

    Q_ASSERT((!oldProfile) || accNode == oldProfile->children.at(accNode->m_Index));

    qDebug() << "Moving:" << accNode->m_AccData.m_pAccount->alias();

    const bool ret = oldProfile ? oldProfile->m_pPerson->removeCustomField(
        VCardUtils::Property::X_RINGACCOUNT,
        accNode->m_AccData.m_pAccount->id()
    ) : true;

    if (Q_UNLIKELY(!ret)) {
        qWarning() << "Removing from the old profile failed, ignoring";
    }

    if (oldProfile)
        oldProfile->children.remove(accNode->m_Index);

    accNode->parent = proNode;
    proNode->children.insert(0, accNode);

    updateIndexes();

    proNode->m_pPerson->addCustomField(
        VCardUtils::Property::X_RINGACCOUNT,
        accNode->m_AccData.m_pAccount->id()
    );

    proNode->m_pPerson->save();

    if (oldProfile)
        oldProfile->m_pPerson->save();

    if (oldProfile)
        q_ptr->endMoveRows();
    else
        q_ptr->endInsertRows();
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

    if(Q_UNLIKELY(((!profileIdx.isValid()) && row < 0) || column > 0))
        return false;

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

        auto accountToMove = d_ptr->nodeForAccount(acc);

        d_ptr->setProfile(accountToMove, newProfile);
    }
    else if (data->hasFormat(RingMimes::PROFILE)) {
        qDebug() << "Dropping profile on row" << row;

        const int destinationRow = row > 0 ?
            d_ptr->m_lProfiles.size() : row;

        auto moving = d_ptr->profileNodeById(data->data(RingMimes::PROFILE));

        if(!moving)
            return false;

        if(!beginMoveRows(QModelIndex(), moving->m_Index, moving->m_Index, QModelIndex(), destinationRow))
            return false;

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

    auto current = static_cast<ProfileNode*>(index.internalPointer());

    if (!current->parent)
        return false;

    return AccountModel::instance().setData(d_ptr->mapToSource(index),value,role);
}

QVariant ProfileModel::headerData(int section, Qt::Orientation orientation, int role ) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)

    if (role == Qt::DisplayRole)
        return tr("Profiles");

    return {};
}
/*
void ProfileModel::collectionAddedCallback(CollectionInterface* backend)
{
   Q_UNUSED(backend)
}*/

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
        if (auto a = AccountModel::instance().getById(accId))
            a->setProfile(const_cast<Person*>(pro));
    }

    selectionModel()->setCurrentIndex(index(proNode->m_Index, 0), QItemSelectionModel::ClearAndSelect);

    proNode->m_ChangedConn = connect(pro, &Person::changed, this, [this, proNode]() {
        if (proNode->m_pPerson->isActive()) {
            const QModelIndex idx = index(proNode->m_Index, 0);
            emit dataChanged(idx, idx);
        }
    });

    d_ptr->_test_validate();

    return true;
}

bool ProfileModel::removeItemCallback(const Person* item)
{
    int nodeIdx = -1;

    auto profile = d_ptr->profileNodeById(item->uid());

    if (!profile)
        return false;

    // Remove the profile
    beginRemoveRows({}, profile->m_Index, profile->m_Index);
    d_ptr->m_lProfiles.removeAt(nodeIdx);
    d_ptr->updateIndexes();
    delete profile;
    endRemoveRows();

    d_ptr->_test_validate();

    return true;
}

/**
 * Remove an unused profile
 *
 * @return If removing the profile has been successful
 */
bool ProfileModel::remove(const QModelIndex& idx)
{
    auto realIdx = idx;

    // Helper to unwind the proxies
    while(realIdx.isValid() && realIdx.model() != this) {
        if (auto m = qobject_cast<const QAbstractProxyModel*>(realIdx.model()))
            realIdx = m->mapToSource(realIdx);
    }

    // Ensure it is a profile that can be removed
    if (!realIdx.isValid()) {
        qDebug() << "Failed to remove profile: invalid index";
        return false;
    }

    auto n = static_cast<ProfileNode*>(realIdx.internalPointer());

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

    return n->m_pPerson->remove();
}

Person* ProfileModelPrivate::addProfile(Person* person, const QString& name, CollectionInterface* col)
{
    if (!col) {
        qWarning() << "Can't add profile, no collection specified";
        return nullptr;
    }

    if (!person) {
        person = new Person();

        person->setFormattedName(
            name.isEmpty() ? tr("New profile") : name
        );
    }

    col->editor<Person>()->addNew(person);

    return person;
}

void ProfileModelPrivate::_test_validate()
{
#ifdef ENABLE_TEST_ASSERTS
    for (auto prof : qAsConst(m_lProfiles)) {
        Q_ASSERT(prof->m_pPerson);

        for (auto acc : qAsConst(prof->children)) {
            Q_ASSERT(acc->m_AccData.m_pAccount);
            Q_ASSERT(acc->m_AccData.m_pAccount->contactMethod());
            Q_ASSERT(acc->m_AccData.m_pAccount->contactMethod()->contact());
            Q_ASSERT(acc->m_AccData.m_pAccount->contactMethod()->contact()->isProfile());
            Q_ASSERT(acc->m_AccData.m_pAccount->contactMethod()->contact() == prof->m_pPerson);
        }

        // It isn't a profile *yet* if there is no associated accounts
        Q_ASSERT(prof->m_pPerson->isProfile() || prof->children.isEmpty());
        Q_ASSERT(prof->m_pPerson->isTracked() || prof->children.isEmpty());
    }
#endif
}

/**
 * Create a new profile
 *
 * @param person an optional person to use for the vCard template
 */
bool ProfileModel::add(Person* person)
{
    const auto cols = collections(CollectionInterface::SupportedFeatures::ADD);
    return cols.isEmpty() ? false : d_ptr->addProfile(person, {}, cols.first()) != nullptr;
}

/**
 * Create a new profile
 *
 * @param name The new profile name
 */
Person* ProfileModel::add(const QString& name)
{
    const auto cols = collections(CollectionInterface::SupportedFeatures::ADD);
    return cols.isEmpty() ? nullptr : d_ptr->addProfile(nullptr, name, cols.first());
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

Account* ProfileModel::getAccount(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    const auto n = static_cast<ProfileNode*>(idx.internalPointer());

    return n->type == ProfileNode::Type::ACCOUNT ?
        n->m_AccData.m_pAccount : nullptr;
}

QModelIndex ProfileModel::accountIndex(Account* a) const
{
    if (auto n = d_ptr->nodeForAccount(a))
        return index(n->m_Index, 0, index(n->parent->m_Index, 0));

    return {};
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

        auto profile = static_cast<ProfileNode*>(newIdx.internalPointer());

        if (profile->type != ProfileNode::Type::PROFILE)
            return;

        d_ptr->setProfile(an, profile);
    });

    return an->m_AccData.m_pSelectionModel;
}

#include "profilemodel.moc"
