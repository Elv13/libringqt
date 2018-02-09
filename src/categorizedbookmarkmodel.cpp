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
#include "categorizedbookmarkmodel.h"

//Qt
#include <QtCore/QMimeData>
#include <QtCore/QCoreApplication>
#include <QtCore/QAbstractItemModel>

//Ring
#include "categorizedhistorymodel.h"
#include "dbus/presencemanager.h"
#include "phonedirectorymodel.h"
#include "contactmethod.h"
#include "callmodel.h"
#include "call.h"
#include "person.h"
#include "uri.h"
#include "mime.h"
#include "collectioneditor.h"
#include "collectioninterface.h"
#include "private/phonedirectorymodel_p.h"

class BookmarkNode;

class CategorizedBookmarkModelPrivate final : public QObject
{
    Q_OBJECT
public:
    CategorizedBookmarkModelPrivate(CategorizedBookmarkModel* parent);

    //Attributes
    QVector<BookmarkNode*>                     m_lCategoryCounter ;
    QHash<QString,BookmarkNode*>               m_hCategories      ;
    QHash<const ContactMethod*, BookmarkNode*> m_hTracked         ;
    bool                                       m_DisplayPopular {false};

    //Helpers
    QString                 category    ( BookmarkNode* number   ) const;
    QVector<ContactMethod*> bookmarkList(                        ) const;
    ContactMethod*          getNumber   ( const QModelIndex& idx ) const;

public Q_SLOTS:
    void slotIndexChanged( const QModelIndex& idx );
    void reloadCategories();

private:
    CategorizedBookmarkModel* q_ptr;
};

//Model item/index
class BookmarkNode final
{
    friend class CategorizedBookmarkModel;
public:
    enum class Type {
        BOOKMARK,
        CATEGORY,
    };

    //Constructor
    explicit BookmarkNode(const QString& name);
    BookmarkNode(ContactMethod* number);
    ~BookmarkNode();

    //Attributes
    ContactMethod*          m_pNumber    {nullptr};
    BookmarkNode*           m_pParent    {nullptr};
    int                     m_Index      {  -1   };
    Type                    m_Type       {       };
    bool                    m_MostPopular{ false };
    QString                 m_Name       {       };
    QVector<BookmarkNode*>  m_lChildren  {       };
    QMetaObject::Connection m_ChConn     {       };
    QMetaObject::Connection m_NameConn   {       };
};

CategorizedBookmarkModelPrivate::CategorizedBookmarkModelPrivate(CategorizedBookmarkModel* parent) :
QObject(parent), q_ptr(parent)
{}

BookmarkNode::BookmarkNode(ContactMethod* number):
m_pNumber(number), m_Type(BookmarkNode::Type::BOOKMARK)
{
    Q_ASSERT(number != nullptr);
}

BookmarkNode::BookmarkNode(const QString& name)
   : m_Type(BookmarkNode::Type::CATEGORY),m_Name(name)
{}

BookmarkNode::~BookmarkNode()
{
    QObject::disconnect(m_ChConn);
    QObject::disconnect(m_NameConn);
}

CategorizedBookmarkModel::CategorizedBookmarkModel(QObject* parent) : QAbstractItemModel(parent), CollectionManagerInterface<ContactMethod>(this),
d_ptr(new CategorizedBookmarkModelPrivate(this))
{
    setObjectName(QStringLiteral("CategorizedBookmarkModel"));
    d_ptr->reloadCategories();

    if (displayMostPopular()) {
        connect(PhoneDirectoryModel::instance().mostPopularNumberModel(),
            &QAbstractItemModel::rowsInserted,
            d_ptr,&CategorizedBookmarkModelPrivate::reloadCategories
        );
    }
}

CategorizedBookmarkModel::~CategorizedBookmarkModel()
{
    for (auto item : qAsConst(d_ptr->m_lCategoryCounter)) {
        for (auto child : qAsConst(item->m_lChildren))
            delete child;

        delete item;
    }
    delete d_ptr;
}

CategorizedBookmarkModel& CategorizedBookmarkModel::instance()
{
    static auto instance = new CategorizedBookmarkModel(QCoreApplication::instance());
    return *instance;
}

QHash<int,QByteArray> CategorizedBookmarkModel::roleNames() const
{
    return PhoneDirectoryModel::instance().roleNames();
}

///Reload bookmark cateogries
void CategorizedBookmarkModelPrivate::reloadCategories()
{
    m_hCategories.clear();

    q_ptr->beginRemoveRows({}, 0, m_lCategoryCounter.size()-1);

    //TODO this is not efficient, nor necessary
    for (auto item : qAsConst(m_lCategoryCounter)) {
        for (auto child : qAsConst(item->m_lChildren)) {
            delete child;
        }
        delete item;
    }
    m_hTracked.clear();
    m_lCategoryCounter.clear();

    q_ptr->endRemoveRows();

    //Load most used contacts
    if (q_ptr->displayMostPopular()) {
        auto item = new BookmarkNode(tr("Most popular"));
        const int catSize = m_lCategoryCounter.size();

        m_hCategories[QStringLiteral("mp")] = item;
        item->m_Index       = catSize;
        item->m_MostPopular = true;

        q_ptr->beginInsertRows({}, catSize, catSize);
        m_lCategoryCounter << item;
        q_ptr->endInsertRows();
    }

    const auto bookmarks = bookmarkList();

    for (auto bookmark : qAsConst(bookmarks)) {
        auto bm = new BookmarkNode(bookmark);
        const QString val = category(bm);

        auto category = m_hCategories.value(val);

        if (!category) {
            category = new BookmarkNode(val);
            const int catSize = m_lCategoryCounter.size();
            m_hCategories[val] = category;

            category->m_Index = catSize;

            q_ptr->beginInsertRows({}, catSize, catSize);
            m_lCategoryCounter << category;
            q_ptr->endInsertRows();
        }

        bookmark->setBookmarked(true);
        bm->m_pParent = category;
        bm->m_Index   = category->m_lChildren.size();
        bm->m_ChConn  = connect(bookmark, &ContactMethod::changed, bookmark, [this,bm]() {
            slotIndexChanged(
                q_ptr->index(bm->m_Index, 0, q_ptr->index(bm->m_pParent->m_Index,0))
            );
        });

        q_ptr->beginInsertRows(q_ptr->index(category->m_Index,0), category->m_lChildren.size(), category->m_lChildren.size());
        category->m_lChildren << bm;
        q_ptr->endInsertRows();

        if (!m_hTracked.contains(bookmark)) {
            const QString displayName = bm->m_pNumber->roleData(Qt::DisplayRole).toString();

            category->m_NameConn = connect(bookmark, &ContactMethod::primaryNameChanged, bookmark, [this,displayName,bm]() {
                //If a contact arrive later, reload
                if (displayName != bm->m_pNumber->roleData(Qt::DisplayRole))
                    reloadCategories();
            });

            m_hTracked[bookmark] = bm;
        }
    }


    emit q_ptr->layoutAboutToBeChanged();
    emit q_ptr->layoutChanged();
}

QVariant CategorizedBookmarkModel::data( const QModelIndex& index, int role) const
{
    if ((!index.isValid()))
        return {};

    if (index.parent().isValid()) {
        const auto parentItem = static_cast<BookmarkNode*>(index.parent().internalPointer());

        if (parentItem->m_MostPopular) {
            const auto mpIdx = PhoneDirectoryModel::instance()
                .mostPopularNumberModel()->index(index.row(),0);

            return PhoneDirectoryModel::instance().mostPopularNumberModel()->data(
                mpIdx,
                role
            );
        }
    }

    const auto modelItem = static_cast<BookmarkNode*>(index.internalPointer());

    if (!modelItem)
        return {};

    switch (modelItem->m_Type) {
        case BookmarkNode::Type::CATEGORY:
            switch (role) {
                case Qt::DisplayRole:
                    return modelItem->m_Name;
                case static_cast<int>(Call::Role::Name):
                    //Make sure it is at the top of the bookmarks when sorted
                    return modelItem->m_MostPopular ? "000000" : modelItem->m_Name;
            }
            break;
        case BookmarkNode::Type::BOOKMARK:
            return modelItem->m_pNumber->roleData(role == Qt::DisplayRole ?
                (int)Call::Role::Name : role);
            break;
    };

    return {};
}

///Get header data
QVariant CategorizedBookmarkModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QVariant(tr("Contacts"));

    return {};
}

///Get the number of child of "parent"
int CategorizedBookmarkModel::rowCount( const QModelIndex& parent ) const
{
    if (!parent.isValid())
        return d_ptr->m_lCategoryCounter.size();

    const auto modelItem = static_cast<BookmarkNode*>(parent.internalPointer());

    //Is from MostPopularModel
    if (!modelItem)
        return 0;

    switch (modelItem->m_Type) {
        case BookmarkNode::Type::CATEGORY:
            if (modelItem->m_MostPopular) {
                static auto& index = PhoneDirectoryModel::instance().d_ptr->m_lPopularityIndex;
                return index.size();
            }
            else
                return modelItem->m_lChildren.size();
        case BookmarkNode::Type::BOOKMARK:
            return 0;
    }
    return 0;
}

Qt::ItemFlags CategorizedBookmarkModel::flags( const QModelIndex& index ) const
{
    if (!index.isValid())
        return 0;

    return index.isValid() ? (
        Qt::ItemIsEnabled    |
        Qt::ItemIsSelectable |
        (index.parent().isValid()?Qt::ItemIsDragEnabled|Qt::ItemIsDropEnabled:Qt::ItemIsEnabled)
    ) : Qt::NoItemFlags;
}

///There is only 1 column
int CategorizedBookmarkModel::columnCount ( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

///Get the bookmark parent
QModelIndex CategorizedBookmarkModel::parent( const QModelIndex& idx) const
{
    if (!idx.isValid())
        return {};

    const auto modelItem = static_cast<BookmarkNode*>(idx.internalPointer());

    if ((!modelItem) && d_ptr->m_hCategories.contains(QStringLiteral("mp")))
        return index(d_ptr->m_hCategories[QStringLiteral("mp")]->m_Index,0);

    switch(modelItem->m_Type) {
        case BookmarkNode::Type::BOOKMARK:
            return index(modelItem->m_pParent->m_Index,0);
        case BookmarkNode::Type::CATEGORY:
            return {};
    }

    return {};
}

///Get the index
QModelIndex CategorizedBookmarkModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0)
        return {};

    if (parent.isValid() && d_ptr->m_lCategoryCounter.size() > parent.row()) {
        const auto modelItem = static_cast<BookmarkNode*>(parent.internalPointer());

        if (modelItem->m_MostPopular)
            return createIndex(row, column, nullptr);

        if (modelItem->m_lChildren.size() <= row)
            return {};

        return createIndex(row,column, modelItem->m_lChildren[row]);
    }
    else if (row >= 0 && row < d_ptr->m_lCategoryCounter.size())
        return createIndex(row,column, d_ptr->m_lCategoryCounter[row]);

    return {};
}

///Get bookmarks mime types
QStringList CategorizedBookmarkModel::mimeTypes() const
{
    static const QStringList mimes {
        RingMimes::PLAIN_TEXT,
        RingMimes::PHONENUMBER
    };

    return mimes;
}

///Generate mime data
QMimeData* CategorizedBookmarkModel::mimeData(const QModelIndexList &indexes) const
{
    auto mimeData = new QMimeData();

    for (const auto& index : qAsConst(indexes)) {
        if (index.isValid()) {
            QString text = data(index, static_cast<int>(Call::Role::Number)).toString();
            mimeData->setData(RingMimes::PLAIN_TEXT , text.toUtf8());
            mimeData->setData(RingMimes::PHONENUMBER, text.toUtf8());
            return mimeData;
        }
    }

    return mimeData;
}

///Return valid payload types
int CategorizedBookmarkModel::acceptedPayloadTypes()
{
    return CallModel::DropPayloadType::CALL;
}

///Get category
QString CategorizedBookmarkModelPrivate::category(BookmarkNode* number) const
{
    if (number->m_Name.size())
        return number->m_Name;

    QString cat = number->m_pNumber->roleData(Qt::DisplayRole).toString();

    if (cat.size())
        cat = cat[0].toUpper();

    return cat;
}

bool CategorizedBookmarkModel::displayMostPopular() const
{
    return d_ptr->m_DisplayPopular;
}

void CategorizedBookmarkModel::setDisplayPopular(bool value)
{
    d_ptr->m_DisplayPopular = value;
    d_ptr->reloadCategories();
}

QVector<ContactMethod*> CategorizedBookmarkModelPrivate::bookmarkList() const
{
    if (q_ptr->collections().isEmpty())
        return {};

    return q_ptr->collections().first()->items<ContactMethod>();
}

void CategorizedBookmarkModel::addBookmark(ContactMethod* number)
{
    if (collections().isEmpty()) {
        qWarning() << "No bookmark backend is set";
        Q_ASSERT(false);
        return;
    }

    if (collections().first()->editor<ContactMethod>()->contains(number))
        return;

    collections().first()->editor<ContactMethod>()->addNew(number);
}

void CategorizedBookmarkModel::removeBookmark(ContactMethod* number)
{
    if (collections().isEmpty()) {
        qWarning() << "No bookmark backend is set";
        return;
    }

    if (!collections().first()->editor<ContactMethod>()->contains(number))
        return;

    collections().first()->editor<ContactMethod>()->remove(number);
}

void CategorizedBookmarkModel::remove(const QModelIndex& idx)
{
    collections().first()->editor<ContactMethod>()->remove(d_ptr->getNumber(idx));
}

void CategorizedBookmarkModel::clear()
{
    collections().first()->clear();
}

ContactMethod* CategorizedBookmarkModelPrivate::getNumber(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    if (idx.parent().isValid() || idx.parent().row() < m_lCategoryCounter.size())
        return nullptr;

    return m_lCategoryCounter[idx.parent().row()]
        ->m_lChildren[idx.row()]->m_pNumber;
}

///Callback when an item change
void CategorizedBookmarkModelPrivate::slotIndexChanged(const QModelIndex& idx)
{
    emit q_ptr->dataChanged(idx,idx);
}

bool CategorizedBookmarkModel::addItemCallback(const ContactMethod* item)
{
    Q_UNUSED(item)

    if (d_ptr->m_hTracked.contains(item))
        return true;

    d_ptr->reloadCategories(); //TODO this is far from optimal

    return true;
}

bool CategorizedBookmarkModel::removeItemCallback(const ContactMethod* cm)
{
    auto item = d_ptr->m_hTracked.value(cm);

    if (!item)
        return false;

    Q_ASSERT(item->m_Type == BookmarkNode::Type::BOOKMARK);
    Q_ASSERT(item->m_pParent);
    Q_ASSERT(item->m_pParent->m_Type == BookmarkNode::Type::CATEGORY);

    const auto catIdx = index(item->m_pParent->m_Index, 0);

    Q_ASSERT(catIdx.isValid());
    Q_ASSERT(item->m_pParent->m_lChildren[item->m_Index] == item);

    beginRemoveRows(catIdx, item->m_Index, item->m_Index);

    item->m_pParent->m_lChildren.remove(item->m_Index);

    for (int i = item->m_Index; i < item->m_pParent->m_lChildren.size(); i++)
        item->m_pParent->m_lChildren[i]->m_Index--;

    endRemoveRows();

    // Also remove empty categories
    if (item->m_pParent->m_lChildren.isEmpty()) {
        Q_ASSERT(d_ptr->m_lCategoryCounter.size() > item->m_pParent->m_Index);

        beginRemoveRows({}, item->m_pParent->m_Index, item->m_pParent->m_Index);

        d_ptr->m_lCategoryCounter.remove(item->m_pParent->m_Index);

        for (int i = item->m_pParent->m_Index; i < d_ptr->m_lCategoryCounter.size(); i++)
            d_ptr->m_lCategoryCounter[i]->m_Index--;

        d_ptr->m_hCategories.remove(item->m_pParent->m_Name);

        delete item->m_pParent;

        endRemoveRows();
    }

    delete item;

    d_ptr->m_hTracked.remove(cm);

    return true;
}

void CategorizedBookmarkModel::collectionAddedCallback(CollectionInterface* backend)
{
    Q_UNUSED(backend)
    d_ptr->reloadCategories();
}

#include <categorizedbookmarkmodel.moc>
