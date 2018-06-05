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
#include "individualtimelinemodel.h"

// LibStdC++
#include <chrono>
#include <memory>

// Qt
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

// Ring
#include "media/textrecording.h"
#include "contactmethod.h"
#include "person.h"
#include "individual.h"
#include "libcard/eventaggregate.h"
#include "call.h"
#include "historytimecategorymodel.h"
#include "private/textrecording_p.h"
#include "libcard/matrixutils.h"

struct TimeCategoryData
{
    HistoryTimeCategoryModel::HistoryConst m_Cat;
    int m_Entries {0};
};

struct IndividualTimelineNode final {
    std::vector<IndividualTimelineNode*> m_lChildren;
    Media::Media::Direction              m_Direction;
    IndividualTimelineNode*              m_pParent {nullptr};
    IndividualTimelineModel::NodeType    m_Type;
    time_t                               m_StartTime {0}; //Also used for generic sorting duties
    time_t                               m_EndTime   {0};
    int                                  m_Index;

    enum SumaryEntries {
        INCOMING  ,
        OUTGOING  ,
        MISSED_IN ,
        MISSED_OUT,
    };

    // Models are cache fault sensitive and the total size of the node are large,
    // let's use some anon unions. Polymorphism would be awkward here as there
    // is multiple actions on the elements.
    //TODO once this settle down, see if some `roleData()` inheritance would
    // be possible.
    union {

        // Type::TIME_CATEGORY
        TimeCategoryData* m_pTimeCat;

        // Type::SECTION_DELIMITER and Type::SNAPSHOT_GROUP
        Serializable::Group* m_pGroup;

        //CALL_GROUP
        ushort m_lSummary[4];

        // Type::CALL
        Event* m_pCall;

        // Type::TEXT_MESSAGE and Type::SNAPSHOT
        TextMessageNode* m_pMessage;
    };

    ~IndividualTimelineNode() {
        if (m_Type == IndividualTimelineModel::NodeType::TIME_CATEGORY)
            delete m_pTimeCat;
    }
};

class IndividualTimelineModelPrivate : public QObject
{
    Q_OBJECT
public:
    explicit IndividualTimelineModelPrivate(IndividualTimelineModel* parent);

    // Attributes
    Individual* m_pIndividual {nullptr};

    std::vector<IndividualTimelineNode*> m_lTimeCategories;
    QHash<int, IndividualTimelineNode*>  m_hCats; //int <-> HistoryTimeCategoryModel::HistoryConst
    int m_TotalEntries {0};

    // Keep track of the current group to simplify the lookup. It also allows
    // to split upstream (Serializable::Group) in multiple logical groups in
    // case different media need to be inserted in the middle of a group.
    IndividualTimelineNode* m_pCurrentTextGroup {nullptr};
    IndividualTimelineNode* m_pCurrentCallGroup {nullptr};

    QHash<Serializable::Group*, IndividualTimelineNode*> m_hTextGroups;
    QSet<Media::TextRecording*> m_hTrackedTRs;
    QSet<ContactMethod*> m_hTrackedCMs;

    // Constants
    static const Matrix1D<IndividualTimelineModel::NodeType ,QString> peerTimelineNodeName;

    // Helpers
    QVariant groupRoleData(IndividualTimelineNode* group, int role);
    IndividualTimelineNode* getCategory(time_t t);
    IndividualTimelineNode* getGroup(TextMessageNode* message);
    void insert(IndividualTimelineNode* n, time_t t, std::vector<IndividualTimelineNode*>& in, const QModelIndex& parent = {});
    void incrementCounter(IndividualTimelineNode* n);
    void init();
    void disconnectOldCms();

    IndividualTimelineModel* q_ptr;
public Q_SLOTS:
    void slotMessageAdded(TextMessageNode* message);
    void slotEventAdded(QSharedPointer<Event>& call);
    void slotReload();
    void slotClear(IndividualTimelineNode* root = nullptr);
    void slotContactChanged(ContactMethod* cm, Person* newContact, Person* oldContact);
    void slotTextRecordingAdded(Media::TextRecording* r);
    void slotRebased(ContactMethod* cm, ContactMethod* other);
    void slotPhoneNumberChanged();
    void slotPersonDestroyed();
    void slotCmDestroyed();
    void slotIndividualDestroyed();
};

const Matrix1D<IndividualTimelineModel::NodeType, QString> IndividualTimelineModelPrivate::peerTimelineNodeName = {
    { IndividualTimelineModel::NodeType::SECTION_DELIMITER , QStringLiteral( "sectionDelimiter" )},
    { IndividualTimelineModel::NodeType::TEXT_MESSAGE      , QStringLiteral( "textMessage"      )},
    { IndividualTimelineModel::NodeType::TIME_CATEGORY     , QStringLiteral( "timeCategory"     )},
    { IndividualTimelineModel::NodeType::CALL_GROUP        , QStringLiteral( "callGroup"        )},
    { IndividualTimelineModel::NodeType::CALL              , QStringLiteral( "call"             )},
    { IndividualTimelineModel::NodeType::AUDIO_RECORDING   , QStringLiteral( "audioRecording"   )},
    { IndividualTimelineModel::NodeType::SNAPSHOT_GROUP    , QStringLiteral( "snapshotGroup"    )},
    { IndividualTimelineModel::NodeType::SNAPSHOT          , QStringLiteral( "snapshot"         )},
    { IndividualTimelineModel::NodeType::EMAIL             , QStringLiteral( "email"            )},
};

IndividualTimelineModelPrivate::IndividualTimelineModelPrivate(IndividualTimelineModel* parent) : q_ptr(parent)
{
    auto t = new QTimer(this);

    t->setInterval(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::hours(1)
        ).count()
    );
    t->start();

    connect(t, &QTimer::timeout, this, &IndividualTimelineModelPrivate::slotReload);
}

void IndividualTimelineModelPrivate::init()
{
    connect(m_pIndividual, &Individual::eventAdded,
        this, &IndividualTimelineModelPrivate::slotEventAdded);

    connect(m_pIndividual, &Individual::relatedContactMethodsAdded,
        this, &IndividualTimelineModelPrivate::slotPhoneNumberChanged);

    connect(m_pIndividual, &Individual::phoneNumbersChanged,
        this, &IndividualTimelineModelPrivate::slotPhoneNumberChanged);

    connect(m_pIndividual, &Individual::childrenContactChanged,
        this, &IndividualTimelineModelPrivate::slotContactChanged);

    connect(m_pIndividual, &Individual::childrenRebased,
        this, &IndividualTimelineModelPrivate::slotRebased);

    connect(m_pIndividual, &Individual::textRecordingAdded,
        this, &IndividualTimelineModelPrivate::slotTextRecordingAdded);

    auto cms = m_pIndividual->relatedContactMethods();
    cms.append(m_pIndividual->phoneNumbers());

    for (auto cm : qAsConst(cms))
        q_ptr->addContactMethod(cm);

    const auto events = m_pIndividual->eventAggregate()->events();

    for (auto e : qAsConst(events))
        slotEventAdded(e);

    const auto trs = m_pIndividual->textRecordings();
    for (auto t : qAsConst(trs))
        slotTextRecordingAdded(t);
}

IndividualTimelineModel::IndividualTimelineModel(Individual* ind) : QAbstractItemModel(ind), d_ptr(new IndividualTimelineModelPrivate(this))
{
    d_ptr->m_pIndividual = ind;

    if (ind)
        connect(d_ptr->m_pIndividual, &QObject::destroyed, d_ptr,
            &IndividualTimelineModelPrivate::slotIndividualDestroyed);

    d_ptr->init();
}

IndividualTimelineModel::~IndividualTimelineModel()
{
    if (d_ptr->m_pIndividual)
        d_ptr->disconnectOldCms();

    beginResetModel();
    d_ptr->slotClear();
    endResetModel();

    delete d_ptr;
}

QHash<int,QByteArray> IndividualTimelineModel::roleNames() const
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
        roles.insert((int)Media::TextRecording::Role::ContactMethod       , "contactMethod"       );
        roles.insert((int)Media::TextRecording::Role::HTML                , "HTML"                );
        roles.insert((int)Media::TextRecording::Role::HasText             , "hasText"             );

        roles.insert((int)IndividualTimelineModel::Role::NodeType         , "nodeType"            );
        roles.insert((int)IndividualTimelineModel::Role::CategoryEntries  , "categoryEntries"     );
        roles.insert((int)IndividualTimelineModel::Role::TotalEntries     , "totalEntries"        );
        roles.insert((int)IndividualTimelineModel::Role::ActiveCategories , "activeCategories"    );
        roles.insert((int)IndividualTimelineModel::Role::EndAt            , "endAt"               );
        roles.insert((int)IndividualTimelineModel::Role::CallCount        , "callCount"           );
        roles.insert((int)IndividualTimelineModel::Role::IncomingEntryCount, "incomingEntryCount" );
        roles.insert((int)IndividualTimelineModel::Role::OutgoingEntryCount, "outgoingEntryCount" );
    }
    return roles;
}

QVariant IndividualTimelineModelPrivate::groupRoleData(IndividualTimelineNode* group, int role)
{
    if (role == Qt::DisplayRole && group->m_Type == IndividualTimelineModel::NodeType::CALL_GROUP)
        return QStringLiteral("%1 (%2 incoming, %3 outgoing, %4 missed)")
            .arg(QDateTime::fromTime_t(group->m_StartTime).toString())
            .arg(group->m_lSummary[IndividualTimelineNode::SumaryEntries::INCOMING])
            .arg(group->m_lSummary[IndividualTimelineNode::SumaryEntries::OUTGOING])
            .arg(
                group->m_lSummary[IndividualTimelineNode::SumaryEntries::MISSED_IN] +
                group->m_lSummary[IndividualTimelineNode::SumaryEntries::MISSED_OUT]
            );

    if (group->m_Type == IndividualTimelineModel::NodeType::SECTION_DELIMITER) {
        switch (role) {
            case (int)IndividualTimelineModel::Role::IncomingEntryCount:
                return group->m_pGroup->m_IncomingCount;
            case (int)IndividualTimelineModel::Role::OutgoingEntryCount:
                return group->m_pGroup->m_OutgoingCount;
        }
    }

    switch(role) {
        case Qt::DisplayRole:
            return QDateTime::fromTime_t(group->m_StartTime).toString();
        case (int)Media::TextRecording::Role::FormattedDate:
            return QDateTime::fromTime_t(
                    group->m_EndTime
            );
    }

    return {};
}

///Get data from the model
QVariant IndividualTimelineModel::data( const QModelIndex& idx, int role) const
{
    if ((!idx.isValid()) || idx.column())
        return {};

    auto n = static_cast<IndividualTimelineNode*>(idx.internalPointer());

    switch(role) {
        case (int) Role::NodeType:
            return QVariant::fromValue(n->m_Type);
        case (int) Role::CategoryEntries:
            return n->m_Type == IndividualTimelineModel::NodeType::TIME_CATEGORY ?
                n->m_pTimeCat->m_Entries : -1;
        case (int) Role::TotalEntries:
            return d_ptr->m_TotalEntries;
        case (int) Role::ActiveCategories:
            return (int) d_ptr->m_lTimeCategories.size();
        case (int) Role::EndAt:
            return QDateTime::fromTime_t(n->m_EndTime);
        case (int) Role::CallCount:
            return n->m_Type == IndividualTimelineModel::NodeType::CALL_GROUP ?
                rowCount(idx) : 0;
    }

    switch(n->m_Type) {
        case IndividualTimelineModel::NodeType::SECTION_DELIMITER:
        case IndividualTimelineModel::NodeType::CALL_GROUP:
            return d_ptr->groupRoleData(n, role);
        case IndividualTimelineModel::NodeType::SNAPSHOT:
        case IndividualTimelineModel::NodeType::TEXT_MESSAGE:
            return n->m_pMessage->roleData(role);
        case IndividualTimelineModel::NodeType::TIME_CATEGORY:
            switch(role) {
                case Qt::DisplayRole:
                    return HistoryTimeCategoryModel::indexToName((int)n->m_pTimeCat->m_Cat);
                case (int)Media::TextRecording::Role::FormattedDate:
                    return QDateTime::fromTime_t(
                         n->m_EndTime
                    );
                default:
                    return QVariant();
            }
        case IndividualTimelineModel::NodeType::CALL:
            return n->m_pCall->roleData(role);
        case IndividualTimelineModel::NodeType::AUDIO_RECORDING:
        case IndividualTimelineModel::NodeType::SNAPSHOT_GROUP:
        case IndividualTimelineModel::NodeType::EMAIL:
        case IndividualTimelineModel::NodeType::COUNT__:
            break;
    }

    return {};
}

///Number of row
int IndividualTimelineModel::rowCount(const QModelIndex& par) const
{
    if (!par.isValid())
        return d_ptr->m_lTimeCategories.size();

    const auto n = static_cast<IndividualTimelineNode*>(par.internalPointer());

    return n->m_lChildren.size();
}

int IndividualTimelineModel::columnCount(const QModelIndex& par) const
{
    if (!par.isValid())
        return 1;

    const auto n = static_cast<IndividualTimelineNode*>(par.internalPointer());

    return n->m_lChildren.empty() ? 0 : 1;
}

///Model flags
Qt::ItemFlags IndividualTimelineModel::flags(const QModelIndex& idx) const
{
    Q_UNUSED(idx)
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex IndividualTimelineModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return {};

    auto n = static_cast<IndividualTimelineNode*>(index.internalPointer());

    if (!n->m_pParent)
        return {};

    return createIndex(n->m_pParent->m_Index, 0, n->m_pParent);
}

QModelIndex IndividualTimelineModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column || row < 0)
        return {};

    if ((!parent.isValid()) && row < (int)d_ptr->m_lTimeCategories.size())
        return createIndex(row, 0, d_ptr->m_lTimeCategories[row]);

    if (!parent.isValid())
        return {};

    auto n = static_cast<IndividualTimelineNode*>(parent.internalPointer());

    if (row >= (int)n->m_lChildren.size())
        return {};

    return createIndex(row, 0, n->m_lChildren[row]);
}

///Set model data
bool IndividualTimelineModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
    if (!idx.isValid())
        return false;

    auto n = static_cast<IndividualTimelineNode*>(idx.internalPointer());

    switch(role) {
        case (int)Media::TextRecording::Role::IsRead: {
            if (n->m_Type != IndividualTimelineModel::NodeType::TEXT_MESSAGE)
                return false;

            if (!n->m_pMessage)
                return false;

            const bool ret = n->m_pMessage->m_pRecording->d_ptr->performMessageAction(
                n->m_pMessage->m_pMessage,
                value.toBool() ?
                    Media::MimeMessage::Actions::READ :
                    Media::MimeMessage::Actions::UNREAD
            );

            if (ret)
                emit dataChanged(idx, idx);

            return ret;
        }
    }

    return false;
}

/// To create the summary scollbar, all entries need to be counted.
void IndividualTimelineModelPrivate::incrementCounter(IndividualTimelineNode* n)
{
    while(n->m_pParent)
        n = n->m_pParent;

    Q_ASSERT(n->m_Type == IndividualTimelineModel::NodeType::TIME_CATEGORY);

    n->m_pTimeCat->m_Entries++;

    m_TotalEntries++;

    const auto idx   = q_ptr->createIndex(n->m_Index, 0, n);
    emit q_ptr->dataChanged(idx, idx);
}

/// Generic modern C++ function to insert entries
void IndividualTimelineModelPrivate::insert(IndividualTimelineNode* n, time_t t,
    std::vector<IndividualTimelineNode*>& in, const QModelIndex& parent)
{
    // Many, if not most, elements are already sorted, speedup this case
    if (in.size() > 0 && in[in.size()-1]->m_StartTime < t) {
        n->m_Index = in.size();
        q_ptr->beginInsertRows(parent, n->m_Index, n->m_Index);
        in.push_back(n);
        q_ptr->endInsertRows();
        return;
    }

    static IndividualTimelineNode fake;
    fake.m_StartTime = t;

    auto it = std::upper_bound(in.begin(), in.end(), &fake,
        [](const IndividualTimelineNode* a, const IndividualTimelineNode* t2) -> bool {
            return (int)a->m_StartTime < (int)t2->m_StartTime;
    });

    n->m_Index = std::distance(in.begin(), it);

    std::for_each(it, in.end(), [](IndividualTimelineNode* n) {n->m_Index++;});

    q_ptr->beginInsertRows(parent, n->m_Index, n->m_Index);
    in.insert(it, n);
    q_ptr->endInsertRows();

    incrementCounter(n);
}

/// Return or create a time category
IndividualTimelineNode* IndividualTimelineModelPrivate::getCategory(time_t t)
{
    const auto cat = HistoryTimeCategoryModel::timeToHistoryConst(t);

    if (m_hCats.contains((int) cat))
        return m_hCats[(int) cat];

    auto n               = new IndividualTimelineNode;
    n->m_Type            = IndividualTimelineModel::NodeType::TIME_CATEGORY;
    n->m_pTimeCat        = new TimeCategoryData;
    n->m_pTimeCat->m_Cat = cat;
    n->m_StartTime       = -(time_t) cat; //HACK

    m_hCats[(int) cat] = n;

    // A little hacky. To reduce the code complexity, it cast the cat to time_t
    // in order to mutualize the insertion code. Otherwise the `insert()` code
    // would be copy/pasted 9 times...
    insert(n, -(time_t) cat, m_lTimeCategories);

    return n;
}

IndividualTimelineNode* IndividualTimelineModelPrivate::getGroup(TextMessageNode* message)
{
    const auto g = message->m_pGroup;
    Q_ASSERT(g);

    if (m_pCurrentTextGroup && g == m_pCurrentTextGroup->m_pGroup) {
        // The most simple case, no lookup required and nearly 100% probability
        return m_pCurrentTextGroup;
    }
    else if (auto n = m_hTextGroups.value(g)) {
        return n;
    }

    auto cat = getCategory(message->m_pMessage->timestamp());

    // Snapshot have their own look and feel
    const auto type = g->type == Media::MimeMessage::Type::SNAPSHOT ?
        IndividualTimelineModel::NodeType::SNAPSHOT_GROUP :
        IndividualTimelineModel::NodeType::SECTION_DELIMITER;

    // Create a new entry
    auto ret       = new IndividualTimelineNode;
    ret->m_Type    = type;
    ret->m_pGroup  = g;
    ret->m_pParent = cat;

    const auto range = g->timeRange();

    // Take the oldest message (for consistency)
    ret->m_StartTime = range.first;
    ret->m_EndTime   = range.second;

    Q_ASSERT(g->size());

    insert(ret, ret->m_StartTime, cat->m_lChildren, q_ptr->createIndex(cat->m_Index, 0, cat));

    m_hTextGroups[g] = ret;

    return ret;
}

void IndividualTimelineModelPrivate::slotMessageAdded(TextMessageNode* message)
{
    const auto messageType = message->m_pMessage->type() == Media::MimeMessage::Type::SNAPSHOT ?
        IndividualTimelineModel::NodeType::SNAPSHOT :
        IndividualTimelineModel::NodeType::TEXT_MESSAGE;

    // Do not show empty messages
    if (message->m_pMessage->plainText().isEmpty() && messageType != IndividualTimelineModel::NodeType::SNAPSHOT)
        return;

    auto group = getGroup(message);
    m_pCurrentTextGroup = group;
    m_pCurrentCallGroup = nullptr;

    auto ret         = new IndividualTimelineNode;
    ret->m_pMessage  = message;
    ret->m_StartTime = message->m_pMessage->timestamp();
    ret->m_pParent   = group;
    ret->m_Type      = messageType;

    insert(ret, ret->m_StartTime, group->m_lChildren, q_ptr->createIndex(group->m_Index, 0, group));

    // Update the group timelapse
    group->m_EndTime = std::max(ret->m_StartTime, group->m_EndTime);

    const auto idx   = q_ptr->createIndex(group->m_Index, 0, group);
    emit q_ptr->dataChanged(idx, idx);
}

void IndividualTimelineModelPrivate::slotEventAdded(QSharedPointer<Event>& event)
{
    if (event->eventCategory() != Event::EventCategory::CALL)
        return; //TODO merge both code paths

    auto cat = getCategory(event->startTimeStamp());

    // This abuses a bit of implementation details of the history. The calls
    // are expected to arrive ordered by time_t, so this allows to take a
    // little shortcut and skip proper lookup. If this is to ever become a false
    // assumption, then this code will need to be updated.
    if ((!m_pCurrentCallGroup) || (m_pCurrentCallGroup->m_pParent != cat) || event->isGroupHead()) {
        m_pCurrentCallGroup = new IndividualTimelineNode;
        m_pCurrentTextGroup = nullptr;

        m_pCurrentCallGroup->m_Type      = IndividualTimelineModel::NodeType::CALL_GROUP;
        m_pCurrentCallGroup->m_StartTime = event->startTimeStamp();
        m_pCurrentCallGroup->m_EndTime   = event->stopTimeStamp ();

        for (int i=0; i < 4; i++) m_pCurrentCallGroup->m_lSummary[i] = 0;

        m_pCurrentCallGroup->m_pParent = cat;

        insert(
            m_pCurrentCallGroup, m_pCurrentCallGroup->m_StartTime, cat->m_lChildren,
            q_ptr->createIndex(cat->m_Index, 0, cat)
        );
    }

    auto ret         = new IndividualTimelineNode;
    ret->m_pCall     = event.data(); //FIXME
    ret->m_Type      = IndividualTimelineModel::NodeType::CALL;
    ret->m_StartTime = event->startTimeStamp();
    ret->m_EndTime   = event->stopTimeStamp();
    ret->m_pParent   = m_pCurrentCallGroup;

    insert(
        ret, ret->m_StartTime, m_pCurrentCallGroup->m_lChildren,
        q_ptr->createIndex(m_pCurrentCallGroup->m_Index, 0, m_pCurrentCallGroup)
    );

    // Update the group timelapse and counters
    m_pCurrentCallGroup->m_lSummary[
        (event->status() == Event::Status::X_MISSED ? 2 : 0) +
        (event->direction() == Event::Direction::INCOMING ? 0 : 1)
    ]++;

    m_pCurrentCallGroup->m_EndTime = ret->m_EndTime;

    const auto idx = q_ptr->createIndex(
        m_pCurrentCallGroup->m_pParent->m_Index, 0, m_pCurrentCallGroup->m_pParent
    );

    // For the CallCount
    const auto gidx = q_ptr->createIndex(
        m_pCurrentCallGroup->m_Index, 0, m_pCurrentCallGroup
    );

    emit q_ptr->dataChanged(idx, idx);

    emit q_ptr->dataChanged(gidx, gidx);
}

/// To use with extreme restrict, this isn't really intended to be used directly
void IndividualTimelineModel::addContactMethod(ContactMethod* cm)
{

    // Only add new content, also check for potential aliases
    foreach (auto ocm, d_ptr->m_hTrackedCMs) {
        if ( (*cm) == (*ocm))
            return;
    }

    d_ptr->m_hTrackedCMs.insert(cm);
}

void IndividualTimelineModelPrivate::slotPhoneNumberChanged()
{
    auto cms = m_pIndividual->relatedContactMethods();
    cms.append(m_pIndividual->phoneNumbers());

    for (auto cm : qAsConst(cms))
        q_ptr->addContactMethod(cm);

    //TODO detect removed CMs and reload
}

// It can happen if something hold a smart pointer for too long. This is a bug
// elsewhere, but given I can't fix Qt5::Quick internal GC race conditions, it
// mitigates potential crashes.
void IndividualTimelineModelPrivate::slotPersonDestroyed()
{
    if (m_pIndividual)
        disconnect(m_pIndividual, &QObject::destroyed, this,
            &IndividualTimelineModelPrivate::slotIndividualDestroyed);

    // The slots will be disconnected by QObject
    m_pIndividual = nullptr;

    qWarning() << "A contact was destroyed while its timeline is referenced" << this;
}

void IndividualTimelineModelPrivate::slotCmDestroyed()
{
    //
}


/**
 * Yay, QML race conditions...
 */
void IndividualTimelineModel::clear()
{
    d_ptr->slotIndividualDestroyed();
}

void IndividualTimelineModelPrivate::slotIndividualDestroyed()
{
    if (!m_pIndividual) {
        // If that happens it was connected too many time
        Q_ASSERT(false);
        return;
    }

    disconnectOldCms();

    disconnect(m_pIndividual, &QObject::destroyed, this,
        &IndividualTimelineModelPrivate::slotIndividualDestroyed);

    m_pIndividual = nullptr;

    qWarning() << "An individual was destroyed while its timeline is referenced" << this;
}

void IndividualTimelineModelPrivate::slotReload()
{
    // Eventually, this could be optimized to detect if `reset` is more efficient
    // than individual `move` operation and pick the "right" mode. For now,
    // `reset` creates more readable code, so that will be it.

    q_ptr->beginResetModel();

    // Keep the old sub-trees, there is no point in re-generating them
    std::list< std::vector<IndividualTimelineNode*> > entries;

    for (auto n : m_lTimeCategories) {
        entries.push_back(std::move(n->m_lChildren));
        n->m_lChildren = {};
    }

    // Add all items to their new categories
    for (auto nl : entries) {
        for (auto n : nl) {
            auto cat = getCategory(n->m_StartTime);

            n->m_pParent = cat;
            n->m_Index   = cat->m_lChildren.size();

            cat->m_lChildren.push_back(n);
        }
    }

    q_ptr->endResetModel();
}

void IndividualTimelineModelPrivate::slotClear(IndividualTimelineNode* root)
{
    for (auto n : root ? root->m_lChildren : m_lTimeCategories)
        slotClear(n);

    if (root)
        delete root;
    else {
        m_lTimeCategories.clear();
        m_hCats.clear();
        m_hTextGroups.clear();
        m_hTrackedCMs.clear();
        m_hTrackedTRs.clear();
        m_pCurrentCallGroup = nullptr;
        m_pCurrentTextGroup = nullptr;
        m_TotalEntries = 0;
    }
}

void IndividualTimelineModelPrivate::disconnectOldCms()
{
    // Both m_pPerson and m_pCM can be null if the smart pointer race condition
    // inside of Qt5::Quick happens

    disconnect(m_pIndividual, &Individual::eventAdded,
        this, &IndividualTimelineModelPrivate::slotEventAdded);

    disconnect(m_pIndividual, &Individual::relatedContactMethodsAdded,
        this, &IndividualTimelineModelPrivate::slotPhoneNumberChanged);

    disconnect(m_pIndividual, &Individual::phoneNumbersChanged,
        this, &IndividualTimelineModelPrivate::slotPhoneNumberChanged);

    disconnect(m_pIndividual, &Individual::childrenContactChanged,
        this, &IndividualTimelineModelPrivate::slotContactChanged);

    disconnect(m_pIndividual, &Individual::textRecordingAdded,
        this, &IndividualTimelineModelPrivate::slotTextRecordingAdded);

    disconnect(m_pIndividual, &Individual::childrenRebased,
        this, &IndividualTimelineModelPrivate::slotRebased);

    // Add both phone number and whatever links to this person
    auto cms = m_pIndividual->relatedContactMethods();
    cms.append(m_pIndividual->phoneNumbers());

    for (auto cm : qAsConst(cms)) {
        disconnect(cm->textRecording()->d_ptr, &Media::TextRecordingPrivate::messageAdded,
            this, &IndividualTimelineModelPrivate::slotMessageAdded);
    }
}

/// Avoid showing mismatching data during/after merges to contact changes
void IndividualTimelineModelPrivate::
slotContactChanged(ContactMethod* cm, Person* newContact, Person* oldContact)
{
    Q_UNUSED(cm)
    //TODO if the contact changed, it probably doesn't belong to this individual
    // anymore. Handle it.

    if ((!newContact) || newContact == oldContact)
        return;

    // Will happen if the original contact was a placeholder
    if (oldContact && newContact->uid() == oldContact->uid())
        return;

    // Tracking what can and cannot be salvaged isn't worth it
    disconnectOldCms();

    q_ptr->beginResetModel();
    slotClear();
    q_ptr->endResetModel();

    init();
}

/// Reload everything if the new and old CM base is incompatible.
void IndividualTimelineModelPrivate::slotRebased(ContactMethod* cm, ContactMethod* other)
{
    if (other == cm)
        return;

    slotReload();
}

void IndividualTimelineModelPrivate::
slotTextRecordingAdded(Media::TextRecording* r)
{
    // This will happen when ContactMethods are merged
    if (m_hTrackedTRs.contains(r))
        return;

    m_hTrackedTRs.insert(r);

    for (auto m : qAsConst(r->d_ptr->m_lNodes))
        slotMessageAdded(m);

    connect(r->d_ptr, &Media::TextRecordingPrivate::messageAdded,
        this, &IndividualTimelineModelPrivate::slotMessageAdded);
}

#include <individualtimelinemodel.moc>

// kate: space-indent on; indent-width 4; replace-tabs on;
