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
#include <infotemplatemanager.h>
#include <infotemplate.h>
#include <accountmodel.h>
#include <individualeditor.h>
#include <certificatemodel.h>
#include <individualtimelinemodel.h>
#include "media/textrecording.h"
#include "contactmethod_p.h"
#include "person_p.h"

class IndividualPrivate final : public QObject
{
    Q_OBJECT
public:

    // This is a private class, no need for d_ptr
    Person* m_pPerson {nullptr};
    bool m_RecordingsInit {false};
    QMetaObject::Connection m_cBeginCB;
    QMetaObject::Connection m_cEndCB;
    TemporaryContactMethod* m_pTmpCM {nullptr};

    ContactMethod*           m_LastUsedCM {nullptr};
    QVector<ContactMethod*>  m_HiddenContactMethods;
    Person::ContactMethods   m_Numbers             ;

    QVector<Media::TextRecording*> m_lRecordings;

    QWeakPointer<IndividualTimelineModel> m_TimelineModel;

    QWeakPointer<Individual> m_pWeakRef;

    // Helpers
    void connectContactMethod(ContactMethod* cm);
    void disconnectContactMethod(ContactMethod* cm);
    InfoTemplate* infoTemplate();

    Individual* q_ptr;

public Q_SLOTS:
    void slotLastUsedTimeChanged(::time_t t       );
    void slotLastContactMethod  (ContactMethod* cm);
    void slotCallAdded          (Call *call       );
    void slotUnreadCountChanged (                 );

    void slotChildrenContactChanged(Person* newContact, Person* oldContact);
    void slotChildrenTextRecordingAdded(Media::TextRecording* t);
    void slotChildrenRebased(ContactMethod* other  );
};

Individual::Individual(Person* parent) :
    QAbstractListModel(const_cast<Person*>(parent)), d_ptr(new IndividualPrivate)
{
    d_ptr->m_pPerson  = parent;
    d_ptr->q_ptr      = this;

    // Row inserted/deleted can be implemented later //FIXME
    d_ptr->m_cBeginCB = connect(this, &Individual::phoneNumbersAboutToChange, this, [this](){beginResetModel();});
    d_ptr->m_cEndCB   = connect(this, &Individual::phoneNumbersChanged      , this, [this](){endResetModel  ();});
}

Individual::Individual() : QAbstractListModel(&PhoneDirectoryModel::instance()), d_ptr(new IndividualPrivate)
{
    d_ptr->q_ptr = this;

}

Individual::~Individual()
{
    disconnect( d_ptr->m_cEndCB   );
    disconnect( d_ptr->m_cBeginCB );

    setEditRow(false);

    delete d_ptr;
}

QVariant Individual::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    if (d_ptr->m_pTmpCM && index.row() >= phoneNumbers().size()) {
        Q_ASSERT(index.row() == phoneNumbers().size());

        switch(role) {
            case Qt::DisplayRole:
            case Qt::EditRole:
                return d_ptr->m_pTmpCM->uri();
            case (int) ContactMethod::Role::CategoryName:
                return d_ptr->m_pTmpCM->category() ?
                    d_ptr->m_pTmpCM->category()->name() : QString();
            case static_cast<int>(Call::Role::ContactMethod):
            case static_cast<int>(Ring::Role::Object):
                return QVariant::fromValue(d_ptr->m_pTmpCM);
        }

        return {};
    }

    // As this model is always associated with the person, the relevant icon
    // is the phone number type (category)
    if (index.isValid() && role == Qt::DecorationRole) {
        return phoneNumbers()[index.row()]->category()->icon();
    }

    return index.isValid() ? phoneNumbers()[index.row()]->roleData(role) : QVariant();
}

bool Individual::setData( const QModelIndex& index, const QVariant &value, int role)
{
    if (index.row() == rowCount()) {
        beginInsertRows({}, rowCount(), rowCount());
        setEditRow(true);
        endInsertRows();
    }
    else if (!index.isValid())
        return false;

    switch(role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            d_ptr->m_pTmpCM->setUri(value.toString());
            emit dataChanged(index, index);
            return true;
    }

    return false;
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
    if (col || parent.isValid())
        return {};

    if (row > phoneNumbers().size()+(d_ptr->m_pTmpCM ? -1 : 0))
        return {};

    return createIndex(row, 0, row);
}

bool Individual::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row >= rowCount())
        return false;

    if (row == rowCount()-1) {
        setEditRow(false);
        emit layoutChanged();
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

bool Individual::hasEditRow() const
{
    return d_ptr->m_pTmpCM;
}

void Individual::setEditRow(bool v)
{
    if (v && !d_ptr->m_pTmpCM) {
        d_ptr->m_pTmpCM = new TemporaryContactMethod();
    }
    else if ((!v) && d_ptr->m_pTmpCM)
        delete d_ptr->m_pTmpCM;

    emit hasEditRowChanged(v);
    emit layoutChanged();
}

void IndividualPrivate::connectContactMethod(ContactMethod* m)
{
    connect(m, &ContactMethod::lastUsedChanged, this,
        &IndividualPrivate::slotLastUsedTimeChanged);

    connect(m, &ContactMethod::callAdded, this,
        &IndividualPrivate::slotCallAdded);

    connect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotChildrenContactChanged);

    connect(m, &ContactMethod::alternativeTextRecordingAdded, this,
        &IndividualPrivate::slotChildrenTextRecordingAdded);

    connect(m, &ContactMethod::rebased, this,
        &IndividualPrivate::slotChildrenRebased);

    connect(m, &ContactMethod::unreadTextMessageCountChanged, this,
        &IndividualPrivate::slotUnreadCountChanged);

//     connect(m, SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
//     connect(m, SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
}

void IndividualPrivate::disconnectContactMethod(ContactMethod* m)
{
    disconnect(m, &ContactMethod::lastUsedChanged, this,
        &IndividualPrivate::slotLastUsedTimeChanged);

    disconnect(m, &ContactMethod::callAdded, this,
        &IndividualPrivate::slotCallAdded);

    disconnect(m, &ContactMethod::contactChanged, this,
        &IndividualPrivate::slotChildrenContactChanged);

    disconnect(m, &ContactMethod::alternativeTextRecordingAdded, this,
        &IndividualPrivate::slotChildrenTextRecordingAdded);

    disconnect(m, &ContactMethod::rebased, this,
        &IndividualPrivate::slotChildrenRebased);

    disconnect(m, &ContactMethod::unreadTextMessageCountChanged, this,
        &IndividualPrivate::slotUnreadCountChanged);

//     disconnect(m, SIGNAL(presentChanged(bool)),d_ptr,SLOT(slotPresenceChanged()));
//     disconnect(m, SIGNAL(trackedChanged(bool)),d_ptr,SLOT(slotTrackedChanged()));
}

void Individual::registerContactMethod(ContactMethod* m)
{
    d_ptr->m_HiddenContactMethods << m;

    emit relatedContactMethodsAdded(m);

    d_ptr->connectContactMethod(m);

    if ((!d_ptr->m_LastUsedCM) || m->lastUsed() > d_ptr->m_LastUsedCM->lastUsed())
        d_ptr->slotLastContactMethod(m);
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
            if (auto r = cm->textRecording())
                d_ptr->slotChildrenTextRecordingAdded(r);

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
    Q_ASSERT(false);
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

    if (Q_UNLIKELY(d_ptr->m_Numbers.indexOf(cm) != -1)) {
        qWarning() << this << "already has the phone number" << cm;
        return cm;
    }

    if (cm->type() == ContactMethod::Type::TEMPORARY)
        cm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(cm)
        );

    if (Q_UNLIKELY(cm->contact() && !(*cm->contact() == *d_ptr->m_pPerson))) {
        qWarning() << "Adding a phone number to" << this << "already owned by" << cm->contact();
        Q_ASSERT(false);
    }

    d_ptr->connectContactMethod(cm);

    if ((!d_ptr->m_LastUsedCM) || cm->lastUsed() > d_ptr->m_LastUsedCM->lastUsed())
        d_ptr->slotLastContactMethod(cm);

    d_ptr->m_Numbers << cm;

    emit phoneNumbersChanged();

    if (d_ptr->m_HiddenContactMethods.indexOf(cm) != -1) {
        d_ptr->m_HiddenContactMethods.removeAll(cm);
        emit relatedContactMethodsRemoved(cm);
    }

    if (Q_UNLIKELY(cm->d_ptr->m_pIndividual && cm->d_ptr->m_pIndividual.data()->d_ptr != d_ptr)) {
        qWarning() << cm << "already has an individual attached" << cm->d_ptr->m_pIndividual <<
            "cannot set" << this;
    }
    else if (!cm->d_ptr->m_pIndividual)
        cm->d_ptr->m_pIndividual = d_ptr->m_pWeakRef;
    else if (cm->d_ptr->m_pIndividual != d_ptr->m_pWeakRef)
        Q_ASSERT(false);

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
    d_ptr->m_Numbers.remove(idx);
    phoneNumbersChanged();

    d_ptr->m_HiddenContactMethods << cm;

    emit relatedContactMethodsAdded(cm);

    return cm;
}

/// ContactMethods URI are immutable, they need to be replaced when edited
ContactMethod* Individual::replacePhoneNumber(ContactMethod* old, ContactMethod* newCm)
{
    if (Q_UNLIKELY(!newCm)) {
        qWarning() << this << "trying to replace a phone number with nothing";
        return nullptr;
    }

    const int idx = d_ptr->m_Numbers.indexOf(old);

    if (Q_UNLIKELY((!old) || idx == -1)) {
        qWarning() << this << "trying to replace a phone number that doesn't exist";
        return nullptr;
    }

    if (old == newCm) {
        qWarning() << "Trying to replace a phone number with itself";
        return old;
    }

    if (newCm->type() == ContactMethod::Type::TEMPORARY)
        newCm = PhoneDirectoryModel::instance().fromTemporary(
            static_cast<TemporaryContactMethod*>(newCm)
        );

    phoneNumbersAboutToChange();

    d_ptr->m_Numbers.replace(idx, newCm);

    d_ptr->m_HiddenContactMethods << newCm;

    emit relatedContactMethodsAdded(old);

    phoneNumbersChanged();

    return newCm;
}

/// Get the last ContactMethod used with that person.
ContactMethod* Individual::lastUsedContactMethod() const
{
    return d_ptr->m_LastUsedCM;
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
        auto p = QSharedPointer<IndividualTimelineModel>(new IndividualTimelineModel(d_ptr->m_pWeakRef));
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
    emit q_ptr->unreadCountChanged(0);
}


void IndividualPrivate::slotChildrenContactChanged(Person* newContact, Person* oldContact)
{
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
    emit q_ptr->childrenRebased(cm, other);
}

/// Like std::any_of
bool Individual::matchExpression(const std::function<bool(ContactMethod*)>& functor)
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

QSharedPointer<Individual> Individual::getIndividual(ContactMethod* cm)
{
    if (auto i = cm->d_ptr->m_pIndividual)
        return i;

    if (cm->contact())
        return getIndividual(cm->contact());

    auto i = QSharedPointer<Individual>(
        new Individual()
    );
    i->d_ptr->m_pWeakRef = i;

    // need to be done after WeakRef is set, do not move to constructor
    i->addPhoneNumber(cm);

    return i;
}

QSharedPointer<Individual> Individual::getIndividual(Person* p)
{
    if (auto i = p->d_ptr->m_pIndividual)
        return i;

    // strong ref
    p->d_ptr->m_pIndividual = QSharedPointer<Individual>(
        new Individual(p)
    );
    p->d_ptr->m_pIndividual->d_ptr->m_pWeakRef = p->d_ptr->m_pIndividual;

    return p->d_ptr->m_pIndividual;
}

QSharedPointer<Individual> Individual::getIndividual(const QList<ContactMethod*>& cms)
{
    if (cms.isEmpty())
        return {};

    QSharedPointer<Individual> ind;

    QSet<Account*> accs;
    QHash<QSharedPointer<Individual>, int> existing;
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

QSharedPointer<EventAggregate> Individual::events(FlagPack<Event::EventCategory> categories)
{
    return {};
}

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

#include <individual.moc>
