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
#include "bookmarkmodel.h"

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
#include "collectioneditor.h"
#include "collectioninterface.h"
#include "private/individualdirectory_p.h"

class BookmarkNode;

class BookmarkModelPrivate final : public QObject
{
    Q_OBJECT
public:
    BookmarkModelPrivate(BookmarkModel* parent);

    //Attributes
    QVector<BookmarkNode*>       m_lCategoryCounter ;
    QHash<QString,BookmarkNode*> m_hCategories      ;
    bool                         m_DisplayPopular {false};

    QHash<const ContactMethodPrivate*, BookmarkNode*> m_hTracked;

    //Helpers
    QVector<ContactMethod*> bookmarkList(                  ) const;
    BookmarkNode*           getCategory ( BookmarkNode* bm );
    void                    clear       (                  );

public Q_SLOTS:
    void slotIndexChanged( const QModelIndex& idx );
    void reloadCategories();

private:
    BookmarkModel* q_ptr;
};

//Model item/index
class BookmarkNode final
{
    friend class BookmarkModel;
public:
    enum class Type {
        BOOKMARK,
        CATEGORY,
    };

    //Constructor
    explicit BookmarkNode(const QString& name);
    BookmarkNode(const ContactMethod* number);
    ~BookmarkNode();

    //Attributes
    const ContactMethod*    m_pNumber    {nullptr};
    BookmarkNode*           m_pParent    {nullptr};
    int                     m_Index      {  -1   };
    Type                    m_Type       {       };
    bool                    m_MostPopular{ false };
    QString                 m_Name       {       };
    QVector<BookmarkNode*>  m_lChildren  {       };
    QMetaObject::Connection m_ChConn     {       };
    QMetaObject::Connection m_NameConn   {       };
    QMetaObject::Connection m_RemConn    {       };
};

BookmarkModelPrivate::BookmarkModelPrivate(BookmarkModel* parent) :
QObject(parent), q_ptr(parent)
{}

BookmarkNode::BookmarkNode(const ContactMethod* number)
    : m_pNumber(number), m_Type(BookmarkNode::Type::BOOKMARK)
{}

BookmarkNode::BookmarkNode(const QString& name)
    : m_Type(BookmarkNode::Type::CATEGORY),m_Name(name)
{}

BookmarkNode::~BookmarkNode()
{
    qDeleteAll(m_lChildren);
    QObject::disconnect(m_ChConn);
    QObject::disconnect(m_NameConn);
    QObject::disconnect(m_RemConn);
}

BookmarkModel::BookmarkModel(QObject* parent) : QAbstractItemModel(parent), CollectionManagerInterface<ContactMethod>(this),
d_ptr(new BookmarkModelPrivate(this))
{
    setObjectName(QStringLiteral("BookmarkModel"));
    d_ptr->reloadCategories();
}

void BookmarkModelPrivate::clear()
{
    q_ptr->beginResetModel();

    m_hCategories.clear();

    qDeleteAll(m_lCategoryCounter);

    m_hTracked.clear();
    m_lCategoryCounter.clear();

    q_ptr->endResetModel();
}

BookmarkModel::~BookmarkModel()
{
    d_ptr->clear();
    delete d_ptr;
}

QHash<int,QByteArray> BookmarkModel::roleNames() const
{
    return Session::instance()->individualDirectory()->roleNames();
}

BookmarkNode* BookmarkModelPrivate::getCategory(BookmarkNode* bm)
{
    const QString val = bm->m_pNumber->bestName().left(1).toUpper();

    if (auto category = m_hCategories.value(val))
        return category;

    auto cat = new BookmarkNode(val);
    const int catSize = m_lCategoryCounter.size();
    m_hCategories[val] = cat;

    cat->m_Index = catSize;

    q_ptr->beginInsertRows({}, catSize, catSize);
    m_lCategoryCounter << cat;
    q_ptr->endInsertRows();

    return cat;
}

///Reload bookmark cateogries
void BookmarkModelPrivate::reloadCategories()
{
    clear();

    //Load most used contacts
    if (q_ptr->displayMostPopular()) {
        auto item = new BookmarkNode(tr("Most popular"));

        m_hCategories[QStringLiteral("mp")] = item;
        item->m_Index       = 0;
        item->m_MostPopular = true;

        q_ptr->beginInsertRows({}, 0, 0);
        m_lCategoryCounter << item;
        q_ptr->endInsertRows();
    }

    const auto bookmarks = bookmarkList();

    for (auto bookmark : qAsConst(bookmarks))
        q_ptr->addItemCallback(bookmark);
}

QVariant BookmarkModel::data( const QModelIndex& index, int role) const
{
    if ((!index.isValid()))
        return {};

    if (index.parent().isValid()) {
        const auto parentItem = static_cast<BookmarkNode*>(index.parent().internalPointer());

        if (parentItem->m_MostPopular) {
            const auto mpIdx = Session::instance()->individualDirectory()->
                mostPopularNumberModel()->index(index.row(),0);

            return Session::instance()->individualDirectory()->mostPopularNumberModel()->data(
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
                case static_cast<int>(Ring::Role::Name):
                    //Make sure it is at the top of the bookmarks when sorted
                    return modelItem->m_MostPopular ? "000000" : modelItem->m_Name;
            }
            break;
        case BookmarkNode::Type::BOOKMARK:
            return modelItem->m_pNumber->roleData(role == Qt::DisplayRole ?
                (int)Ring::Role::Name : role);
            break;
    };

    return {};
}

///Get header data
QVariant BookmarkModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)

    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return QVariant(tr("Contacts"));

    return {};
}

///Get the number of child of "parent"
int BookmarkModel::rowCount( const QModelIndex& parent ) const
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
                static auto& index = Session::instance()->individualDirectory()->d_ptr->m_lPopularityIndex;
                return index.size();
            }
            else
                return modelItem->m_lChildren.size();
        case BookmarkNode::Type::BOOKMARK:
            return 0;
    }
    return 0;
}

Qt::ItemFlags BookmarkModel::flags( const QModelIndex& index ) const
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
int BookmarkModel::columnCount ( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 1;
}

///Get the bookmark parent
QModelIndex BookmarkModel::parent( const QModelIndex& idx) const
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
QModelIndex BookmarkModel::index(int row, int column, const QModelIndex& parent) const
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
QStringList BookmarkModel::mimeTypes() const
{
    static const QStringList mimes {
        RingMimes::PLAIN_TEXT,
        RingMimes::PHONENUMBER
    };

    return mimes;
}

///Generate mime data
QMimeData* BookmarkModel::mimeData(const QModelIndexList &indexes) const
{
    auto mimeData = new QMimeData();

    for (const auto& index : qAsConst(indexes)) {
        if (!index.isValid())
            continue;

        const auto text = data(
            index, static_cast<int>(ContactMethod::Role::Uri)
        ).toString().toUtf8();

        mimeData->setData(RingMimes::PLAIN_TEXT , text);
        mimeData->setData(RingMimes::PHONENUMBER, text);

        return mimeData;
    }

    return mimeData;
}

///Return valid payload types
int BookmarkModel::acceptedPayloadTypes()
{
    return CallModel::DropPayloadType::CALL;
}

bool BookmarkModel::displayMostPopular() const
{
    return d_ptr->m_DisplayPopular;
}

void BookmarkModel::setDisplayPopular(bool value)
{
    d_ptr->m_DisplayPopular = value;

    if (d_ptr->m_DisplayPopular)
        connect(Session::instance()->individualDirectory()->mostPopularNumberModel(),
            &QAbstractItemModel::rowsInserted,
            d_ptr,&BookmarkModelPrivate::reloadCategories
        );
    else
        disconnect(Session::instance()->individualDirectory()->mostPopularNumberModel(),
            &QAbstractItemModel::rowsInserted,
            d_ptr,&BookmarkModelPrivate::reloadCategories
        );

    d_ptr->reloadCategories();
}

QVector<ContactMethod*> BookmarkModelPrivate::bookmarkList() const
{
    if (q_ptr->collections().isEmpty())
        return {};

    return q_ptr->collections().first()->items<ContactMethod>();
}

void BookmarkModel::addBookmark(ContactMethod* number)
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

void BookmarkModel::removeBookmark(ContactMethod* number)
{
    if (collections().isEmpty()) {
        qWarning() << "No bookmark backend is set";
        return;
    }

    if (!collections().first()->editor<ContactMethod>()->contains(number))
        return;

    collections().first()->editor<ContactMethod>()->remove(number);
}

void BookmarkModel::remove(const QModelIndex& idx)
{
    if ((!idx.isValid()) || idx.model() != this)
        return;

    const auto item = static_cast<BookmarkNode*>(idx.internalPointer());

    if ((!item) || item->m_Type== BookmarkNode::Type::CATEGORY)
        return;

    collections().first()->editor<ContactMethod>()->remove(item->m_pNumber);
}

void BookmarkModel::clear()
{
    collections().first()->clear();
}

///Callback when an item change
void BookmarkModelPrivate::slotIndexChanged(const QModelIndex& idx)
{
    emit q_ptr->dataChanged(idx,idx);
}

bool BookmarkModel::addItemCallback(const ContactMethod* bookmark)
{
    // Contact methods can be duplicates of each other, make sure to handle them
    if ((!bookmark) || d_ptr->m_hTracked.contains(bookmark->d()) || bookmark->isSelf())
        return true;

    auto bm = new BookmarkNode(bookmark);
    auto category = d_ptr->getCategory(bm);

    bm->m_pParent = category;
    bm->m_Index   = category->m_lChildren.size();
    bm->m_ChConn  = connect(bookmark, &ContactMethod::changed, bookmark, [this,bm]() {
        d_ptr->slotIndexChanged(
            index(bm->m_Index, 0, index(bm->m_pParent->m_Index,0))
        );
    });

    beginInsertRows(index(category->m_Index,0), bm->m_Index, bm->m_Index);
    category->m_lChildren << bm;
    endInsertRows();

    const QString displayName = bookmark->bestName();
    auto dptr = bookmark->d();

    //If a contact arrive later, remove and add again (as the category may have changed)
    category->m_NameConn = connect(bookmark, &ContactMethod::primaryNameChanged, bookmark, [this,displayName,bookmark]() {
        if (displayName != bookmark->bestName()) {
            Q_ASSERT(d_ptr->m_hTracked.contains(bookmark->d()));
            removeItemCallback(bookmark);
            addItemCallback(bookmark);
        }
    });

    //If something like the nameservice causes a rebase, update the reverse map
    bm->m_RemConn = connect(bookmark, &ContactMethod::rebased, bookmark, [bm, dptr, this]() {
        d_ptr->m_hTracked.remove(dptr);
        d_ptr->m_hTracked[bm->m_pNumber->d()] = bm;
    });

    d_ptr->m_hTracked[dptr] = bm;

    return true;
}

bool BookmarkModel::removeItemCallback(const ContactMethod* cm)
{
    auto item = d_ptr->m_hTracked.value(cm->d());

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

    d_ptr->m_hTracked.remove(cm->d());

    return true;
}

void BookmarkModel::collectionAddedCallback(CollectionInterface* backend)
{
    Q_UNUSED(backend)
    d_ptr->reloadCategories();
}

#include <bookmarkmodel.moc>
