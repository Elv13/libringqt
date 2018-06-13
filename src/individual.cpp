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
#include "individual.h"

// Ring
#include <contactmethod.h>
#include <numbercategory.h>
#include <categorizedhistorymodel.h>
#include <phonedirectorymodel.h>
#include <person.h>
#include <account.h>
#include "libcard/eventaggregate.h"
#include <infotemplatemanager.h>
#include <infotemplate.h>
#include <accountmodel.h>
#include <individualeditor.h>
#include <certificatemodel.h>
#include <individualtimelinemodel.h>
#include "media/textrecording.h"
#include "contactmethod_p.h"
#include "person_p.h"
#include "callmodel.h"
#include "globalinstances.h"
#include "interfaces/pixmapmanipulatori.h"

/** The ::Individual properties associated with each ContactMethod.
 *
 * This is possible because a ContactMethod only has a single ::Individual
 * object.
 */
struct ContactMethodIndividualData
{
    /// Used for the model
    int m_PhoneNumberIndex {-1};
    IndividualPrivate* d_ptr {nullptr};
};

class IndividualPrivate final : public QObject
{
    Q_OBJECT
public:

    explicit IndividualPrivate() {
        moveToThread(QCoreApplication::instance()->thread());
        setParent(&PhoneDirectoryModel::instance());
    }

    // This is a private class, no need for d_ptr
    Person* m_pPerson {nullptr};
    bool m_RecordingsInit {false};
    QMetaObject::Connection m_cBeginCB;
    QMetaObject::Connection m_cEndCB;
    TemporaryContactMethod* m_pTmpCM {nullptr};
    QString m_BestName;

    ContactMethod*           m_LastUsedCM {nullptr};
    QVector<ContactMethod*>  m_HiddenContactMethods;
    Person::ContactMethods   m_Numbers             ;

    QVector<Media::TextRecording*> m_lRecordings;

    QWeakPointer<IndividualTimelineModel> m_TimelineModel;

    QSharedPointer<EventAggregate> m_pEventAggregate;

    // Helpers
    void connectContactMethod(ContactMethod* cm);
    void disconnectContactMethod(ContactMethod* cm);
    InfoTemplate* infoTemplate();
    bool merge(Individual* other);
    bool contains(ContactMethod* cm, bool includeHidden = true) const;

    QList<Individual*> m_lParents;

    Individual* q_ptr;

public Q_SLOTS:
    void slotLastUsedTimeChanged(::time_t t       );
    void slotLastContactMethod  ( ContactMethod* cm       );
    void slotCallAdded          ( Call *call              );
    void slotUnreadCountChanged (                         );
    void slotCmDestroyed        (                         );
    void slotContactChanged     (                         );
    void slotChanged            (                         );
    void slotRegisteredName     (                         );
    void slotBookmark           ( bool b                  );
    void slotMediaAvailabilityChanged();
    void slotHasActiveCallChanged();

    void slotChildrenContactChanged(Person* newContact, Person* oldContact);
    void slotChildrenTextRecordingAdded(Media::TextRecording* t);
    void slotChildrenRebased(ContactMethod* other  );
};

Individual::Individual(Person* parent) :
    QAbstractListModel(nullptr)
{
    moveToThread(QCoreApplication::instance()->thread());
    setParent(parent);
    d_ptr = new IndividualPrivate();
    d_ptr->m_pPerson  = parent;
    d_ptr->q_ptr      = this;

    // Row inserted/deleted can be implemented later //FIXME
    d_ptr->m_cBeginCB = connect(this, &Individual::phoneNumbersAboutToChange, this, [this](){beginResetModel();});
    d_ptr->m_cEndCB   = connect(this, &Individual::phoneNumbersChanged      , this, [this](){endResetModel  ();});
    d_ptr->m_lParents << this;
    moveToThread(QCoreApplication::instance()->thread());
    setObjectName(parent->formattedName());


    connect(parent, &Person::formattedNameChanged, d_ptr,
        &IndividualPrivate::slotRegisteredName);
}

Individual::Individual() : QAbstractListModel(&PhoneDirectoryModel::instance()), d_ptr(new IndividualPrivate)
{
    d_ptr->q_ptr = this;
    d_ptr->m_lParents << this;
    moveToThread(QCoreApplication::instance()->thread());
}

Individual::~Individual()
{
    // If anything is still listening, make sure at least is gets a proper cleanup
    d_ptr->m_Numbers.clear();
    d_ptr->m_HiddenContactMethods.clear();

    if (QSharedPointer<IndividualTimelineModel> tl = d_ptr->m_TimelineModel)
        tl->clear();

    d_ptr->m_TimelineModel = nullptr;

    disconnect( d_ptr->m_cEndCB   );
    disconnect( d_ptr->m_cBeginCB );

    setEditRow(false);

    d_ptr->m_lParents.removeAll(this);

    if (d_ptr->m_lParents.isEmpty())
        delete d_ptr;
}

bool IndividualPrivate::contains(ContactMethod* cm, bool includeHidden) const
{
    bool ret = false;
    q_ptr->forAllNumbers([&ret, cm](ContactMethod* other) {
        if (other->d() == cm->d()) {
            ret = true;
            return;
        }
    }, includeHidden);

    return ret;
}

/**
 * This will happen when an individual was already created and contact is then
 * added to one of the contact method. The trick is to "fix" this silently by
 * swapping the d_ptr. Once the QSharedPointer<Individual> loses it's last
 * references it gets deleted and everyone is happy.
 */
bool IndividualPrivate::merge(Individual* other)
{
    // It will happen when the Person re-use the first individual it finds.
    // (it's done on purpose)
    if (other->d_ptr == this)
        return false;

    if (Q_UNLIKELY(m_pPerson && other->d_ptr->m_pPerson && !(*m_pPerson == *other->d_ptr->m_pPerson))) {
        qWarning() << "Trying to merge 2 incompatible individuals";
        return false;
    }

    other->d_ptr->m_lParents << q_ptr;
    other->d_ptr->m_BestName.clear();

    if ((!other->d_ptr->m_LastUsedCM) || (m_LastUsedCM && m_LastUsedCM->lastUsed() > other->d_ptr->m_LastUsedCM->lastUsed()))
        other->d_ptr->m_LastUsedCM =  m_LastUsedCM;

    for (auto cm : qAsConst(m_Numbers)) {
        if (cm->d_ptr->m_pIndividualData && cm->d_ptr->m_pIndividualData->d_ptr == this) {
            delete cm->d_ptr->m_pIndividualData;
            cm->d_ptr->m_pIndividualData = nullptr;
        }

        if (!other->d_ptr->contains(cm))
            other->addPhoneNumber(cm);
    }

    for (auto cm : qAsConst(m_HiddenContactMethods)) {
        if (!other->d_ptr->contains(cm))
            other->registerContactMethod(cm);
    }

    //TODO merge the event aggregates

    m_lParents.removeAll(q_ptr);

    // Marge all the copies
    for (auto q : qAsConst(m_lParents)) {
        if (other->d_ptr->m_lParents.indexOf(q) == -1) {
            other->d_ptr->m_lParents << q;
            q->d_ptr = other->d_ptr;
            emit q_ptr->layoutChanged();
        }
    }

    q_ptr->d_ptr = other->d_ptr;

    emit q_ptr->layoutChanged();

    emit PhoneDirectoryModel::instance().individualMerged(q_ptr, other);

    delete this;

    return true;
}

QVariant Individual::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    if (d_ptr->m_pTmpCM && index.row() >= phoneNumbers().size()) {
        Q_ASSERT(index.row() == phoneNumbers().size());
        Q_ASSERT(d_ptr->m_pTmpCM->type() == ContactMethod::Type::TEMPORARY);
        Q_ASSERT(d_ptr->m_pTmpCM->roleData((int)ContactMethod::Role::Type).toInt() == (int)ContactMethod::Type::TEMPORARY);
        return d_ptr->m_pTmpCM->roleData(role);
    }

    const auto cm = phoneNumbers()[index.row()];

    // As this model is always associated with the person, the relevant icon
    // is the phone number type (category)
    if (role == Qt::DecorationRole)
        return cm->category()->icon();

    return cm->roleData(role);
}

bool Individual::setData( const QModelIndex& index, const QVariant &value, int role)
{
    if (index.row() == rowCount()) {
        setEditRow(true);
    }
    else if (!index.isValid())
        return false;

    if (index.row() == rowCount() - 1 && d_ptr->m_pTmpCM)
        return d_ptr->m_pTmpCM->setRoleData(value, role);

    return phoneNumbers()[index.row()]->setRoleData(value, role);
}

int Individual::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : phoneNumbers().size() + (
        d_ptr->m_pTmpCM ? 1 : 0
    );
}

// Re-implement to allow adding new rows
QModelIndex Individual::index(int row, int col, const QModelIndex& parent) const
{
    if (col || parent.isValid() || row < 0 || row >= rowCount())
        return {};

    return createIndex(row, 0, row);
}

bool Individual::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row >= rowCount())
        return false;

    if (row == rowCount()-1) {
        setEditRow(false);
        return true;
    }

    auto pn = phoneNumbers();
    for (int i = row; i < row+count; i++)
        pn.remove(row);

    setPhoneNumbers(pn);

    emit layoutChanged();

    return true;
}

QHash<int,QByteArray> Individual::roleNames() const
{
    static QHash<int, QByteArray> roles = PhoneDirectoryModel::instance().roleNames();
    static bool initRoles = false;
    if (!initRoles) {
        initRoles = true;
    }

    return roles;
}

Call* Individual::firstActiveCall() const
{
    return CallModel::instance().firstActiveCall(const_cast<Individual*>(this));
}


QString Individual::bestName() const
{
    if (!d_ptr->m_BestName.isEmpty())
        return d_ptr->m_BestName;

    Person* p = nullptr;
    QString firstRegisteredName, display, firstUri;

    forAllNumbers([&p, &firstRegisteredName, &display, &firstUri](ContactMethod* cm) {
        if (cm->contact())
            p = cm->contact();

        // The phone numbers are first are have priority
        if (cm->registeredName().size() && firstRegisteredName.isEmpty())
            firstRegisteredName = cm->registeredName();

        if (firstUri.isEmpty())
            firstUri = cm->uri();

        if (display.isEmpty() && cm->primaryName() != cm->uri())
            display = cm->primaryName();
    });

    if (d_ptr->m_pPerson && !p)
        p = d_ptr->m_pPerson;

    // Can be empty for new contacts
    if (p)
        d_ptr->m_BestName = p->formattedName();

    if (d_ptr->m_BestName.isEmpty() && !firstRegisteredName.isEmpty())
        d_ptr->m_BestName = firstRegisteredName;
    else if (d_ptr->m_BestName.isEmpty() && !display.isEmpty())
        d_ptr->m_BestName = display;
    else if (d_ptr->m_BestName.isEmpty() && !firstUri.isEmpty())
        d_ptr->m_BestName = firstUri;
    else if (d_ptr->m_BestName.isEmpty())
        d_ptr->m_BestName = tr("Unknown");

    Q_ASSERT(!d_ptr->m_BestName.isEmpty());

    d_ptr->q_ptr->setObjectName(d_ptr->m_BestName);

    return d_ptr->m_BestName;
}

QString Individual::lastUsedUri() const
{
    return lastUsedContactMethod() ? QString(lastUsedContactMethod()->uri()) : QStringLiteral("N/A");
}

QVariant Individual::roleData(int role) const
{
    switch (role) {
        case Qt::DisplayRole:
        case static_cast<int>(Ring::Role::Name):
        case static_cast<int>(Call::Role::Name):
            return bestName();
        case static_cast<int>(Ring::Role::Number):
        case static_cast<int>(Call::Role::Number):
            // Use the most recent or nothing
            return lastUsedContactMethod() ? lastUsedContactMethod()->bestId() : QString();
        case Qt::DecorationRole:
            return d_ptr->m_pPerson ?
                GlobalInstances::pixmapManipulator().decorationRole(d_ptr->m_pPerson) : QVariant();
        case static_cast<int>(Ring::Role::LastUsed):
        case static_cast<int>(Call::Role::Date):
            return lastUsedContactMethod() ?
                QVariant::fromValue(lastUsedContactMethod()->lastUsed()) : QVariant::fromValue((time_t)0);
        case static_cast<int>(Ring::Role::FormattedLastUsed):
        case static_cast<int>(Call::Role::FormattedDate):
        case static_cast<int>(Call::Role::FuzzyDate):
            return HistoryTimeCategoryModel::timeToHistoryCategory(
                lastUsedContactMethod() ? lastUsedContactMethod()->lastUsed() : 0
            );
        case static_cast<int>(Ring::Role::IndexedLastUsed):
            return QVariant(static_cast<int>(HistoryTimeCategoryModel::timeToHistoryConst(
                lastUsedContactMethod() ? lastUsedContactMethod()->lastUsed() : 0
            )));
        case static_cast<int>(Call::Role::ContactMethod):
        case static_cast<int>(Ring::Role::Object):
            return QVariant::fromValue(const_cast<Individual*>(this));
            //return QVariant::fromValue(lastUsedContactMethod());
            //return QVariant::fromValue(IndividualPointer(d_ptr->m_pWeakRef));
        case static_cast<int>(Ring::Role::ObjectType):
            return QVariant::fromValue(Ring::ObjectType::Individual);
        case static_cast<int>(Call::Role::IsBookmark):
        case static_cast<int>(Ring::Role::IsBookmarked):
            return hasBookmarks();
        case static_cast<int>(Ring::Role::IsPresent):
        case static_cast<int>(Call::Role::IsPresent):
            return hasProperty<&ContactMethod::isPresent>();
        case static_cast<int>(ContactMethod::Role::IsReachable):
            return hasProperty<&ContactMethod::isReachable>();
        case static_cast<int>(ContactMethod::Role::TotalCallCount):
            return propertySum<&ContactMethod::callCount>();
        case static_cast<int>(Ring::Role::UnreadTextMessageCount):
            return unreadTextMessageCount();
        case static_cast<int>(Ring::Role::IsRecording):
            return hasProperty<&ContactMethod::isRecording>();
        case static_cast<int>(Ring::Role::HasActiveCall):
            return hasProperty<&ContactMethod::hasActiveCall>();
        case static_cast<int>(Ring::Role::HasActiveVideo):
            return hasProperty<&ContactMethod::hasActiveVideo>();

//       case static_cast<int>(Role::CanCall): //TODO
//           return canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
//       case static_cast<int>(Role::CanVideoCall):
//           return canVideoCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
//       case static_cast<int>(Role::CanSendTexts):
//           return canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
//       case static_cast<int>(Role::TotalEventCount):
//           return EventModel::instance().d_ptr->events(this).size();
//       case static_cast<int>(Role::TotalMessageCount):
//           if (auto rec = textRecording())
//             cat = rec->sentCount() + rec->receivedCount();
//          else
//             cat = 0;
//          break;

    }

    return {};
}

Person* Individual::person() const
{
    return d_ptr->m_pPerson;
}

int Individual::unreadTextMessageCount() const
{
    int unread = 0;

    QSet<Media::TextRecording*> trs;

    forAllNumbers([&unread, &trs](ContactMethod* cm) {
        if (cm->hasTextRecordings()) {
            auto rec = cm->textRecording();
            if ((!rec) || !trs.contains(rec)) {
                unread += rec->unreadCount();
                trs << rec;
            }
        }
    });

    return unread;
}

bool Individual::hasEditRow() const
{
    return d_ptr->m_pTmpCM;
}

void Individual::setEditRow(bool v)
{
    const int idx = rowCount();

    if (v && !d_ptr->m_pTmpCM) {
        beginInsertRows({}, idx, idx);
        d_ptr->m_pTmpCM = new TemporaryContactMethod();
        endInsertRows();
    }
    else if ((!v) && d_ptr->m_pTmpCM) {
        beginRemoveRows({}, idx-1, idx-1);
        delete d_ptr->m_pTmpCM;
        d_ptr->m_pTmpCM = 0;
        endRemoveRows();
    }

    emit hasEditRowChanged(hasEditRow());
}

QSharedPointer<EventAggregate> Individual::eventAggregate() const
{
    if (!d_ptr->m_pEventAggregate) {
        d_ptr->m_pEventAggregate = EventAggregate::build(
            const_cast<Individual*>(this)
        );
    }

    return d_ptr->m_pEventAggregate;
}

void IndividualPrivate::connectContactMethod(ContactMethod* m)
{
    connect(m, &ContactMethod::destroyed, this,
        &IndividualPrivate::slotCmDestroyed);

    connect(m, &ContactMethod::lastUsedChanged, this,
        &IndividualPrivate::slotLastUsedTimeChanged);

    connect(m, &ContactMethod::callAdded, this,
        &IndividualPrivate::slotCallAdded);

    connect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotChildrenContactChanged);

    connect(m, &ContactMethod::textRecordingAdded, this,
        &IndividualPrivate::slotChildrenTextRecordingAdded);

    connect(m, &ContactMethod::rebased, this,
        &IndividualPrivate::slotChildrenRebased);

    connect(m, &ContactMethod::unreadTextMessageCountChanged, this,
        &IndividualPrivate::slotUnreadCountChanged);

    connect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotContactChanged);

    connect(m, &ContactMethod::changed, this,
        &IndividualPrivate::slotChanged);

    connect(m, &ContactMethod::registeredNameSet, this,
        &IndividualPrivate::slotRegisteredName);

    connect(m, &ContactMethod::primaryNameChanged, this,
        &IndividualPrivate::slotRegisteredName);

    connect(m, &ContactMethod::bookmarkedChanged, this,
        &IndividualPrivate::slotBookmark);

    connect(m, &ContactMethod::mediaAvailabilityChanged, this,
        &IndividualPrivate::slotMediaAvailabilityChanged);

    connect(m, &ContactMethod::hasActiveCallChanged, this,
        &IndividualPrivate::slotHasActiveCallChanged);

//     connect(m, SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
//     connect(m, SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
}

void IndividualPrivate::disconnectContactMethod(ContactMethod* m)
{
    disconnect(m, &ContactMethod::destroyed, this,
        &IndividualPrivate::slotCmDestroyed);

    disconnect(m, &ContactMethod::lastUsedChanged, this,
        &IndividualPrivate::slotLastUsedTimeChanged);

    disconnect(m, &ContactMethod::callAdded, this,
        &IndividualPrivate::slotCallAdded);

    disconnect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotChildrenContactChanged);

    disconnect(m, &ContactMethod::textRecordingAdded, this,
        &IndividualPrivate::slotChildrenTextRecordingAdded);

    disconnect(m, &ContactMethod::rebased, this,
        &IndividualPrivate::slotChildrenRebased);

    disconnect(m, &ContactMethod::unreadTextMessageCountChanged, this,
        &IndividualPrivate::slotUnreadCountChanged);

    disconnect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotContactChanged);

    disconnect(m, &ContactMethod::changed, this,
        &IndividualPrivate::slotChanged);

    disconnect(m, &ContactMethod::primaryNameChanged, this,
        &IndividualPrivate::slotRegisteredName);

    disconnect(m, &ContactMethod::registeredNameSet, this,
        &IndividualPrivate::slotRegisteredName);

    disconnect(m, &ContactMethod::bookmarkedChanged, this,
        &IndividualPrivate::slotBookmark);

    disconnect(m, &ContactMethod::mediaAvailabilityChanged, this,
        &IndividualPrivate::slotMediaAvailabilityChanged);

    disconnect(m, &ContactMethod::hasActiveCallChanged, this,
        &IndividualPrivate::slotHasActiveCallChanged);

//     disconnect(m, SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
//     disconnect(m, SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
}

void Individual::registerContactMethod(ContactMethod* m)
{
    d_ptr->m_HiddenContactMethods << m;
    d_ptr->m_BestName.clear();

    emit relatedContactMethodsAdded(m);

    d_ptr->connectContactMethod(m);

    if ((!d_ptr->m_LastUsedCM) || m->lastUsed() > d_ptr->m_LastUsedCM->lastUsed())
        d_ptr->slotLastContactMethod(m);

    if (objectName().isEmpty())
        setObjectName(m->primaryName());
}

///Get the phone number list
QVector<ContactMethod*> Individual::phoneNumbers() const
{
   return d_ptr->m_Numbers;
}

QVector<Media::TextRecording*> Individual::textRecordings() const
{
    // Avoid initializing in constructor, it causes deadlocks
    if (!d_ptr->m_RecordingsInit) {
        d_ptr->m_RecordingsInit = true;

        forAllNumbers([this](ContactMethod* cm) {
            if (cm->hasTextRecordings())
                d_ptr->slotChildrenTextRecordingAdded(cm->textRecording());

            const auto trs = cm->alternativeTextRecordings();

            for (auto t : qAsConst(trs))
                d_ptr->slotChildrenTextRecordingAdded(t);
        });
    }

    return d_ptr->m_lRecordings;
}

/**
 * Over time, more ContactMethods are associated with a Person. For example,
 * if the person move and get a new phone number (and lose the old one) or
 * change country. Other example could be losing the Ring account private
 * key and having to create a new one. There is other accidental instances
 * caused by various race conditions.
 */
QVector<ContactMethod*> Individual::relatedContactMethods() const
{
    return d_ptr->m_HiddenContactMethods;
}

///Set the phone number (type and number)
void Individual::setPhoneNumbers(const QVector<ContactMethod*>& numbers)
{
    //TODO make a diff instead of full reload
    const auto dedup = QSet<ContactMethod*>::fromList(numbers.toList());

    emit phoneNumbersAboutToChange();

    for (ContactMethod* n : qAsConst(d_ptr->m_Numbers))
        d_ptr->disconnectContactMethod(n);

    d_ptr->m_Numbers = QVector<ContactMethod*>::fromList(dedup.toList());

    for (ContactMethod* n : qAsConst(d_ptr->m_Numbers))
        d_ptr->connectContactMethod(n);

    emit phoneNumbersChanged();

    //Allow incoming calls from those numbers
    const QList<Account*> ringAccounts = AccountModel::instance().getAccountsByProtocol(Account::Protocol::RING);
    QStringList certIds;
    for (ContactMethod* n : qAsConst(d_ptr->m_Numbers)) {
        if (n->uri().protocolHint() == URI::ProtocolHint::RING)
            certIds << n->uri().userinfo(); // certid must only contain the hash, no scheme
    }

    for (const QString& hash : qAsConst(certIds)) {
        Certificate* cert = CertificateModel::instance().getCertificateFromId(hash);
        if (cert) {
            for (Account* a : ringAccounts) {
                if (a->allowIncomingFromContact())
                a->allowCertificate(cert);
            }
        }
    }
}

ContactMethod* Individual::addPhoneNumber(ContactMethod* cm)
{
    if ((!cm) || cm->type() == ContactMethod::Type::BLANK)
        return nullptr;

    for (auto cm2 : qAsConst(d_ptr->m_Numbers)) {
        if (Q_UNLIKELY(cm2->d() == cm->d())) {
            qWarning() << this << "already has the phone number" << cm;
            return cm;
        }
    }

    if (cm->type() == ContactMethod::Type::TEMPORARY)
        cm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(cm)
        );

    // Useful for debugging, but normal for Individual created using
    // cm->individual()
    //if (!d_ptr->m_pPerson)
    //    qDebug() << "Trying to add a new phone number without a contact" << cm;

    if (Q_UNLIKELY(cm->contact() && !(*cm->contact() == *d_ptr->m_pPerson))) {
        qWarning() << "Adding a phone number to" << this << "already owned by" << cm->contact();
        Q_ASSERT(false);
    }

    if (d_ptr->m_pPerson)
        cm->setPerson(d_ptr->m_pPerson);

    d_ptr->connectContactMethod(cm);

    if ((!d_ptr->m_LastUsedCM) || cm->lastUsed() > d_ptr->m_LastUsedCM->lastUsed())
        d_ptr->slotLastContactMethod(cm);

    if (cm->d_ptr->m_pIndividualData) {
        //Note: There is a race condition here when 2 thread load 2 contacts
        if (cm->d_ptr->m_pIndividualData->d_ptr != d_ptr) {
            if (cm->individual()->person() == person())
                d_ptr->merge(cm->individual());
            else
                Q_ASSERT(false);
        }
        cm->d_ptr->m_pIndividualData->m_PhoneNumberIndex = d_ptr->m_Numbers.size();
    }
    else
        cm->d_ptr->m_pIndividualData = new ContactMethodIndividualData {
            d_ptr->m_Numbers.size(),
            d_ptr
        };

    beginInsertRows({}, d_ptr->m_Numbers.size(), d_ptr->m_Numbers.size());
    d_ptr->m_Numbers << cm;
    d_ptr->m_BestName.clear();
    endInsertRows();

    emit phoneNumbersChanged();
    emit relatedContactMethodsAdded(cm);

    if (d_ptr->m_HiddenContactMethods.indexOf(cm) != -1) {
        d_ptr->m_HiddenContactMethods.removeAll(cm);
        emit relatedContactMethodsRemoved(cm);
    }

    if (Q_UNLIKELY(cm->d_ptr->m_pIndividual && cm->d_ptr->m_pIndividual->d_ptr != d_ptr)) {
        qWarning() << cm << "already has an individual attached" << cm->d_ptr->m_pIndividual <<
            "cannot set" << this;
    }
    else if (!cm->d_ptr->m_pIndividual)
        cm->d_ptr->m_pIndividual = this;
    else if (cm->d_ptr->m_pIndividual->d_ptr != d_ptr) {
        if (d_ptr->m_pPerson && ((!cm->contact()) || cm->contact()->d_ptr == d_ptr->m_pPerson->d_ptr)) {
            Individual* ind = cm->d_ptr->m_pIndividual;
            ind->d_ptr->merge(d_ptr->m_pPerson->individual());
        }
        else
            Q_ASSERT(false);
    }

    if (objectName().isEmpty())
        setObjectName(cm->primaryName());

    return cm;
}

ContactMethod* Individual::removePhoneNumber(ContactMethod* cm)
{
    if (Q_UNLIKELY(!cm)) {
        return nullptr;
    }

    const int idx = d_ptr->m_Numbers.indexOf(cm);

    if (Q_UNLIKELY(idx == -1)) {
        qWarning() << this << "trying to replace a phone number that doesn't exist";
        return nullptr;
    }

    phoneNumbersAboutToChange();
    beginRemoveRows({}, idx, idx);
    d_ptr->m_Numbers.remove(idx);

    for (int i =0; i < d_ptr->m_Numbers.size(); i++) {
        auto cm = d_ptr->m_Numbers[i];

        if (!cm->d_ptr->m_pIndividualData)
            cm->d_ptr->m_pIndividualData = new ContactMethodIndividualData {
                -1,
                d_ptr
            };
        else
            Q_ASSERT(cm->d_ptr->m_pIndividualData->d_ptr == d_ptr);

        cm->d_ptr->m_pIndividualData->m_PhoneNumberIndex = i;
    }

    endRemoveRows();
    phoneNumbersChanged();

    if (cm->d_ptr->m_pIndividualData && cm->d_ptr->m_pIndividualData->d_ptr == d_ptr) {
        delete cm->d_ptr->m_pIndividualData;
        cm->d_ptr->m_pIndividualData = nullptr;
    }

    d_ptr->m_HiddenContactMethods << cm;

    emit relatedContactMethodsAdded(cm);

    return cm;
}

/// ContactMethods URI are immutable, they need to be replaced when edited
ContactMethod* Individual::replacePhoneNumber(ContactMethod* old, ContactMethod* newCm)
{
    if (Q_UNLIKELY((!newCm) || (!old))) {
        qWarning() << this << "trying to replace a phone number with nothing";
        return nullptr;
    }

    const int idx = d_ptr->m_Numbers.indexOf(old);

    if (Q_UNLIKELY((!old) || idx == -1)) {
        qWarning() << this << "trying to replace a phone number that doesn't exist";
        return nullptr;
    }

    if (old->d() == newCm->d()) {
        qWarning() << "Trying to replace a phone number with itself";
        return old;
    }

    if (old->d_ptr->m_pIndividualData) {
        delete old->d_ptr->m_pIndividualData;
        old->d_ptr->m_pIndividualData = nullptr;
    }

    if (newCm->type() == ContactMethod::Type::TEMPORARY)
        newCm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(newCm)
        );

    phoneNumbersAboutToChange();

    Q_ASSERT(!newCm->d_ptr->m_pIndividualData);

    d_ptr->m_Numbers.replace(idx, newCm);

    for (int i =0; i < d_ptr->m_Numbers.size(); i++) {
        auto cm = d_ptr->m_Numbers[i];

        if (!cm->d_ptr->m_pIndividualData)
            cm->d_ptr->m_pIndividualData = new ContactMethodIndividualData {
                -1,
                d_ptr
            };
        else
            Q_ASSERT(cm->d_ptr->m_pIndividualData->d_ptr == d_ptr);

        cm->d_ptr->m_pIndividualData->m_PhoneNumberIndex = i;
    }

    const auto midx = index(idx, 0);

    emit dataChanged(midx, midx);

    d_ptr->m_HiddenContactMethods << newCm;
    d_ptr->connectContactMethod(newCm);
    d_ptr->disconnectContactMethod(old);

    d_ptr->m_BestName.clear();

    emit relatedContactMethodsAdded(old);

    phoneNumbersChanged();

    return newCm;
}

/// Get the last ContactMethod used with that person.
ContactMethod* Individual::lastUsedContactMethod() const
{
    return d_ptr->m_LastUsedCM;
}

ContactMethod* Individual::mainContactMethod() const
{
    // If that's not true, then there is ambiguities that cannot be resolved
    // using an algorithm. A choice has to be presented to the user.
    if (phoneNumbers().size() != 1)
        return nullptr;

    // It may or may not be an hidden ContactMethod, it's not important, it's
    // attached to this individual and known to work. If it was wrongly
    // attached, then the bug is elsewhere and this condition stays true.
    if (auto cm = lastUsedContactMethod())
        return cm;

    // It will get there is the user was never reached
    return phoneNumbers().first();
}

ContactMethod* Individual::preferredContactMethod(int m) const
{
    if (m < 0 || m >= (int)Media::Media::Type::COUNT__)
        return nullptr;

    return preferredContactMethod((Media::Media::Type)m);
}

ContactMethod* Individual::preferredContactMethod(Media::Media::Type m) const
{
    ContactMethod* ret = nullptr;

    auto check = [m, &ret](ContactMethod* cm) {
        if (!cm)
            return;

        switch (m) {
            case Media::Media::Type::AUDIO:
                if (cm->canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE)
                    ret = cm;
                break;
            case Media::Media::Type::VIDEO:
                if (cm->canVideoCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE)
                    ret = cm;
                break;
            case Media::Media::Type::TEXT:
                if (cm->canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE)
                    ret = cm;
                break;
            case Media::Media::Type::FILE:
            case Media::Media::Type::COUNT__:
                break;
        }
    };

    check(mainContactMethod());

    if (ret)
        return ret;

    forAllNumbers([m, &ret, check](ContactMethod* cm) {
        if (ret)
            return;

        check(cm);
    });

    return ret;
}

bool Individual::requireUserSelection() const
{
    return !mainContactMethod();
}

bool Individual::hasHiddenContactMethods() const
{
    return !d_ptr->m_HiddenContactMethods.isEmpty();
}

/**
 * Returns a timeline model if and only if a person phone number was contacted.
 *
 * For "self manager" Person objects, it means it is *required* to set the phone
 * numbers right.
 */
QSharedPointer<QAbstractItemModel> Individual::timelineModel() const
{
    if (!d_ptr->m_TimelineModel) {
        auto p = QSharedPointer<IndividualTimelineModel>(new IndividualTimelineModel(
            const_cast<Individual*>(this)
        ));
        d_ptr->m_TimelineModel = p;
        return p;
    }

    return d_ptr->m_TimelineModel;
}

void IndividualPrivate::slotLastUsedTimeChanged(::time_t t)
{
   Q_UNUSED(t)

   auto cm = qobject_cast<ContactMethod*>(sender());

   // This function should always be called by a contactmethod signal. Otherwise
   // the timestamp comes from an unknown source and cannot be traced.
   Q_ASSERT(cm);

   slotLastContactMethod(cm);
}

void IndividualPrivate::slotLastContactMethod(ContactMethod* cm)
{
    if (cm && q_ptr->lastUsedTime() > cm->lastUsed())
        return;

    m_LastUsedCM = cm;

    emit q_ptr->lastUsedTimeChanged(cm ? cm->lastUsed() : 0);
}

void IndividualPrivate::slotCallAdded(Call *call)
{
    emit q_ptr->callAdded(call);
}

void IndividualPrivate::slotUnreadCountChanged()
{
    emit q_ptr->unreadCountChanged(q_ptr->unreadTextMessageCount());
}

void IndividualPrivate::slotCmDestroyed()
{
    auto cm = qobject_cast<ContactMethod*>(sender());

    // Some Qt versions emit this "too late" and the metaObject is already
    // deleted. It's not a bug, just an implementation detail. All slots
    // are already disconnected.
    if (!cm)
        cm = static_cast<ContactMethod*>(sender());
    else
        disconnectContactMethod(cm);

    m_HiddenContactMethods.removeAll(cm);
    m_Numbers.removeAll(cm);
}

void IndividualPrivate::slotChildrenContactChanged(Person* newContact, Person* oldContact)
{
    m_BestName.clear();
    auto cm = qobject_cast<ContactMethod*>(sender());
    emit q_ptr->childrenContactChanged(cm, newContact, oldContact);
}

void IndividualPrivate::slotChildrenTextRecordingAdded(Media::TextRecording* t)
{
    if (!m_lRecordings.contains(t))
        m_lRecordings << t;

    emit q_ptr->textRecordingAdded(t);
}

void IndividualPrivate::slotChildrenRebased(ContactMethod* other)
{
    auto cm = qobject_cast<ContactMethod*>(sender());

    // Merge the individual. This assume the contacts are the same
    if (cm->d_ptr->m_pIndividual && cm->d_ptr->m_pIndividual->d_ptr == this) {
        merge(cm->d_ptr->m_pIndividual);
    }

    emit q_ptr->childrenRebased(cm, other);
}

void IndividualPrivate::slotContactChanged()
{
    auto cm = qobject_cast<ContactMethod*>(sender());

    if ((!m_pPerson) && cm->contact())
        merge(cm->contact()->individual());
    else if (Individual* ind = cm->d_ptr->m_pIndividual) {
        if (m_pPerson)
            ind->d_ptr->merge(m_pPerson->individual());
    }

    if (cm->d_ptr->m_pIndividualData && cm->d_ptr->m_pIndividualData->d_ptr != this) {
        delete cm->d_ptr->m_pIndividualData;

        cm->d_ptr->m_pIndividualData = contains(cm, false) ? new ContactMethodIndividualData {
            -1,
            this
        } : nullptr;

        for (int i =0; i < m_Numbers.size(); i++) {
            auto cm = m_Numbers[i];
            Q_ASSERT(cm->d_ptr->m_pIndividualData);
            cm->d_ptr->m_pIndividualData->m_PhoneNumberIndex = i;
        }
    }
}

void IndividualPrivate::slotChanged()
{
    auto cm = qobject_cast<ContactMethod*>(sender());

    if (cm->d_ptr->m_pIndividualData && cm->d_ptr->m_pIndividualData->d_ptr == this) {
        const auto idx = q_ptr->index(cm->d_ptr->m_pIndividualData->m_PhoneNumberIndex, 0);

        emit q_ptr->dataChanged(idx, idx);
    }

//     static bool blocked = false;
//
//     if (blocked)
//         return;
//
//
//     blocked = true;
//
//     // Make sure the models get up-to-date data even if they watch only CMs
//     q_ptr->forAllNumbers([cm](ContactMethod* other) {
//         if (cm != other)
//             emit other->changed();
//     });
//
//     blocked = false;



    emit q_ptr->changed();
}

void IndividualPrivate::slotRegisteredName()
{
    m_BestName.clear();
    emit q_ptr->changed();
}

void IndividualPrivate::slotBookmark(bool b)
{
    auto cm = qobject_cast<ContactMethod*>(sender());

    emit q_ptr->bookmarkedChanged(cm, b);
}

void IndividualPrivate::slotMediaAvailabilityChanged()
{
    emit q_ptr->mediaAvailabilityChanged();
}

void IndividualPrivate::slotHasActiveCallChanged()
{
    emit q_ptr->hasActiveCallChanged();
}

/// Like std::any_of
bool Individual::matchExpression(const std::function<bool(ContactMethod*)>& functor) const
{
    for (const auto l : {phoneNumbers(), relatedContactMethods()}) {
        for (const auto cm : qAsConst(l))
            if (functor(cm))
                return true;
    }

    return false;
}

void Individual::forAllNumbers(const std::function<void(ContactMethod*)> functor, bool indludeHidden) const
{
    const auto nbs = phoneNumbers();
    for (const auto cm : qAsConst(nbs))
        functor(cm);

    if (indludeHidden) {
        const auto nb2 = relatedContactMethods();
        for (const auto cm : qAsConst(nb2))
            functor(cm);
    }
}


bool Individual::isSelf() const
{
    return hasProperty<&ContactMethod::isSelf>();
}

bool Individual::isDuplicate() const
{
    return d_ptr->q_ptr != this;
}

/** Get the last time this person was contacted.
 * This method returns zero when the person was never contacted.
 *  @todo Implement some caching
 */
time_t Individual::lastUsedTime() const
{
    if (!lastUsedContactMethod())
        return 0;

    return lastUsedContactMethod()->lastUsed();
}

QString Individual::formattedLastUsedTime() const
{
    return QDateTime::fromTime_t(lastUsedTime()).toString();
}

int Individual::callCount() const
{
    return propertySum<&ContactMethod::callCount>();
}

int Individual::textMessageCount() const
{
    const auto recs = textRecordings();
    int ret = 0;

    for (const auto r : qAsConst(recs))
        ret += r->size();

    return ret;
}

int Individual::totalSpentTime() const
{
    return propertySum<&ContactMethod::totalSpentTime>();
}

bool Individual::hasPhoneNumbers() const
{
    return !d_ptr->m_Numbers.isEmpty();
}

bool Individual::hasBookmarks() const
{
    return hasProperty<&ContactMethod::isBookmarked>();
}

ContactMethod* Individual::firstBookmark() const
{
    ContactMethod* cm = nullptr;

    forAllNumbers([&cm](ContactMethod* cm2) {
        if ((!cm) && cm2->isBookmarked())
            cm = cm2;
    });

    return cm;
}

Person* Individual::buildPerson() const
{
    if (d_ptr->m_pPerson)
        return d_ptr->m_pPerson;

    Person* p = nullptr;

    forAllNumbers([&p](ContactMethod* cm) {
        if ((!p) && cm->contact())
            p = cm->contact();
    }, false);

    if (!p) {
        p = new Person();
        p->setFormattedName(bestName());
        p->d_ptr->m_pIndividual = const_cast<Individual*>(this);
    }

    d_ptr->m_pPerson = p;

    connect(p, &Person::formattedNameChanged, d_ptr,
        &IndividualPrivate::slotRegisteredName);

    for (auto cm : qAsConst(d_ptr->m_Numbers)) {
        if (!cm->contact())
            cm->setPerson(p);
    }

    return p;
}

bool Individual::markAsRead()
{
    forAllNumbers([](ContactMethod* cm) {
        if (cm->hasTextRecordings()) {
            auto rec = cm->textRecording();
            if (rec && rec->unreadCount())
                rec->setAllRead();
        }
    });

    return true;
}

Individual* Individual::getIndividual(ContactMethod* cm)
{
    if (auto i = cm->d_ptr->m_pIndividual)
        return i;

    if (cm->contact())
        return getIndividual(cm->contact());

    auto i = new Individual();

    // need to be done after WeakRef is set, do not move to constructor
    i->addPhoneNumber(cm);

    return i;
}

Individual* Individual::getIndividual(Person* p)
{
    if (auto i = p->d_ptr->m_pIndividual)
        return i;

    // strong ref
    p->d_ptr->m_pIndividual = new Individual(p);

    return p->d_ptr->m_pIndividual;
}

Individual* Individual::getIndividual(const QList<ContactMethod*>& cms)
{
    if (cms.isEmpty())
        return {};

    Individual* ind {nullptr};

    QSet<Account*> accs;
    QHash<Individual*, int> existing;
    QSet<Person*> persons;

    // Find the most common Individual for deduplication purpose
    for (auto cm : qAsConst(cms)) {
        if(auto i = cm->d_ptr->m_pIndividual)
            existing[i]++;

        if (auto a = cm->account())
            accs << a;

        if (auto p = cm->contact())
            persons << p;
    }

    int max = 0;
    for(auto i = existing.constBegin(); i != existing.constEnd();i++) {
        if (i.key() && i.value() > max) {
            max = i.value();
            ind = i.key();
        }
    }

    if (existing.size() > 1) {
        qWarning() << "getIndividual was called with a set containing more"
            "than 1 existing individual, this will produce undefined hehavior";
        Q_ASSERT(false);
    }

    if (persons.size() > 1) {
        qWarning() << "getIndividual was called with a set containing more"
            "than 1 person, this will produce undefined hehavior";
        Q_ASSERT(false);
    }

    // Create one is none was found
    if (!ind) {
        ind = (!persons.isEmpty()) ?
            getIndividual(*persons.constBegin()) : getIndividual(cms.first());
    }

    // Set the Individual to each empty CM
    for (auto cm : qAsConst(cms)) {
        if (!cm->d_ptr->m_pIndividual)
            cm->d_ptr->m_pIndividual = ind;
    }

    // If there is person with different individual, print a warning, it's a bug
    for (auto p : qAsConst(persons)) {
        if (!p->d_ptr->m_pIndividual) {
            p->d_ptr->m_pIndividual = ind;
            Q_ASSERT(false);
        }
        else if (p->d_ptr->m_pIndividual->d_ptr != ind->d_ptr) {
            qWarning() << p << "has a more than one individual identity, this is a bug";
            Q_ASSERT(false);
        }
    }

    return ind;
}

// QSharedPointer<EventAggregate> Individual::events(FlagPack<Event::EventCategory> categories)
// {
//     return {};
// }

InfoTemplate* IndividualPrivate::infoTemplate()
{
    return InfoTemplateManager::instance().defaultInfoTemplate();
}

/**
 * Volatile editor object the user interface can use to encapsulate the logic
 * behind modifying an individual, creating contacts and so on.
 */
QSharedPointer<IndividualEditor> Individual::createEditor() const
{
    return QSharedPointer<IndividualEditor>(
        new IndividualEditor(
            const_cast<Individual*>(this),
            d_ptr->infoTemplate()
        )
    );
}

bool Individual::canCall() const
{
    return matchExpression([this](ContactMethod* cm) {
        return cm->canCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
}

bool Individual::canVideoCall() const
{
    return matchExpression([this](ContactMethod* cm) {
        return cm->canVideoCall() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
}

bool Individual::canSendTexts() const
{
    return matchExpression([this](ContactMethod* cm) {
        return cm->canSendTexts() == ContactMethod::MediaAvailailityStatus::AVAILABLE;
    });
}

bool Individual::isAvailable() const
{
    return hasProperty<&ContactMethod::isAvailable>();
}

QModelIndex Individual::defaultIndex() const
{
    if (d_ptr->m_Numbers.isEmpty())
        return {};

    ContactMethod* cm = nullptr;
    int idx = 0;

    for (int i = 0; i < d_ptr->m_Numbers.size(); i++) {
        auto other = d_ptr->m_Numbers[i];
        if ((!cm) || other->lastUsed() > cm->lastUsed()) {
            cm = other;
            idx = i;
        }
    }

    return index(idx, 0);
}

bool Individual::isOffline() const
{
    bool ret = true;
    forAllNumbers([&ret](ContactMethod* cm) {
        if (!ret)
            return;

        // There is no way to know, assume the individual is present
        if (!cm->account()) {
            ret = false;
            return;
        }

        // SIP tracking is unreliable, assume it's wrong
        if (cm->account()->protocol() != Account::Protocol::RING) {
            ret = false;
            return;
        }

        // The individual is present
        if (cm->lastUsed() && cm->isTracked() && cm->isPresent())
            ret = false;

    }, false);
}

void* Individual::d() const
{
    return d_ptr;
}

#include <individual.moc>
