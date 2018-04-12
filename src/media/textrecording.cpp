/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                          *
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
#include "textrecording.h"

//Qt
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonDocument>
#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtCore/QFile>

//Daemon
#include <account_const.h>

//Ring
#include "callmodel.h"
#include "contactmethod.h"
#include "account.h"
#include "phonedirectorymodel.h"
#include "accountmodel.h"
#include "personmodel.h"
#include "private/textrecording_p.h"
#include "private/contactmethod_p.h"
#include "globalinstances.h"
#include "interfaces/pixmapmanipulatori.h"
#include "itemdataroles.h"
#include "mime.h"
#include "dbus/configurationmanager.h"
#include "private/textrecordingmodel.h"

//Std
#include <ctime>

Media::TextRecordingPrivate::TextRecordingPrivate(TextRecording* r) : QObject(r),
    q_ptr(r),m_pImModel(nullptr),m_pCurrentGroup(nullptr),m_UnreadCount(0)
{
}

Media::TextRecording::TextRecording(const Recording::Status status) : Recording(Recording::Type::TEXT, status), d_ptr(new TextRecordingPrivate(this))
{
}

Media::TextRecording::~TextRecording()
{
    d_ptr->clear();

    if (d_ptr->m_pUnreadTextMessagesModel)
        delete d_ptr->m_pUnreadTextMessagesModel;

    if (d_ptr->m_pImModel)
        delete d_ptr->m_pImModel;

    delete d_ptr;
}

/// Keep track of the number of messages in each states
bool Media::TextRecordingPrivate::performMessageAction(MimeMessage* m, MimeMessage::Actions a)
{
    const auto st = m->status();

    if (m->performAction(a)) {
        m_mMessageCounter.setAt(st         , m_mMessageCounter[ st          ]-1);
        m_mMessageCounter.setAt(m->status(), m_mMessageCounter[ m->status() ]+1);
        emit q_ptr->messageStateChanged();

        if (m->status() == MimeMessage::State::UNREAD) {
            emit q_ptr->unreadCountChange(1);
            //emit n->m_pContactMethod->unreadTextMessageCountChanged(); //FIXME
            //emit n->m_pContactMethod->changed();
        }
        else if (m->status() == MimeMessage::State::READ && st == MimeMessage::State::UNREAD) {
            emit q_ptr->unreadCountChange(-1);
            //emit n->m_pContactMethod->unreadTextMessageCountChanged(); //FIXME
            //emit n->m_pContactMethod->changed();
        }

        //TODO async save
        q_ptr->save();

        return true;
    }

    return false;
}

/// Keep track of the number of messages in each states
bool Media::TextRecordingPrivate::performMessageAction(MimeMessage* m, DRing::Account::MessageStates a)
{
    const auto st = m->status();

    const bool ret = m->performAction(a);

    if (st != m->status()) {
        m_mMessageCounter.setAt(st         , m_mMessageCounter[ st          ]-1);
        m_mMessageCounter.setAt(m->status(), m_mMessageCounter[ m->status() ]+1);
        emit q_ptr->messageStateChanged();
    }

    return ret;
}

/**
 * Updates the message status and potentially the message id, if a new status is set.
 * Returns true if the Message object was modified, false otherwise.
 */
bool Media::TextRecordingPrivate::updateMessageStatus(MimeMessage* m, DRing::Account::MessageStates newSatus)
{
    bool modified = false;

    if (static_cast<int>(newSatus) >= static_cast<int>(MimeMessage::State::COUNT__)) {
        qWarning() << "Unknown message status with code: " << static_cast<int>(newSatus);
        newSatus = DRing::Account::MessageStates::UNKNOWN;
    }
    else {
        //READ status is not used yet it'll be the final state when it is
        if (newSatus == DRing::Account::MessageStates::READ
                || newSatus == DRing::Account::MessageStates::SENT
                || newSatus == DRing::Account::MessageStates::FAILURE) {
            m_hPendingMessages.remove(m->id());
            if (m->id() != 0) {
                //m->id = 0; TODO figure out why this was put there
                modified = true;
            }
        }
    }

    // The int comparison is safe because both enum class share the same first
    // few elements.
    if ((int)m->status() != (int)newSatus) {
        performMessageAction(m, newSatus);
        modified = true;
    }

    return modified;
}

void Media::TextRecordingPrivate::accountMessageStatusChanged(const uint64_t id, DRing::Account::MessageStates status)
{
    if (auto node = m_hPendingMessages.value(id, nullptr)) {
        if (updateMessageStatus(node->m_pMessage, status)) {
            //You're looking at why local file storage is a "bad" idea
            q_ptr->save();

            if (m_pImModel) {
                auto msg_index = m_pImModel->index(node->m_row, 0);
                m_pImModel->dataChanged(msg_index, msg_index);
            }
        }
    }
}

bool Media::TextRecording::hasMimeType(const QString& mimeType) const
{
    return d_ptr->m_hMimeTypes.contains(mimeType);
}

QStringList Media::TextRecording::mimeTypes() const
{
    return d_ptr->m_lMimeTypes;
}

///Get the instant messaging model associated with this recording
QAbstractItemModel* Media::TextRecording::instantMessagingModel() const
{
    if (!d_ptr->m_pImModel) {
        d_ptr->m_pImModel = new TextRecordingModel(const_cast<TextRecording*>(this));
    }

    return d_ptr->m_pImModel;
}

void Media::TextRecording::setAsRead(MimeMessage* m) const
{
    d_ptr->performMessageAction(m, MimeMessage::Actions::READ );
}

void Media::TextRecording::setAsUnread(MimeMessage* m) const
{
    d_ptr->performMessageAction(m, MimeMessage::Actions::UNREAD );
}

///Set all messages as read and then save the recording
void Media::TextRecording::setAllRead()
{
    bool changed = false;
    for(int row = 0; row < d_ptr->m_lNodes.size(); ++row) {
        if (d_ptr->m_lNodes[row]->m_pMessage->status() != MimeMessage::State::READ) {

            d_ptr->performMessageAction(
                d_ptr->m_lNodes[row]->m_pMessage,
                MimeMessage::Actions::READ
            );

            if (d_ptr->m_pImModel) {
                auto idx = d_ptr->m_pImModel->index(row, 0);
                emit d_ptr->m_pImModel->dataChanged(idx,idx);
            }
            changed = true;
        }
    }
    if (changed) {
        // TODO: we assume that the CM is the same for now, and that at least some of the messages
        //       are text
        int oldVal = d_ptr->m_UnreadCount;
        d_ptr->m_UnreadCount = 0;
        emit unreadCountChange(-oldVal);
        emit d_ptr->m_lNodes[0]->m_pContactMethod->unreadTextMessageCountChanged();
        emit d_ptr->m_lNodes[0]->m_pContactMethod->changed();
        save();
    }
}

QStringList Media::TextRecording::paths() const
{
    QStringList ret;

    for (const auto p : qAsConst(d_ptr->m_lAssociatedPeers)) {
        ret << p->sha1s[0].toLatin1()+QStringLiteral(".json");
    }

    return ret;
}

QSet<ContactMethod*> Media::TextRecording::peers() const
{
    QSet<ContactMethod*> cms;

    for (const auto peers : qAsConst(d_ptr->m_lAssociatedPeers)) {
        for (auto cm : qAsConst(peers->peers))
            cms.insert(cm);
    }

    return cms;
}

/**
 * I (Emmanuel Lepage) is in the process of writing a better one for this that
 * can be upstreamed into Qt (there is interest in merging a generic QVariant
   filter model), however, it is too complex to merge into LRC for such basic
   use case. So, for the sake of simplicity until upstream have this feature,
   here is a subset of the generic filter proxy. The time between now and the
   Qt review + Qt release + LRC drop old version of Qt is too long anyway.

   A copy of the code (copyrighted by me) is available in the ring-kde
   next release for those interested. In 2016-2017, this code could probably
   be replaced by the new one, be it in KItemModels (the KDE abstract proxy
   library) or QtCore.
 */
class TextProxyModel : public QSortFilterProxyModel
{
public:
    explicit TextProxyModel(QObject* parent) : QSortFilterProxyModel(parent){}
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
    {
        const QModelIndex srcIdx = sourceModel()->index(source_row, filterKeyColumn(), source_parent);

        return srcIdx.data((int)Media::TextRecording::Role::HasText).toBool();
    }
};

/**
 * Subset of the instantMessagingModel() with only plain text and HTML
 * messages. This model can be displayed directly to the user.
 */
QAbstractItemModel* Media::TextRecording::instantTextMessagingModel() const
{
    if (!d_ptr->m_pTextMessagesModel) {
        auto p = new TextProxyModel(const_cast<TextRecording*>(this));
        p->setSourceModel(instantMessagingModel());
        d_ptr->m_pTextMessagesModel = p;
    }

    return d_ptr->m_pTextMessagesModel;
}

/**
 * Proxy model to get the unread text messages, as well as their number (rowCount)
 */
class UnreadProxyModel final : public QSortFilterProxyModel
{
public:
    explicit UnreadProxyModel(QObject* parent) : QSortFilterProxyModel(parent){}
    virtual ~UnreadProxyModel() {}
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override
    {
        const QModelIndex srcIdx = sourceModel()->index(source_row, filterKeyColumn(), source_parent);

        return !srcIdx.data((int)Media::TextRecording::Role::IsRead).toBool();
    }
};

/**
 * Subset of the instantTextMessagingModel() with only unread plain text and HTML
 * messages. This model can be used to get the number of unread messages.
 */
QAbstractItemModel* Media::TextRecording::unreadInstantTextMessagingModel() const
{
    if (!d_ptr->m_pUnreadTextMessagesModel) {
        auto p = new UnreadProxyModel(instantTextMessagingModel());
        p->setSourceModel(instantTextMessagingModel());
        d_ptr->m_pUnreadTextMessagesModel = p;
    }

    return d_ptr->m_pUnreadTextMessagesModel;
}


bool Media::TextRecording::isEmpty() const
{
   return !size();
}


int Media::TextRecording::unreadCount() const
{
    return d_ptr->m_mMessageCounter[MimeMessage::State::UNREAD];
}

int Media::TextRecording::sendingCount() const
{
    return d_ptr->m_mMessageCounter[MimeMessage::State::SENDING];
}

int Media::TextRecording::sentCount() const
{
    return d_ptr->m_mMessageCounter[MimeMessage::State::SENT];
}

int Media::TextRecording::receivedCount() const
{
    return d_ptr->m_mMessageCounter[MimeMessage::State::READ]
        + d_ptr->m_mMessageCounter[MimeMessage::State::UNREAD];
}

int Media::TextRecording::unknownCount() const
{
    return d_ptr->m_mMessageCounter[MimeMessage::State::UNKNOWN];
}

int Media::TextRecording::size() const
{
    return d_ptr->m_lNodes.size();
}

// Qt convention compat
int Media::TextRecording::count() const { return size(); }

QHash<QByteArray,QByteArray> Media::TextRecordingPrivate::toJsons() const
{
    QHash<QByteArray,QByteArray> ret;

    int groups = 0;

    for (const auto p : qAsConst(m_lAssociatedPeers)) {
        p->hasChanged = false;

        QJsonObject output;
        p->write(output);

        groups += p->groups.size();

        QJsonDocument doc(output);
        ret[p->sha1s[0].toLatin1()] = doc.toJson();
    }

    // Try to GC empty section when messages are deleted
    if (!groups)
        return {};

   return ret;
}

time_t Media::TextRecording::lastUsed() const
{
    return d_ptr->m_LastUsed;
}

QList<Serializable::Group*> Media::TextRecordingPrivate::allGroups() const
{
    QList<Serializable::Group*> ret;

    for (auto p : qAsConst(m_lAssociatedPeers)) {
        for (auto g : qAsConst(p->groups)) {
            ret << g;
        }
    }

    return ret;
}

Media::TextRecording* Media::TextRecording::fromJson(const QList<QJsonObject>& items, const QString& path, ContactMethod* cm, CollectionInterface* backend)
{

    // If it's loaded from a JSON file, assume it's consumed until proven otherwise
    auto t = new TextRecording(Recording::Status::CONSUMED);

    if (backend)
        t->setCollection(backend);

    //Load the history data
    for (const QJsonObject& obj : qAsConst(items))
        t->d_ptr->m_lAssociatedPeers << SerializableEntityManager::fromJson(obj, path, cm);

    //Create the model
    bool statusChanged = false; // if a msg status changed during parsing, we need to re-save the model

    //Reconstruct the conversation
    //TODO do it right, right now it flatten the graph
    for (auto p : qAsConst(t->d_ptr->m_lAssociatedPeers)) {
        //Seems old version didn't store that
        if (p->peers.isEmpty())
            continue;
        // TODO: for now assume the convo is with only 1 CM at a time
        ContactMethod* peerCM = (*p->peers.constBegin());

        // get the latest timestamp to set last used
        time_t lastUsed = 0;
        for (auto g : qAsConst(p->groups)) {
            for (const auto& m : qAsConst(g->messagesRef())) {
                auto n  = new ::TextMessageNode(t);
                n->m_pGroup    = g;
                n->m_pMessage  = m.first;
                n->m_pCM       = m.second;
                if (!n->m_pCM) {
                    if (cm) {
                        n->m_pCM = const_cast<ContactMethod*>(cm); //TODO remove in 2016
                        n->m_AuthorSha1 = cm->sha1();

                        if (p->peers.isEmpty())
                            p->addPeer(cm);
                    } else {
                        if (p->m_hSha1.contains(n->m_AuthorSha1)) {
                            n->m_pCM = p->m_hSha1[n->m_AuthorSha1];
                        } else {
                            // message was outgoing and author sha1 was set to that of the sending account
                            n->m_pCM = peerCM;
                            n->m_AuthorSha1 = peerCM->sha1();
                        }
                    }
                }

                // Keep track of the number of entries (per state)
                t->d_ptr->m_mMessageCounter.setAt(m.first->status(), t->d_ptr->m_mMessageCounter[m.first->status()]+1);

                n->m_pContactMethod = n->m_pCM; //FIXME deadcode

                t->d_ptr->m_lNodes << n;

                if (lastUsed < n->m_pMessage->timestamp())
                    lastUsed = n->m_pMessage->timestamp();

                t->d_ptr->m_LastUsed = std::max(t->d_ptr->m_LastUsed, m.first->timestamp());

                //FIXME for now the message status from older sessions is ignored, 99% of time it makes no
                // sense.

                //static auto& configurationManager = ConfigurationManager::instance();

                //if (m->id()) {
                    //int status = configurationManager.getMessageStatus(m->id());
                    //t->d_ptr->m_hPendingMessages[m->id()] = n;
                    //if (t->d_ptr->updateMessageStatus(m, static_cast<DRing::Account::MessageStates>(status)))
                    //    statusChanged = true;
                //}
            }
        }

        emit t->messageStateChanged();

        if (statusChanged)
            t->save();

        // update the timestamp of the CM
        peerCM->d_ptr->setLastUsed(lastUsed);
    }

    return t;
}

Media::TextRecording* Media::TextRecording::fromPath(const QString& path, const Metadata& metadata, CollectionInterface* backend)
{
    Q_UNUSED(metadata)

    QString content;

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open text recording json file";
        return nullptr;
    }

    content = QString::fromUtf8(file.readAll());

    if (content.isEmpty()) {
        qWarning() << "Text recording file is empty";
        return nullptr;
    }

    QJsonParseError err;
    QJsonDocument loadDoc = QJsonDocument::fromJson(content.toUtf8(), &err);

    if (err.error != QJsonParseError::ParseError::NoError) {
        qWarning() << "Error Decoding Text Message History Json" << err.errorString();
        return nullptr;
    }

    return fromJson({loadDoc.object()}, path, nullptr, backend);
}

void Media::TextRecordingPrivate::initGroup(MimeMessage::Type t, ContactMethod* cm)
{
    //Create new groups when:
    // * None was found on the disk
    // * The media type changed
    // * [TODO] Once a timeout is reached

    if ((!m_pCurrentGroup) || (m_pCurrentGroup->size()
      && m_pCurrentGroup->messagesRef().constLast().first->type() != t)) {
        auto cMethod = q_ptr->call() ? q_ptr->call()->peerContactMethod() : cm;

        Q_ASSERT(cMethod);

        m_pCurrentGroup = new Serializable::Group(cMethod->account());
        m_pCurrentGroup->type = t;

        auto p = SerializableEntityManager::peer(cMethod);

        if (m_lAssociatedPeers.indexOf(p) == -1)
            m_lAssociatedPeers << p;

        m_pCurrentGroup->m_pParent = SerializableEntityManager::peer(cm);

        p->groups << m_pCurrentGroup;
    }
    else {
        m_pCurrentGroup->addPeer(cm);
    }
}

void Media::TextRecordingPrivate::insertNewSnapshot(Call* call, const QString& path)
{
    initGroup(MimeMessage::Type::SNAPSHOT);

    auto m = MimeMessage::buildFromSnapshot(path);

    // Save the snapshot metadata

    m_pCurrentGroup->addMessage(m, call->peerContactMethod());

    // Insert the message
    ::TextMessageNode* n  = new ::TextMessageNode(q_ptr);
    n->m_pMessage         = m;
    n->m_pContactMethod   = call->peerContactMethod();
    n->m_row              = m_lNodes.size();
    n->m_pGroup           = m_pCurrentGroup;
    n->m_AuthorSha1       = call->peerContactMethod()->sha1();

    m_mMessageCounter.setAt(m->status(), m_mMessageCounter[m->status()]+1);

    emit q_ptr->aboutToInsertMessage(
        {{RingMimes::SNAPSHOT, path}}, n->m_pContactMethod, m->direction()
    );

    m_lNodes << n;

    if (n->m_pContactMethod->lastUsed() < m->timestamp())
        n->m_pContactMethod->d_ptr->setLastUsed(m->timestamp());

    m_LastUsed = std::max(m_LastUsed, m->timestamp());

    // Save the conversation
    q_ptr->save();

    emit q_ptr->messageInserted(
        {{RingMimes::SNAPSHOT, path}}, n->m_pContactMethod, m->direction()
    );

    emit q_ptr->messageStateChanged();

    emit messageAdded(n);
}

void Media::TextRecordingPrivate::insertNewMessage(const QMap<QString,QString>& message, ContactMethod* cm, Media::Media::Direction direction, uint64_t id)
{
    initGroup(MimeMessage::Type::CHAT, cm);

    auto m = MimeMessage::buildNew(message, direction, id);

    // Some messages may contain profile or ring-specific metadata, those are
    // not text messages and don't belong here.
    if (!m)
        return;

    for (auto i = message.constBegin(); i != message.constEnd(); i++) {
        const int currentSize = m_hMimeTypes.size();

        //This one is currently useless
        if (i.value() == QLatin1String("application/resource-lists+xml"))
            continue;

        // Make the clients life easier and tell the payload type
        const int hasArgs = i.key().indexOf(';');
        const QString strippedMimeType = hasArgs != -1 ? i.key().left(hasArgs) : i.key();

        m_hMimeTypes[strippedMimeType] = true;

        if (currentSize != m_hMimeTypes.size())
            m_lMimeTypes << strippedMimeType;
    }

    m_pCurrentGroup->addMessage(m, cm);

    //Update the reconstructed conversation
    auto n               = new ::TextMessageNode(q_ptr);
    n->m_pMessage        = m;
    n->m_pContactMethod  = const_cast<ContactMethod*>(cm);
    n->m_row             = m_lNodes.size();
    n->m_pGroup          = m_pCurrentGroup;
    n->m_AuthorSha1      = cm->sha1();

    m_mMessageCounter.setAt(m->status(), m_mMessageCounter[m->status()]+1);

    emit q_ptr->aboutToInsertMessage(message, const_cast<ContactMethod*>(cm), direction);

    m_lNodes << n;

    //FIXME the states need to be set, maybe assume SENDING when the id > 0
    if (m->id() > 0 && m->status() == MimeMessage::State::SENDING)
        m_hPendingMessages[id] = n;

    //Save the conversation
    q_ptr->save();


    if (cm->lastUsed() < m->timestamp())
        cm->d_ptr->setLastUsed(m->timestamp());

    m_LastUsed = std::max(m_LastUsed, m->timestamp());

    emit q_ptr->messageInserted(message, const_cast<ContactMethod*>(cm), direction);
    if (m->status() != MimeMessage::State::READ) { //FIXME
        m_UnreadCount += 1;
        emit q_ptr->unreadCountChange(1);
        emit cm->unreadTextMessageCountChanged();
        emit cm->changed();
    }

    emit q_ptr->messageStateChanged();

    emit messageAdded(n);
}

MimeMessage* Media::TextRecording::messageAt(int row) const
{
    if (row >= d_ptr->m_lNodes.size() || row < 0)
        return nullptr;

    return d_ptr->m_lNodes[row]->m_pMessage;
}

QVariant Media::TextRecording::roleData(int role) const
{
    switch(role) {
        case Qt::DisplayRole:
            return peers().size() ? (*peers().constBegin())->primaryName() : roleData(-1, Qt::DisplayRole);
        case (int) Ring::Role::Length:
            return QString::number(size()) + tr(" elements");
        case (int) Ring::Role::FormattedLastUsed:
            return d_ptr->m_lNodes.isEmpty() ? tr("N/A") :
                QDateTime::fromTime_t(d_ptr->m_lNodes.last()->m_pMessage->timestamp()).toString();
    }

    return {};
}


QVariant TextMessageNode::snapshotRoleData(int role) const
{
    Q_ASSERT(!m_pMessage->payloads().isEmpty());

    switch(role) {
        case Qt::DisplayRole:
            return m_pMessage->payloads().first()->payload;
    }

    return {};
}

QVariant TextMessageNode::roleData(int role) const
{
    if (m_pMessage->type() == MimeMessage::Type::SNAPSHOT)
        return snapshotRoleData(role);

    switch (role) {
        case Qt::DisplayRole:
            return QVariant(m_pMessage->plainText());
        case Qt::DecorationRole         :
            if (m_pMessage->direction() == Media::Media::Direction::IN)
                return GlobalInstances::pixmapManipulator().decorationRole(m_pContactMethod);
            else if (m_pContactMethod->account())
                return GlobalInstances::pixmapManipulator().decorationRole(
                    m_pContactMethod->account()->contactMethod()
                );
            else {
                /* It's most likely an account that doesn't exist anymore
                * Use a fallback image in pixmapManipulator
                */
                return GlobalInstances::pixmapManipulator().decorationRole((ContactMethod*)nullptr);
            }
            break;
        case (int)Media::TextRecording::Role::Direction            :
            return QVariant::fromValue(m_pMessage->direction());
        case (int)Media::TextRecording::Role::AuthorDisplayname    :
        case (int)Ring::Role::Name                                 :
            if (m_pMessage->direction() == Media::Media::Direction::IN)
                return m_pContactMethod->roleData(static_cast<int>(Ring::Role::Name));
            else
                return QObject::tr("Me");
        case (int)Media::TextRecording::Role::AuthorUri            :
        case (int)Ring::Role::Number                               :
            return m_pContactMethod->uri();
        case (int)Media::TextRecording::Role::AuthorPresenceStatus :
            // Always consider "self" as present
            if (m_pMessage->direction() == Media::Media::Direction::OUT)
                return true;
            else
                return m_pContactMethod->contact() ?
                m_pContactMethod->contact()->isPresent() : m_pContactMethod->isPresent();
        case (int)Media::TextRecording::Role::Timestamp            :
            return (uint)m_pMessage->timestamp();
        case (int)Media::TextRecording::Role::IsRead               :
            return m_pMessage->status() != MimeMessage::State::UNREAD;
        case (int)Media::TextRecording::Role::FormattedDate        :
            return QDateTime::fromTime_t(m_pMessage->timestamp()).toString();
        case (int)Media::TextRecording::Role::IsStatus             :
            return m_pMessage->type() == MimeMessage::Type::STATUS;
        case (int)Media::TextRecording::Role::HTML                 :
            return QVariant(m_pMessage->html());
        case (int)Media::TextRecording::Role::HasText              :
            return m_pMessage->hasText();
        case (int)Media::TextRecording::Role::ContactMethod        :
            return QVariant::fromValue(m_pContactMethod);
        case (int)Media::TextRecording::Role::DeliveryStatus       :
            return QVariant::fromValue(m_pMessage->status());
        case (int)Media::TextRecording::Role::FormattedHtml        :
            return QVariant::fromValue(m_pMessage->getFormattedHtml());
        case (int)Media::TextRecording::Role::LinkList             :
            return QVariant::fromValue(m_pMessage->linkList());
        case (int)Media::TextRecording::Role::Id                   :
            return QVariant::fromValue(m_pMessage->id());
        default:
            break;
    }

    return {};
}

QVariant Media::TextRecording::roleData(int row, int role) const
{
    if (row < -d_ptr->m_lNodes.size() || row >= d_ptr->m_lNodes.size())
        return {};

    // Allow reverse lookup too as the last messages are the most relevant
    if (row < 0)
        row = d_ptr->m_lNodes.size()+row;

    Q_ASSERT(row < d_ptr->m_lNodes.size());

    return d_ptr->m_lNodes[row]->roleData(role);
}

void Media::TextRecordingPrivate::clear()
{
    emit q_ptr->aboutToClear();

    for ( TextMessageNode *node : m_lNodes)
        delete node;

    m_lNodes.clear();

    m_lAssociatedPeers.clear();

    m_pCurrentGroup = nullptr;
    m_hMimeTypes.clear();
    m_lMimeTypes.clear();

    emit q_ptr->cleared();

    if (m_UnreadCount != 0) {
        m_UnreadCount = 0;
        emit q_ptr->unreadCountChange(0);
    }
}

void Media::TextRecording::discard()
{
    d_ptr->clear();
    //FIXME delete all paths();
}

void Media::TextRecording::consume()
{
    setAllRead();
}

// kate: space-indent on; indent-width 4; replace-tabs on;
