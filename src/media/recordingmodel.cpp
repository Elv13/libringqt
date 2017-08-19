/****************************************************************************
 *   Copyright (C) 2015-2017 by Savoir-faire Linux                          *
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                           *
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
#include "recordingmodel.h"

// STD
#include <vector>

//Qt
#include <QtCore/QMimeData>
#include <QtCore/QCoreApplication>
#include <QtCore/QUrl>
#include <QtCore/QItemSelectionModel>

//Ring
#include "recording.h"
#include "textrecording.h"
#include "avrecording.h"
#include "media.h"
#include "call.h"
#include "localtextrecordingcollection.h"

//DRing
#include "dbus/configurationmanager.h"

struct RecordingNode final
{
    // Types
    enum class Type {
        TOP_LEVEL,
        SESSION  ,
        TAG      ,
    };

    // Constructors
    RecordingNode(RecordingNode::Type type);
    ~RecordingNode();

    //Attributes
    RecordingNode::Type         m_Type      {       };
    int                         m_Index     { -1    };
    QString                     m_CatName   {       };
    Media::Recording*           m_pRec      {nullptr};
    std::vector<RecordingNode*> m_lChildren {       };
    RecordingNode*              m_pParent   {nullptr};
};

class RecordingSubTreeProxy : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit RecordingSubTreeProxy(Media::RecordingModelPrivate* d, RecordingNode* root) :
        QAbstractListModel(&Media::RecordingModel::instance()), m_pRoot(root), d_ptr(d) {}

    virtual ~RecordingSubTreeProxy() {}

    virtual QVariant data ( const QModelIndex& index, int role) const override;
    virtual int rowCount( const QModelIndex& parent) const override;

private:
    Media::RecordingModelPrivate* d_ptr;
    RecordingNode* m_pRoot;
};

namespace Media {

class RecordingModelPrivate final : public QObject
{
   Q_OBJECT
public:
    explicit RecordingModelPrivate(RecordingModel* parent);
    ~RecordingModelPrivate();

    // Attributes
    std::vector<RecordingNode*>      m_lCategories             ;
    QHash<Recording*,RecordingNode*> m_hMapping                ;
    RecordingNode*                   m_pText          {nullptr};
    RecordingNode*                   m_pAudioVideo    {nullptr};
    LocalTextRecordingCollection*    m_pTextRecordingCollection;
    int                              m_UnreadCount    { 0     };
    QItemSelectionModel*             m_pSelectionModel{nullptr};

    // Helpers
    void initCategories();
    void forwardInsertion(TextRecording* r, ContactMethod* cm, Media::Direction direction);
    void updateUnreadCount(const int count);
    void emitChangedProxy();

    Q_DISABLE_COPY(RecordingModelPrivate)
    Q_DECLARE_PUBLIC(RecordingModel)

public Q_SLOTS:
    void slotSelectionChanged(const QModelIndex&, const QModelIndex&);

private:
    RecordingModel* q_ptr;
};

} // namespace Media::

RecordingNode::RecordingNode(RecordingNode::Type type) : m_Type(type)
{
}

RecordingNode::~RecordingNode()
{
    foreach(RecordingNode* c, m_lChildren)
        delete c;
}

Media::RecordingModelPrivate::RecordingModelPrivate(RecordingModel* parent) : q_ptr(parent),m_pText(nullptr),
m_pAudioVideo(nullptr)
{

}

Media::RecordingModelPrivate::~RecordingModelPrivate()
{
    m_hMapping.clear();
    m_lCategories.clear();

    if (m_pTextRecordingCollection)
        delete m_pTextRecordingCollection;

    if (m_pText)
        delete m_pText;

    if (m_pAudioVideo)
        delete m_pAudioVideo;
}

void Media::RecordingModelPrivate::forwardInsertion(TextRecording* r, ContactMethod* cm, Media::Media::Direction direction)
{
    Q_UNUSED(direction)
    emit q_ptr->newTextMessage(r, cm);

}

void Media::RecordingModelPrivate::updateUnreadCount(const int count)
{
    m_UnreadCount += count;
    if (m_UnreadCount <= 0) {
        m_UnreadCount = 0;
    }
    emit q_ptr->unreadMessagesCountChanged(m_UnreadCount);
}

Media::RecordingModel::~RecordingModel()
{
    delete d_ptr;
}

Media::RecordingModel::RecordingModel(QObject* parent) : QAbstractItemModel(parent), CollectionManagerInterface<Recording>(this),
d_ptr(new RecordingModelPrivate(this))
{
    setObjectName(QStringLiteral("RecordingModel"));

    d_ptr->m_pTextRecordingCollection = addCollection<LocalTextRecordingCollection>();

    d_ptr->m_pTextRecordingCollection->listId([](const QList<CollectionInterface::Element>& e) {
        //TODO
        Q_UNUSED(e);
    });

    d_ptr->initCategories();
}

Media::RecordingModel& Media::RecordingModel::instance()
{
    static auto instance = new RecordingModel(QCoreApplication::instance());
    return *instance;
}

QHash<int,QByteArray> Media::RecordingModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    /*static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;
    }*/
    return roles;
}

//Do nothing
bool Media::RecordingModel::setData( const QModelIndex& index, const QVariant &value, int role)
{
    Q_UNUSED(index)
    Q_UNUSED(value)
    Q_UNUSED(role)
    return false;
}

///Get bookmark model data RecordingNode::Type and Call::Role
QVariant Media::RecordingModel::data( const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto modelItem = static_cast<RecordingNode*>(index.internalPointer());

    if (modelItem->m_Type == RecordingNode::Type::TOP_LEVEL) {
        if (index.column() == 0 && role == Qt::DisplayRole)
            return modelItem->m_CatName;
        if (index.column() == 1 && role == Qt::DisplayRole)
            return static_cast<int>(modelItem->m_lChildren.size());

        return {};
    }

    const TextRecording* tRec  = nullptr;
    const AVRecording*   avRec = nullptr;

    if (modelItem->m_pRec->type() == Recording::Type::TEXT)
        tRec = static_cast<const TextRecording*>(modelItem->m_pRec);
    else if (modelItem->m_pRec->type() == Recording::Type::AUDIO_VIDEO)
        avRec = static_cast<const AVRecording*>(modelItem->m_pRec);

    switch(index.column()) {
        case 0:
            if (tRec) {
                switch (role) {
                    case Qt::DisplayRole:
                        return tRec->peers().size() ? tRec->peers().constFirst()->primaryName() : tRec->roleData(-1, role);
                    default:
                        return tRec->roleData(-1, role);
                }
            }
            else if(avRec && avRec->call())
                return avRec->roleData(role);
            break;
        case 1:
            switch (role) {
                case Qt::DisplayRole:
                    if (tRec && tRec->peers().size())
                        return tRec->size();
                    if (avRec && avRec->call())
                        return tr("N/A");
            }
            break;
    }

    return QVariant();
}

///Get header data
QVariant Media::RecordingModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch(section) {
            case 0:
                return QVariant(tr("Recordings"));
            case 1:
                return QVariant(tr("Count"));
        }
    }
    return QVariant();
}

///Get the number of child of "parent"
int Media::RecordingModel::rowCount( const QModelIndex& parent ) const
{
    if (!parent.isValid())
        return d_ptr->m_lCategories.size();

    // Only the first column has a tree
    if (parent.column())
        return 0;

    const auto modelItem = static_cast<RecordingNode*>(parent.internalPointer());

    return modelItem->m_lChildren.size();
}

Qt::ItemFlags Media::RecordingModel::flags( const QModelIndex& index ) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

int Media::RecordingModel::columnCount ( const QModelIndex& parent) const
{
    Q_UNUSED(parent)

    if (!parent.isValid())
        return 2;

    const auto modelItem = static_cast<RecordingNode*>(parent.internalPointer());

    return modelItem->m_lChildren.size() ? 2 : 0;
}

///Get the bookmark parent
QModelIndex Media::RecordingModel::parent( const QModelIndex& idx) const
{
    if (!idx.isValid())
        return {};

    const auto modelItem = static_cast<RecordingNode*>(idx.internalPointer());

    if (modelItem->m_Type != RecordingNode::Type::SESSION)
        return {};

    if (auto item = static_cast<RecordingNode*>(modelItem)->m_pParent)
        return createIndex(item->m_Index, 0, item);

    return {};
} //parent

///Get the index
QModelIndex Media::RecordingModel::index(int row, int column, const QModelIndex& parent) const
{
    const int count = static_cast<int>(d_ptr->m_lCategories.size());

    if (column > 1 || row < 0 || ((!parent.isValid()) && row >= count))
        return {};

    if (!parent.isValid())
        return createIndex(row, column, d_ptr->m_lCategories[row]);

    const auto modelItem = static_cast<const RecordingNode*>(parent.internalPointer());

    if (row >= static_cast<int>(modelItem->m_lChildren.size()))
        return {};

    return createIndex(row, column, modelItem->m_lChildren[row]);
}

void Media::RecordingModelPrivate::initCategories()
{
    if (m_lCategories.size())
        return;

    //Create some categories
    q_ptr->beginInsertRows({}, 0 , 1);
    m_pText = new RecordingNode(RecordingNode::Type::TOP_LEVEL);
    m_pText->m_CatName = tr("Text messages");
    m_pText->m_Index   = 0;
    m_lCategories.push_back(m_pText);

    m_pAudioVideo = new RecordingNode(RecordingNode::Type::TOP_LEVEL);
    m_pAudioVideo->m_CatName = tr("Audio/Video");
    m_pAudioVideo->m_Index   = 1;
    m_lCategories.push_back(m_pAudioVideo);
    q_ptr->endInsertRows();
}

bool Media::RecordingModel::addItemCallback(const Recording* item)
{
    Q_UNUSED(item)

    d_ptr->initCategories();

    //Categorize by general media group
    RecordingNode* parent = nullptr;

    if (item->type() == Recording::Type::TEXT)
        parent = d_ptr->m_pText;
    else if (item->type() == Recording::Type::AUDIO_VIDEO)
        parent = d_ptr->m_pAudioVideo;

    if (!parent)
        return false;

    //Insert the item]
    const int idx = parent->m_lChildren.size();
    const_cast<Recording*>(item)->setParent(this);
    beginInsertRows(index(parent->m_Index,0), idx, idx);

    RecordingNode* n = new RecordingNode       ( RecordingNode::Type::SESSION );
    n->m_pRec        = const_cast<Recording*>  ( item );
    n->m_Index       = idx;
    n->m_pParent     = parent;
    parent->m_lChildren.push_back(n);
    d_ptr->m_hMapping[n->m_pRec] = n;

    endInsertRows();

    if (item->type() == Recording::Type::TEXT) {
        const TextRecording* r = static_cast<const TextRecording*>(item);
        connect(r, &TextRecording::unreadCountChange, d_ptr, &RecordingModelPrivate::updateUnreadCount);
        connect(r, &TextRecording::messageInserted  , [n, this, parent](
            const QMap<QString,QString>&, ContactMethod* cm, Media::Media::Direction d
        ){
            const auto par = index(parent->m_Index, 0);

            emit dataChanged(
                index(n->m_Index, 0, par),
                index(n->m_Index, 1, par)
            );

            if (n->m_pRec->type() == Recording::Type::TEXT)
                d_ptr->forwardInsertion(
                    static_cast<TextRecording*>(n->m_pRec), cm, d
                );
        });
    }

    return true;
}

bool Media::RecordingModel::removeItemCallback(const Recording* item)
{
    Q_UNUSED(item)
    return false;
}

bool Media::RecordingModel::clearAllCollections() const
{
    foreach (CollectionInterface* backend, collections(CollectionInterface::SupportedFeatures::CLEAR)) {
        backend->clear();
    }
    return true;
}

///Deletes all recordings (which are possible to delete) and clears model
void Media::RecordingModel::clear()
{
    beginResetModel();
    clearAllCollections();
    endResetModel();
}

void Media::RecordingModel::collectionAddedCallback(CollectionInterface* backend)
{
    Q_UNUSED(backend)
}

///Set where the call recordings will be saved
void Media::RecordingModel::setRecordPath(const QString& path)
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    configurationManager.setRecordPath(path);
}

///Return the path where recordings are going to be saved
QString Media::RecordingModel::recordPath() const
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    return configurationManager.getRecordPath();
}

///are all calls recorded by default
bool Media::RecordingModel::isAlwaysRecording() const
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    return configurationManager.getIsAlwaysRecording();
}

///Set if all calls needs to be recorded
void Media::RecordingModel::setAlwaysRecording(bool record)
{
    ConfigurationManagerInterface& configurationManager = ConfigurationManager::instance();
    configurationManager.setIsAlwaysRecording   ( record );
}

int  Media::RecordingModel::unreadCount() const
{
    return d_ptr->m_UnreadCount;
}

///Create or load the recording associated with the ContactMethod cm
Media::TextRecording* Media::RecordingModel::createTextRecording(const ContactMethod* cm)
{
    TextRecording* r = d_ptr->m_pTextRecordingCollection->createFor(cm);
    r->setParent(this);

    return r;
}

QItemSelectionModel* Media::RecordingModel::selectionModel() const
{
    if (!d_ptr->m_pSelectionModel) {
        d_ptr->m_pSelectionModel = new QItemSelectionModel(
            const_cast<RecordingModel*>(this)
        );
        connect(d_ptr->m_pSelectionModel, &QItemSelectionModel::currentChanged,
            d_ptr, &RecordingModelPrivate::slotSelectionChanged);

    }

    return d_ptr->m_pSelectionModel;
}

/**
 * The daemon can only play a single file at once. This code ensure LRC keeps
 * track of the currently playing file.
 *
 * It only makes sense for audio (and eventually video) files.
 */
void Media::RecordingModelPrivate::
slotSelectionChanged(const QModelIndex& c, const QModelIndex& o)
{
    if (o.isValid() && o.parent().isValid() && !c.isValid()) {
        emit q_ptr->currentRecordingChanged(nullptr);
        return;
    }

    if (!c.parent().isValid())
        return;

    emit q_ptr->currentRecordingChanged(
        static_cast<RecordingNode*>(c.internalPointer())->m_pRec
    );
}

Media::Recording* Media::RecordingModel::currentRecording() const
{
    if (!d_ptr->m_pSelectionModel)
        return nullptr;

    const auto idx = d_ptr->m_pSelectionModel->currentIndex();

    if (!idx.isValid())
        return nullptr;

    const auto modelItem = static_cast<RecordingNode*>(idx.internalPointer());

    return modelItem->m_pRec;
}


void Media::RecordingModel::setCurrentRecording( Recording* recording )
{
    if (!d_ptr->m_hMapping.contains(recording))
        return;

    auto n   = d_ptr->m_hMapping[recording];
    auto idx = createIndex(n->m_Index, 0, n);

    selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
}

QVariant RecordingSubTreeProxy::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    return m_pRoot->m_lChildren[index.row()]->m_pRec->roleData(role);
}

int RecordingSubTreeProxy::rowCount( const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_pRoot->m_lChildren.size();
}

QAbstractItemModel* Media::RecordingModel::audioRecordingModel() const
{
    static auto p = new RecordingSubTreeProxy(
        instance().d_ptr, instance().d_ptr->m_lCategories[1]
    );

    return p;
}

QAbstractItemModel* Media::RecordingModel::textRecordingModel() const
{
    static auto p = new RecordingSubTreeProxy(
        instance().d_ptr, instance().d_ptr->m_lCategories[0]
    );

    return p;
}

#include <recordingmodel.moc>
