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
#include "call.h"
#include "historytimecategorymodel.h"
#include "private/textrecording_p.h"
#include "private/matrixutils.h"

struct TimeCategoryData
{
    HistoryTimeCategoryModel::HistoryConst m_Cat;
    int m_Entries {0};
};

struct PeerTimelineNode final {
    std::vector<PeerTimelineNode*> m_lChildren;
    Media::Media::Direction        m_Direction;
    PeerTimelineNode*              m_pParent {nullptr};
    PeerTimelineModel::NodeType    m_Type;
    time_t                         m_StartTime {0}; //Also used for generic sorting duties
    time_t                         m_EndTime   {0};
    int                            m_Index;

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
        Call* m_pCall;

        // Type::TEXT_MESSAGE and Type::SNAPSHOT
        TextMessageNode* m_pMessage;
    };

    ~PeerTimelineNode() {
        if (m_Type == PeerTimelineModel::NodeType::TIME_CATEGORY)
            delete m_pTimeCat;
    }
};

class PeerTimelineModelPrivate : public QObject
{
    Q_OBJECT
public:
    explicit PeerTimelineModelPrivate(PeerTimelineModel* parent);

    // Attributes
    ContactMethod*        m_pCM        {nullptr};
    Person*               m_pPerson    {nullptr};

    std::vector<PeerTimelineNode*> m_lTimeCategories;
    QHash<int, PeerTimelineNode*>  m_hCats; //int <-> HistoryTimeCategoryModel::HistoryConst
    int m_TotalEntries {0};

    // Keep track of the current group to simplify the lookup. It also allows
    // to split upstream (Serializable::Group) in multiple logical groups in
    // case different media need to be inserted in the middle of a group.
    PeerTimelineNode* m_pCurrentTextGroup {nullptr};
    PeerTimelineNode* m_pCurrentCallGroup {nullptr};

    QHash<Serializable::Group*, PeerTimelineNode*> m_hTextGroups;
    QSet<Media::TextRecording*> m_hTrackedTRs;
    QSet<ContactMethod*> m_hTrackedCMs;

    // Constants
    static const Matrix1D<PeerTimelineModel::NodeType ,QString> peerTimelineNodeName;

    // Helpers
    QVariant groupRoleData(PeerTimelineNode* group, int role);
    PeerTimelineNode* getCategory(time_t t);
    PeerTimelineNode* getGroup(TextMessageNode* message);
    void insert(PeerTimelineNode* n, time_t t, std::vector<PeerTimelineNode*>& in, const QModelIndex& parent = {});
    void incrementCounter(PeerTimelineNode* n);
    void init();
    void disconnectOldCms();

    PeerTimelineModel* q_ptr;
public Q_SLOTS:
    void slotMessageAdded(TextMessageNode* message);
    void slotCallAdded(Call* call);
    void slotReload();
    void slotClear(PeerTimelineNode* root = nullptr);
    void slotContactChanged(Person* newContact, Person* oldContact);
    void slotTextRecordingAdded(Media::TextRecording* r);
    void slotRebased(ContactMethod* other);
    void slotPhoneNumberChanged();
    void slotPersonDestroyed();
};

const Matrix1D<PeerTimelineModel::NodeType, QString> PeerTimelineModelPrivate::peerTimelineNodeName = {
    { PeerTimelineModel::NodeType::SECTION_DELIMITER , QStringLiteral( "sectionDelimiter" )},
    { PeerTimelineModel::NodeType::TEXT_MESSAGE      , QStringLiteral( "textMessage"      )},
    { PeerTimelineModel::NodeType::TIME_CATEGORY     , QStringLiteral( "timeCategory"     )},
    { PeerTimelineModel::NodeType::CALL_GROUP        , QStringLiteral( "callGroup"        )},
    { PeerTimelineModel::NodeType::CALL              , QStringLiteral( "call"             )},
    { PeerTimelineModel::NodeType::AUDIO_RECORDING   , QStringLiteral( "audioRecording"   )},
    { PeerTimelineModel::NodeType::SNAPSHOT_GROUP    , QStringLiteral( "snapshotGroup"    )},
    { PeerTimelineModel::NodeType::SNAPSHOT          , QStringLiteral( "snapshot"         )},
    { PeerTimelineModel::NodeType::EMAIL             , QStringLiteral( "email"            )},
};

PeerTimelineModelPrivate::PeerTimelineModelPrivate(PeerTimelineModel* parent) : q_ptr(parent)
{
    auto t = new QTimer(this);

    t->setInterval(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::hours(1)
        ).count()
    );
    t->start();

    connect(t, &QTimer::timeout, this, &PeerTimelineModelPrivate::slotReload);

    //TODO
    // * Detect new person contact method
    // * Merge the timeline when the contact is set
}

void PeerTimelineModelPrivate::init()
{
    if (m_pPerson) {
        connect(m_pPerson, &Person::callAdded,
            this, &PeerTimelineModelPrivate::slotCallAdded);

        connect(m_pPerson->individual().data(), &Individual::relatedContactMethodsAdded,
            this, &PeerTimelineModelPrivate::slotPhoneNumberChanged);

        connect(m_pPerson->individual().data(), &Individual::phoneNumbersChanged,
            this, &PeerTimelineModelPrivate::slotPhoneNumberChanged);

        auto cms = m_pPerson->individual()->relatedContactMethods();
        cms.append(m_pPerson->individual()->phoneNumbers());

        for (auto cm : qAsConst(cms))
            q_ptr->addContactMethod(cm);
    }
    else {
        m_hTrackedCMs.insert(m_pCM);

        slotTextRecordingAdded(m_pCM->textRecording());

        connect(m_pCM, &ContactMethod::callAdded,
            this, &PeerTimelineModelPrivate::slotCallAdded);

        connect(m_pCM, &ContactMethod::contactChanged,
            this, &PeerTimelineModelPrivate::slotContactChanged);

        connect(m_pCM, &ContactMethod::rebased,
            this, &PeerTimelineModelPrivate::slotRebased);

        connect(m_pCM, &ContactMethod::alternativeTextRecordingAdded,
            this, &PeerTimelineModelPrivate::slotTextRecordingAdded);

        const auto calls = m_pCM->calls();

        for (auto c : qAsConst(calls))
            slotCallAdded(c);

        const auto trs = m_pCM->alternativeTextRecordings();

        for (auto t : qAsConst(trs))
            slotTextRecordingAdded(t);
    }
}

PeerTimelineModel::PeerTimelineModel(Person* cm) : QAbstractItemModel(cm), d_ptr(new PeerTimelineModelPrivate(this))
{
    d_ptr->m_pPerson = cm;
    d_ptr->init();
}

PeerTimelineModel::PeerTimelineModel(ContactMethod* cm) : QAbstractItemModel(cm), d_ptr(new PeerTimelineModelPrivate(this))
{
    if (cm->contact()) {
        d_ptr->m_pPerson = cm->contact();
        connect(d_ptr->m_pPerson, &QObject::destroyed,
            d_ptr, &PeerTimelineModelPrivate::slotPersonDestroyed);
    }
    else {
        d_ptr->m_pCM = cm;
        connect(cm, &QObject::destroyed,
            d_ptr, &PeerTimelineModelPrivate::slotPersonDestroyed);
    }

    d_ptr->init();
}

PeerTimelineModel::~PeerTimelineModel()
{
    d_ptr->disconnectOldCms();

    beginResetModel();
    d_ptr->slotClear();
    endResetModel();

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
        roles.insert((int)PeerTimelineModel::Role::CategoryEntries        , "categoryEntries"     );
        roles.insert((int)PeerTimelineModel::Role::TotalEntries           , "totalEntries"        );
        roles.insert((int)PeerTimelineModel::Role::ActiveCategories       , "activeCategories"    );
        roles.insert((int)PeerTimelineModel::Role::EndAt                  , "endAt"               );
    }
    return roles;
}

QVariant PeerTimelineModelPrivate::groupRoleData(PeerTimelineNode* group, int role)
{
    if (role == Qt::DisplayRole && group->m_Type == PeerTimelineModel::NodeType::CALL_GROUP)
        return QStringLiteral("%1 (%2 incoming, %3 outgoing, %4 missed)")
            .arg(QDateTime::fromTime_t(group->m_StartTime).toString())
            .arg(group->m_lSummary[PeerTimelineNode::SumaryEntries::INCOMING])
            .arg(group->m_lSummary[PeerTimelineNode::SumaryEntries::OUTGOING])
            .arg(
                group->m_lSummary[PeerTimelineNode::SumaryEntries::MISSED_IN] +
                group->m_lSummary[PeerTimelineNode::SumaryEntries::MISSED_OUT]
            );

    switch(role) {
        case Qt::DisplayRole:
            return QDateTime::fromTime_t(group->m_StartTime).toString();
    }

    return {};
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
        case (int) Role::CategoryEntries:
            return n->m_Type == PeerTimelineModel::NodeType::TIME_CATEGORY ?
                n->m_pTimeCat->m_Entries : -1;
        case (int) Role::TotalEntries:
            return d_ptr->m_TotalEntries;
        case (int) Role::ActiveCategories:
            return (int) d_ptr->m_lTimeCategories.size();
        case (int) Role::EndAt:
            return QDateTime::fromTime_t(n->m_EndTime);
    }

    switch(n->m_Type) {
        case PeerTimelineModel::NodeType::SECTION_DELIMITER:
        case PeerTimelineModel::NodeType::CALL_GROUP:
            return d_ptr->groupRoleData(n, role);
        case PeerTimelineModel::NodeType::SNAPSHOT:
        case PeerTimelineModel::NodeType::TEXT_MESSAGE:
            return n->m_pMessage->roleData(role);
        case PeerTimelineModel::NodeType::TIME_CATEGORY:
            return role == Qt::DisplayRole ?
                HistoryTimeCategoryModel::indexToName((int)n->m_pTimeCat->m_Cat) : QVariant();
        case PeerTimelineModel::NodeType::CALL:
            return n->m_pCall->roleData(role);
        case PeerTimelineModel::NodeType::AUDIO_RECORDING:
        case PeerTimelineModel::NodeType::SNAPSHOT_GROUP:
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

int PeerTimelineModel::columnCount(const QModelIndex& par) const
{
    if (!par.isValid())
        return 1;

    const auto n = static_cast<PeerTimelineNode*>(par.internalPointer());

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
    if (!idx.isValid())
        return false;

    auto n = static_cast<PeerTimelineNode*>(idx.internalPointer());

    switch(role) {
        case (int)Media::TextRecording::Role::IsRead: {
            if (n->m_Type != PeerTimelineModel::NodeType::TEXT_MESSAGE)
                return false;

            if (!n->m_pMessage)
                return false;

            const bool ret = n->m_pMessage->m_pRecording->d_ptr->performMessageAction(
                n->m_pMessage->m_pMessage,
                value.toBool() ?
                    MimeMessage::Actions::READ :
                    MimeMessage::Actions::UNREAD
            );

            if (ret)
                emit dataChanged(idx, idx);

            return ret;
        }
    }

    return false;
}

/// To create the summary scollbar, all entries need to be counted.
void PeerTimelineModelPrivate::incrementCounter(PeerTimelineNode* n)
{
    while(n->m_pParent)
        n = n->m_pParent;

    Q_ASSERT(n->m_Type == PeerTimelineModel::NodeType::TIME_CATEGORY);

    n->m_pTimeCat->m_Entries++;

    m_TotalEntries++;

    const auto idx   = q_ptr->createIndex(n->m_Index, 0, n);
    emit q_ptr->dataChanged(idx, idx);
}

/// Generic modern C++ function to insert entries
void PeerTimelineModelPrivate::insert(PeerTimelineNode* n, time_t t,
    std::vector<PeerTimelineNode*>& in, const QModelIndex& parent)
{
    // Many, if not most, elements are already sorted, speedup this case
    if (in.size() > 0 && in[in.size()-1]->m_StartTime < t) {
        n->m_Index = in.size();
        q_ptr->beginInsertRows(parent, n->m_Index, n->m_Index);
        in.push_back(n);
        q_ptr->endInsertRows();
        return;
    }

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

    incrementCounter(n);
}

/// Return or create a time category
PeerTimelineNode* PeerTimelineModelPrivate::getCategory(time_t t)
{
    const auto cat = HistoryTimeCategoryModel::timeToHistoryConst(t);

    if (m_hCats.contains((int) cat))
        return m_hCats[(int) cat];

    auto n               = new PeerTimelineNode;
    n->m_Type            = PeerTimelineModel::NodeType::TIME_CATEGORY;
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

PeerTimelineNode* PeerTimelineModelPrivate::getGroup(TextMessageNode* message)
{
    const auto g = message->m_pGroup;
    Q_ASSERT(g);

    if (m_pCurrentTextGroup && g == m_pCurrentTextGroup->m_pGroup) {
        // The most simple case, no lookup required and nearly 100% probability
        return m_pCurrentTextGroup;
    }
    else if (m_hTextGroups.contains(g)) {
        return m_hTextGroups[g];
    }
    else {
        auto cat = getCategory(message->m_pMessage->timestamp());

        // Snapshot have their own look and feel
        const auto type = g->type == MimeMessage::Type::SNAPSHOT ?
            PeerTimelineModel::NodeType::SNAPSHOT_GROUP :
            PeerTimelineModel::NodeType::SECTION_DELIMITER;

        // Create a new entry
        auto ret       = new PeerTimelineNode;
        ret->m_Type    = type;
        ret->m_pGroup  = g;
        ret->m_pParent = cat;

        // Take the oldest message (for consistency)
        ret->m_StartTime = ret->m_pGroup->messages.constFirst()->timestamp();

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
    const auto messageType = message->m_pMessage->type() == MimeMessage::Type::SNAPSHOT ?
        PeerTimelineModel::NodeType::SNAPSHOT :
        PeerTimelineModel::NodeType::TEXT_MESSAGE;

    // Do not show empty messages
    if (message->m_pMessage->plainText().isEmpty() && messageType != PeerTimelineModel::NodeType::SNAPSHOT)
        return;

    auto group = getGroup(message);
    m_pCurrentTextGroup = group;
    m_pCurrentCallGroup = nullptr;

    auto ret         = new PeerTimelineNode;
    ret->m_pMessage  = message;
    ret->m_StartTime = message->m_pMessage->timestamp();
    ret->m_pParent   = group;
    ret->m_Type      = messageType;

    insert(ret, ret->m_StartTime, group->m_lChildren, q_ptr->createIndex(group->m_Index, 0, group));

    // Update the group timelapse
    group->m_EndTime = ret->m_StartTime;
    const auto idx   = q_ptr->createIndex(group->m_Index, 0, group);
    emit q_ptr->dataChanged(idx, idx);
}

void PeerTimelineModelPrivate::slotCallAdded(Call* call)
{
    auto cat = getCategory(call->startTimeStamp());

    // This abuses a bit of implementation details of the history. The calls
    // are expected to arrive ordered by time_t, so this allows to take a
    // little shortcut and skip proper lookup. If this is to ever become a false
    // assumption, then this code will need to be updated.
    if ((!m_pCurrentCallGroup) || (m_pCurrentCallGroup->m_pParent != cat)) {
        m_pCurrentCallGroup = new PeerTimelineNode;
        m_pCurrentTextGroup = nullptr;

        m_pCurrentCallGroup->m_Type      = PeerTimelineModel::NodeType::CALL_GROUP;
        m_pCurrentCallGroup->m_StartTime = call->startTimeStamp();
        m_pCurrentCallGroup->m_EndTime   = call->stopTimeStamp ();

        for (int i=0; i < 4; i++) m_pCurrentCallGroup->m_lSummary[i] = 0;

        m_pCurrentCallGroup->m_pParent = cat;

        insert(
            m_pCurrentCallGroup, m_pCurrentCallGroup->m_StartTime, cat->m_lChildren,
            q_ptr->createIndex(cat->m_Index, 0, cat)
        );
    }

    auto ret         = new PeerTimelineNode;
    ret->m_pCall     = call;
    ret->m_Type      = PeerTimelineModel::NodeType::CALL;
    ret->m_StartTime = call->startTimeStamp();
    ret->m_EndTime   = call->stopTimeStamp();
    ret->m_pParent   = m_pCurrentCallGroup;

    insert(
        ret, ret->m_StartTime, m_pCurrentCallGroup->m_lChildren,
        q_ptr->createIndex(m_pCurrentCallGroup->m_Index, 0, m_pCurrentCallGroup)
    );

    // Update the group timelapse and counters
    m_pCurrentCallGroup->m_lSummary[
        (call->isMissed() ? 2 : 0) +
        (call->direction() == Call::Direction::INCOMING ? 0 : 1)
    ]++;

    m_pCurrentCallGroup->m_EndTime = ret->m_EndTime;

    const auto idx = q_ptr->createIndex(
        m_pCurrentCallGroup->m_pParent->m_Index, 0, m_pCurrentCallGroup->m_pParent
    );

    emit q_ptr->dataChanged(idx, idx);
}

/// To use with extreme restrict, this isn't really intended to be used directly
void PeerTimelineModel::addContactMethod(ContactMethod* cm)
{

    // Only add new content, also check for potential aliases
    foreach (auto ocm, d_ptr->m_hTrackedCMs) {
        if ( (*cm) == (*ocm))
            return;
    }

    const auto calls = cm->calls();

    for (auto c : qAsConst(calls))
        d_ptr->slotCallAdded(c);

    d_ptr->slotTextRecordingAdded(cm->textRecording());

    connect(cm, &ContactMethod::contactChanged,
        d_ptr, &PeerTimelineModelPrivate::slotContactChanged);

    connect(cm, &ContactMethod::rebased,
        d_ptr, &PeerTimelineModelPrivate::slotRebased);

    connect(cm, &ContactMethod::alternativeTextRecordingAdded,
        d_ptr, &PeerTimelineModelPrivate::slotTextRecordingAdded);

    const auto trs = cm->alternativeTextRecordings();
    for (auto t : qAsConst(trs))
        d_ptr->slotTextRecordingAdded(t);

    d_ptr->m_hTrackedCMs.insert(cm);
}

void PeerTimelineModelPrivate::slotPhoneNumberChanged()
{
    auto cms = m_pPerson->individual()->relatedContactMethods();
    cms.append(m_pPerson->individual()->phoneNumbers());

    for (auto cm : qAsConst(cms))
        q_ptr->addContactMethod(cm);

    //TODO detect removed CMs and reload
}

// It can happen if something hold a smart pointer for too long. This is a bug
// elsewhere, but given I can't fix Qt5::Quick internal GC race conditions, it
// mitigates potential crashes.
void PeerTimelineModelPrivate::slotPersonDestroyed()
{
    // The slots will be disconnected by QObject
    m_pPerson = nullptr;
    m_pCM     = nullptr;
    qWarning() << "A contact was destroyed while its timeline is referenced" << this;
}

void PeerTimelineModelPrivate::slotReload()
{
    // Eventually, this could be optimized to detect if `reset` is more efficient
    // than individual `move` operation and pick the "right" mode. For now,
    // `reset` creates more readable code, so that will be it.

    q_ptr->beginResetModel();

    // Keep the old sub-trees, there is no point in re-generating them
    std::list< std::vector<PeerTimelineNode*> > entries;

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

void PeerTimelineModelPrivate::slotClear(PeerTimelineNode* root)
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

void PeerTimelineModelPrivate::disconnectOldCms()
{
    // Both m_pPerson and m_pCM can be null if the smart pointer race condition
    // inside of Qt5::Quick happens

    if (m_pPerson) {
        disconnect(m_pPerson, &Person::callAdded,
            this, &PeerTimelineModelPrivate::slotCallAdded);

        disconnect(m_pPerson->individual().data(), &Individual::relatedContactMethodsAdded,
            this, &PeerTimelineModelPrivate::slotPhoneNumberChanged);

        disconnect(m_pPerson->individual().data(), &Individual::phoneNumbersChanged,
            this, &PeerTimelineModelPrivate::slotPhoneNumberChanged);

        // Add both phone number and whatever links to this person
        auto cms = m_pPerson->individual()->relatedContactMethods();
        cms.append(m_pPerson->individual()->phoneNumbers());

        for (auto cm : qAsConst(cms)) {
            disconnect(cm->textRecording()->d_ptr, &Media::TextRecordingPrivate::messageAdded,
                this, &PeerTimelineModelPrivate::slotMessageAdded);

            disconnect(cm, &ContactMethod::contactChanged,
                this, &PeerTimelineModelPrivate::slotContactChanged);

            disconnect(cm, &ContactMethod::alternativeTextRecordingAdded,
                this, &PeerTimelineModelPrivate::slotTextRecordingAdded);

            disconnect(cm, &ContactMethod::rebased,
                this, &PeerTimelineModelPrivate::slotRebased);
        }
    }
    else if (m_pCM) {
        if (auto textRec = m_pCM->textRecording())
            disconnect(textRec->d_ptr, &Media::TextRecordingPrivate::messageAdded,
            this, &PeerTimelineModelPrivate::slotMessageAdded);

        disconnect(m_pCM, &ContactMethod::alternativeTextRecordingAdded,
            this, &PeerTimelineModelPrivate::slotTextRecordingAdded);

        disconnect(m_pCM, &ContactMethod::callAdded,
            this, &PeerTimelineModelPrivate::slotCallAdded);

        disconnect(m_pCM, &ContactMethod::rebased,
            this, &PeerTimelineModelPrivate::slotRebased);

        disconnect(m_pCM, &ContactMethod::contactChanged,
            this, &PeerTimelineModelPrivate::slotContactChanged);
    }
}

/// Avoid showing mismatching data during/after merges to contact changes
void PeerTimelineModelPrivate::
slotContactChanged(Person* newContact, Person* oldContact)
{
    if ((!newContact) || newContact == oldContact)
        return;

    // Will happen if the original contact was a placeholder
    if (oldContact && newContact->uid() == oldContact->uid())
        return;

    if (m_pPerson)
        disconnect(m_pPerson, &QObject::destroyed,
            this, &PeerTimelineModelPrivate::slotPersonDestroyed);

    m_pPerson = newContact;

    connect(m_pPerson, &QObject::destroyed,
        this, &PeerTimelineModelPrivate::slotPersonDestroyed);

    // Tracking what can and cannot be salvaged isn't worth it
    disconnectOldCms();

    q_ptr->beginResetModel();
    slotClear();
    q_ptr->endResetModel();

    init();
}

/// Reload everything if the new and old CM base is incompatible.
void PeerTimelineModelPrivate::slotRebased(ContactMethod* other)
{
    if (other == sender())
        return;

    slotReload();
}

void PeerTimelineModelPrivate::
slotTextRecordingAdded(Media::TextRecording* r)
{
    // This will happen when ContactMethods are merged
    if (m_hTrackedTRs.contains(r))
        return;

    m_hTrackedTRs.insert(r);

    for (auto m : qAsConst(r->d_ptr->m_lNodes))
        slotMessageAdded(m);

    connect(r->d_ptr, &Media::TextRecordingPrivate::messageAdded,
        this, &PeerTimelineModelPrivate::slotMessageAdded);
}

#include <peertimelinemodel.moc>

// kate: space-indent on; indent-width 4; replace-tabs on;
