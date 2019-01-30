/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
 *   Copyright (C) 2018 by Bluesystems                                      *
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
#include "notificationstatemodel_p.h"

//Qt
#include <QtCore/QMimeData>
#include <QtCore/QCoreApplication>
#include <QtCore/QAbstractItemModel>

//Ring
#include "individualdirectory.h"
#include "contactmethod.h"
#include "callmodel.h"
#include "mime.h"
#include "session.h"
#include "picocms/collectioneditor.h"
#include "picocms/collectioninterface.h"
#include "private/individualdirectory_p.h"

//Model item/index
class NotificationStateNode final
{
public:
    //Constructor
    NotificationStateNode(ContactMethod* n) : m_pNumber(n) {}
    ~NotificationStateNode();

    //Attributes
    ContactMethod*          m_pNumber {nullptr};
    int                     m_Index   {  -1   };
    QMetaObject::Connection m_ChConn  {       };
};

class NotificationStateModelPrivate final
{
public:
    //Attributes
    QVector<NotificationStateNode*> m_lNodes;
    QHash<void*, NotificationStateNode*> m_hTracked;
};

NotificationStateNode::~NotificationStateNode()
{
    QObject::disconnect(m_ChConn);
}

NotificationStateModel::NotificationStateModel(QObject* parent) : QAbstractListModel(parent), CollectionManagerInterface<ContactMethod>(this),
d_ptr(new NotificationStateModelPrivate())
{
    setObjectName(QStringLiteral("NotificationStateModel"));
}

NotificationStateModel::~NotificationStateModel()
{
    qDeleteAll(d_ptr->m_lNodes);
    delete d_ptr;
}

QHash<int,QByteArray> NotificationStateModel::roleNames() const
{
    return Session::instance()->individualDirectory()->roleNames();
}

NotificationStateModel* NotificationStateModel::instance()
{
    static NotificationStateModel i(QCoreApplication::instance());
    return &i;
}

QVariant NotificationStateModel::data( const QModelIndex& index, int role) const
{
    if ((!index.isValid()))
        return {};

    return d_ptr->m_lNodes[index.row()]->m_pNumber->roleData(role == Qt::DisplayRole ?
        (int)Ring::Role::Name : role);
}

///Get the number of child of "parent"
int NotificationStateModel::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : d_ptr->m_lNodes.size();
}

///Get the index
QModelIndex NotificationStateModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid() || column != 0 || row < 0 || row >= d_ptr->m_lNodes.size())
        return {};

    return createIndex(row,column, d_ptr->m_lNodes[row]);
}

void NotificationStateModel::setState(ContactMethod* number, ContactMethod::NotificationPolicy p)
{
    if (collections().isEmpty()) {
        qWarning() << "No notification state collection";
        return;
    }

    auto col = collections().first();

    // Return
    if (p == ContactMethod::NotificationPolicy::DEFAULT) {
        if (!col->editor<ContactMethod>()->contains(number))
            return;

        col->editor<ContactMethod>()->remove(number);

        return;
    }

    if (col->editor<ContactMethod>()->contains(number))
        return;

    col->editor<ContactMethod>()->addNew(number);
}

void NotificationStateModel::clear()
{
    collections().first()->clear();
}

bool NotificationStateModel::addItemCallback(const ContactMethod* bookmark)
{
    // Contact methods can be duplicates of each other, make sure to handle them
    if ((!bookmark) || d_ptr->m_hTracked.contains(bookmark->d()) || bookmark->isSelf())
        return true;

    auto bm = new NotificationStateNode(const_cast<ContactMethod*>(bookmark));

    bm->m_Index   = d_ptr->m_lNodes.size();
    bm->m_ChConn  = connect(bookmark, &ContactMethod::changed, bookmark, [this,bm]() {
        auto idx = index(bm->m_Index, 0);
        emit dataChanged(idx,idx);
    });

    beginInsertRows({}, bm->m_Index, bm->m_Index);
    d_ptr->m_lNodes << bm;
    endInsertRows();

    return true;
}

bool NotificationStateModel::removeItemCallback(const ContactMethod* cm)
{
    auto item = d_ptr->m_hTracked.value(cm->d());

    if (!item)
        return false;

    beginRemoveRows({}, item->m_Index, item->m_Index);

    d_ptr->m_lNodes.remove(item->m_Index);

    for (int i = item->m_Index; i < d_ptr->m_lNodes.size(); i++)
        d_ptr->m_lNodes[i]->m_Index--;

    endRemoveRows();

    delete item;

    return true;
}

void NotificationStateModel::collectionAddedCallback(CollectionInterface* backend)
{
    Q_UNUSED(backend)
}
