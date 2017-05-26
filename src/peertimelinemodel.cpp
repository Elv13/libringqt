/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                            *
 *                                                                                  *
 *   This library is free software; you can redistribute it and/or                  *
 *   modify it under the terms of the GNU Lesser General Public                     *
 *   License as published by the Free Software Foundation; either                   *
 *   version 2.1 of the License, or (at your option) any later version.             *
 *                                                                                  *
 *   This library is distributed in the hope that it will be useful,                *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of                 *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU              *
 *   Lesser General Public License for more details.                                *
 *                                                                                  *
 *   You should have received a copy of the GNU Lesser General Public               *
 *   License along with this library; if not, write to the Free Software            *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA *
 ***********************************************************************************/
#include "peertimelinemodel.h"

// Qt
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

// Ring
#include "media/textrecording.h"
#include "contactmethod.h"
#include "person.h"
#include "historytimecategorymodel.h"
#include "private/textrecording_p.h"
#include "private/matrixutils.h"

struct PeerTimelineNode final {
    std::vector<PeerTimelineNode*> m_lChildren;
    Media::Media::Direction        m_Direction;
    PeerTimelineNode*              m_pParent {nullptr};
    PeerTimelineModel::NodeType    m_Type;
    time_t                         m_StartTime {0}; //Also used for generic sorting duties
    time_t                         m_EndTime   {0};
    int                            m_Index;

    // Models are cache fault sensitive and the total size of the node are large,
    // let's use some anon unions. Polymorphism would be awkward here as there
    // is multiple actions on the elements.
    //TODO once this settle down, see if some `roleData()` inheritance would
    // be possible.
    union {

        // Type::TIME_CATEGORY
        HistoryTimeCategoryModel::HistoryConst m_HistoryCat;

        // Type::SECTION_DELIMITER
        Serializable::Group* m_pGroup;

        //CALL_GROUP
        ushort sumary[4];

        // Type::CALL
        Call* m_pCall;

        // Type::TEXT_MESSAGE
        TextMessageNode* m_pMessage;
    };

    ~PeerTimelineNode() {}
};

class PeerTimelineModelPrivate : public QObject
{
    Q_OBJECT
public:
    // Attributes
    Media::TextRecording* m_pRecording {nullptr};
    ContactMethod*        m_pCM        {nullptr};
    Person*               m_pPerson    {nullptr};

    std::vector<PeerTimelineNode*> m_lTimeCategories;
    QHash<int, PeerTimelineNode*>  m_hCats; //int <-> HistoryTimeCategoryModel::HistoryConst
    PeerTimelineNode* m_pCurrentGroup {nullptr};
    QHash<Serializable::Group*, PeerTimelineNode*> m_hTextGroups;

    // Constants
    static const Matrix1D<PeerTimelineModel::NodeType ,QString> peerTimelineNodeName;

    // Helpers
    PeerTimelineNode* getCategory(time_t t);
    PeerTimelineNode* getGroup(TextMessageNode* message);
    void insert(PeerTimelineNode* n, time_t t, std::vector<PeerTimelineNode*>& in, const QModelIndex& parent = {});
    void init();

    PeerTimelineModel* q_ptr;
public Q_SLOTS:
    void slotMessageAdded(TextMessageNode* message);
};

const Matrix1D<PeerTimelineModel::NodeType, QString> PeerTimelineModelPrivate::peerTimelineNodeName = {
    { PeerTimelineModel::NodeType::SECTION_DELIMITER , QStringLiteral( "sectionDelimiter" )},
    { PeerTimelineModel::NodeType::TEXT_MESSAGE      , QStringLiteral( "textMessage"      )},
    { PeerTimelineModel::NodeType::TIME_CATEGORY     , QStringLiteral( "timeCategory"     )},
    { PeerTimelineModel::NodeType::CALL_GROUP        , QStringLiteral( "callGroup"        )},
    { PeerTimelineModel::NodeType::CALL              , QStringLiteral( "call"             )},
    { PeerTimelineModel::NodeType::AUDIO_RECORDING   , QStringLiteral( "audioRecording"   )},
    { PeerTimelineModel::NodeType::SCREENSHOT        , QStringLiteral( "screenshot"       )},
    { PeerTimelineModel::NodeType::EMAIL             , QStringLiteral( "email"            )},
};

PeerTimelineModel::PeerTimelineModel(Person* cm) : QAbstractItemModel(cm), d_ptr(new PeerTimelineModelPrivate)
{
//     d_ptr->
}

PeerTimelineModel::PeerTimelineModel(ContactMethod* cm) : QAbstractItemModel(cm), d_ptr(new PeerTimelineModelPrivate)
{
    d_ptr->q_ptr = this;
    d_ptr->m_pCM = cm;
    d_ptr->m_pRecording = cm->textRecording();
    d_ptr->init();
    connect(d_ptr->m_pRecording->d_ptr, &Media::TextRecordingPrivate::messageAdded,
        d_ptr, & PeerTimelineModelPrivate::slotMessageAdded);
}

PeerTimelineModel::~PeerTimelineModel()
{
   delete d_ptr;
}

QHash<int,QByteArray> PeerTimelineModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;

        QHash<int, QByteArray>::const_iterator i;
        for (i = Ring::roleNames.constBegin(); i != Ring::roleNames.constEnd(); ++i)
            roles[i.key()] = i.value();

        roles.insert((int)Media::TextRecording::Role::Direction           , "direction"           );
        roles.insert((int)Media::TextRecording::Role::AuthorDisplayname   , "authorDisplayname"   );
        roles.insert((int)Media::TextRecording::Role::AuthorUri           , "authorUri"           );
        roles.insert((int)Media::TextRecording::Role::AuthorPresenceStatus, "authorPresenceStatus");
        roles.insert((int)Media::TextRecording::Role::Timestamp           , "timestamp"           );
        roles.insert((int)Media::TextRecording::Role::IsRead              , "isRead"              );
        roles.insert((int)Media::TextRecording::Role::FormattedDate       , "formattedDate"       );
        roles.insert((int)Media::TextRecording::Role::IsStatus            , "isStatus"            );
        roles.insert((int)Media::TextRecording::Role::DeliveryStatus      , "deliveryStatus"      );
        roles.insert((int)Media::TextRecording::Role::FormattedHtml       , "formattedHtml"       );
        roles.insert((int)Media::TextRecording::Role::LinkList            , "linkList"            );
        roles.insert((int)Media::TextRecording::Role::Id                  , "id"                  );

        roles.insert((int)PeerTimelineModel::Role::NodeType               , "nodeType"            );
        roles.insert((int)PeerTimelineModel::Role::EndAt                  , "endAt"               );
    }
    return roles;
}
///Get data from the model
QVariant PeerTimelineModel::data( const QModelIndex& idx, int role) const
{
    if ((!idx.isValid()) || idx.column())
        return {};

    auto n = static_cast<PeerTimelineNode*>(idx.internalPointer());

    switch(role) {
        case (int) Role::NodeType:
            return QVariant::fromValue(n->m_Type);
        case (int) Role::EndAt:
            return QDateTime::fromTime_t(n->m_EndTime);
    }

    switch(n->m_Type) {
        case PeerTimelineModel::NodeType::SECTION_DELIMITER:
            return role == Qt::DisplayRole ? QDateTime::fromTime_t(n->m_StartTime) : QVariant();
        case PeerTimelineModel::NodeType::TEXT_MESSAGE:
            return n->m_pMessage->roleData(role);
        case PeerTimelineModel::NodeType::TIME_CATEGORY:
            return role == Qt::DisplayRole ?
                HistoryTimeCategoryModel::indexToName((int)n->m_HistoryCat) : QVariant();
        case PeerTimelineModel::NodeType::CALL_GROUP:
        case PeerTimelineModel::NodeType::CALL:
        case PeerTimelineModel::NodeType::AUDIO_RECORDING:
        case PeerTimelineModel::NodeType::SCREENSHOT:
        case PeerTimelineModel::NodeType::EMAIL:
        case PeerTimelineModel::NodeType::COUNT__:
            break;
    }

    return {};
}

///Number of row
int PeerTimelineModel::rowCount(const QModelIndex& par) const
{
    if (!par.isValid())
        return d_ptr->m_lTimeCategories.size();

    const auto n = static_cast<PeerTimelineNode*>(par.internalPointer());

    return n->m_lChildren.size();
}

int PeerTimelineModel::columnCount(const QModelIndex& parentIdx) const
{
    if (!parentIdx.isValid())
        return 1;

    const auto n = static_cast<PeerTimelineNode*>(parentIdx.internalPointer());

    return n->m_lChildren.empty() ? 0 : 1;
}

///Model flags
Qt::ItemFlags PeerTimelineModel::flags(const QModelIndex& idx) const
{
    Q_UNUSED(idx)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex PeerTimelineModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    auto n = static_cast<PeerTimelineNode*>(index.internalPointer());

    if (!n->m_pParent)
        return {};

    return createIndex(n->m_pParent->m_Index, 0, n->m_pParent);
}

QModelIndex PeerTimelineModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column || row < 0)
        return {};

    if ((!parent.isValid()) && row < (int)d_ptr->m_lTimeCategories.size())
        return createIndex(row, 0, d_ptr->m_lTimeCategories[row]);

    if (!parent.isValid())
        return {};

    auto n = static_cast<PeerTimelineNode*>(parent.internalPointer());

    if (row >= (int)n->m_lChildren.size())
        return {};

    return createIndex(row, 0, n->m_lChildren[row]);
}

///Set model data
bool PeerTimelineModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
    Q_UNUSED(idx)
    Q_UNUSED(value)
    Q_UNUSED(role)

    //TODO support isRead and some other

    return false;
}

/// Generic modern C++ function to insert entries
void PeerTimelineModelPrivate::insert(PeerTimelineNode* n, time_t t,
    std::vector<PeerTimelineNode*>& in, const QModelIndex& parent)
{
    static PeerTimelineNode fake;
    fake.m_StartTime = t;

    auto it = std::upper_bound(in.begin(), in.end(), &fake,
        [](const PeerTimelineNode* a, const PeerTimelineNode* t2) -> bool {
            return (int)a->m_StartTime < (int)t2->m_StartTime;
    });

    n->m_Index = std::distance(in.begin(), it);

    std::for_each(it, in.end(), [](PeerTimelineNode* n) {n->m_Index++;});

    q_ptr->beginInsertRows(parent, n->m_Index, n->m_Index);
    in.insert(it, n);
    q_ptr->endInsertRows();
}

/// Return or create a time category
PeerTimelineNode* PeerTimelineModelPrivate::getCategory(time_t t)
{
    const auto cat = HistoryTimeCategoryModel::timeToHistoryConst(t);

    if (m_hCats.contains((int) cat))
        return m_hCats[(int) cat];

    auto n = new PeerTimelineNode;
    n->m_Type       = PeerTimelineModel::NodeType::TIME_CATEGORY;
    n->m_HistoryCat = cat;
    n->m_StartTime  = -(time_t) cat; //HACK

    m_hCats[(int) cat] = n;

    // A little hacky. To reduce the code complecity, it cast the cat to time_t
    // in order to mutualize the insertion code. Otherwise the `insert()` code
    // would be copy/pasted 9 times...
    insert(n, -(time_t) cat, m_lTimeCategories);

    return n;
}

PeerTimelineNode* PeerTimelineModelPrivate::getGroup(TextMessageNode* message)
{
    auto g = message->m_pMessage->group;
    Q_ASSERT(g);

    if (m_pCurrentGroup && g == m_pCurrentGroup->m_pGroup) {
        // The most simple case, no lookup required and nearly 100% probability
        return m_pCurrentGroup;
    }
    else if (m_hTextGroups.contains(g)) {
        return m_hTextGroups[g];
    }
    else {
        auto cat = getCategory(message->m_pMessage->timestamp);

        // Create a new entry
        PeerTimelineNode* ret = new PeerTimelineNode;
        ret->m_Type    = PeerTimelineModel::NodeType::SECTION_DELIMITER;
        ret->m_pGroup  = g;
        ret->m_pParent = cat;

        // Take the oldest message (for consistency)
        ret->m_StartTime = ret->m_pGroup->messages.first()->timestamp;

        Q_ASSERT(g->messages.isEmpty() == false);

        insert(ret, ret->m_StartTime, cat->m_lChildren, q_ptr->createIndex(cat->m_Index, 0, cat));

        m_hTextGroups[g] = ret;

        return ret;
    }

    // Can't happen, but trigger a warning when missing
    return nullptr;
}

void PeerTimelineModelPrivate::slotMessageAdded(TextMessageNode* message)
{
    auto group = getGroup(message);
    m_pCurrentGroup = group;

    PeerTimelineNode* ret = new PeerTimelineNode;
    ret->m_Type      = PeerTimelineModel::NodeType::TEXT_MESSAGE;
    ret->m_pMessage  = message;
    ret->m_StartTime = message->m_pMessage->timestamp;
    ret->m_pParent   = group;

    insert(ret, ret->m_StartTime, group->m_lChildren, q_ptr->createIndex(group->m_Index, 0, group));

    // Update the group timelapse
    group->m_EndTime = ret->m_StartTime;
    const auto idx   = q_ptr->createIndex(group->m_Index, 0, group->m_pParent);
    emit q_ptr->dataChanged(idx, idx);
}

void PeerTimelineModelPrivate::init()
{
    for( auto m : qAsConst(m_pRecording->d_ptr->m_lNodes))
        slotMessageAdded(m);
}

#include <peertimelinemodel.moc>

// kate: space-indent on; indent-width 4; replace-tabs on;
