/************************************************************************************
 *   Copyright (C) 2014-2016 by Savoir-faire Linux                                  *
 *   Copyright (C) 2017 by Copyright (C) 2017 by Bluesystems                        *
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
#include <peerprofilecollection2.h>

#include <QtCore/QStandardPaths>

#include <person.h>
#include <vcardutils.h>
#include <contactmethod.h>
#include <individual.h>

#include "private/person_p.h"

class PeerProfileCollection2Private final
{
public:
    explicit PeerProfileCollection2Private(PeerProfileCollection2* parent);

    PeerProfileCollection2::DefaultMode m_Mode = PeerProfileCollection2::DefaultMode::QUICK_MERGE;

    PeerProfileCollection2* q_ptr;

    static QString path() {
        static QString p = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/peer_profiles/";
        return p;
    }

    void mergePersons(Person* source, ContactMethod* target) const;
    void quickMerge(Person* source, Person* target) const;
};

PeerProfileCollection2Private::PeerProfileCollection2Private(PeerProfileCollection2* parent) : q_ptr(parent)
{
}

PeerProfileCollection2::PeerProfileCollection2(CollectionMediator<Person>* mediator, PeerProfileCollection2* parent) :
FallbackPersonCollection(mediator, PeerProfileCollection2Private::path(), parent),d_ptr(new PeerProfileCollection2Private(this))
{
}

PeerProfileCollection2::~PeerProfileCollection2()
{
    delete d_ptr;
}

QString PeerProfileCollection2::name () const
{
    return QObject::tr("Peer profile collection v2");
}

QString PeerProfileCollection2::category () const
{
    return QObject::tr("Contact");
}

QByteArray PeerProfileCollection2::id() const
{
   return "ppc";
}

void PeerProfileCollection2::setDefaultMode(PeerProfileCollection2::DefaultMode m)
{
    d_ptr->m_Mode = m;
}

PeerProfileCollection2::DefaultMode PeerProfileCollection2::defaultMode() const
{
    return d_ptr->m_Mode;
}

void PeerProfileCollection2::setMergeOption(Person::Role role, PeerProfileCollection2::MergeOption option)
{
    Q_UNUSED(role)
    Q_UNUSED(option)
    //TODO
}

PeerProfileCollection2::MergeOption PeerProfileCollection2::mergeOption(Person::Role)
{
    //TODO
    return {};
}

void PeerProfileCollection2Private::quickMerge(Person* source, Person* target) const
{
    bool changed = false;

    // First, merge the strings
    for (QByteArray prop : {"nickName"    , "firstName", "secondName", "formattedName",
                            "organization", "preferredEmail", "group", "department"}) {
        if (target->property(prop).toString().isEmpty() && !source->property(prop).toString().isEmpty()) {
            changed = true;
            target->setProperty(prop, source->property(prop));
        }
    }

    if (target->photo().isNull() && !source->photo().isNull()) {
        changed = true;
        target->setPhoto(source->photo());
    }

    QSet<QString> dedup;

    const auto pn = target->individual()->phoneNumbers();

    for (auto cm : qAsConst(pn))
        dedup.insert(cm->uri());

    const auto pn2 = target->individual()->phoneNumbers();

    for (auto cm : qAsConst(pn2)) {
        if (!dedup.contains(cm->uri())) {
            changed = true;
            target->individual()->addPhoneNumber(cm);
        }
    }

    if (changed)
        target->save();
}

void PeerProfileCollection2Private::mergePersons(Person* source, ContactMethod* target) const
{
    switch (m_Mode) {
        case PeerProfileCollection2::DefaultMode::NEW_CONTACT:
            target->setPerson(source);
            q_ptr->add(source);
            source->save();
            return;
        case PeerProfileCollection2::DefaultMode::IGNORE_DUPLICATE:
            //p->merge(p->phoneNumbers().first()->contact());
            //TODO check if it's safe to delete it yet
            break;
        case PeerProfileCollection2::DefaultMode::QUICK_MERGE:
            quickMerge(source, target->contact());
            break;
        case PeerProfileCollection2::DefaultMode::ALWAYS_ASK:
            //TODO
            break;
        case PeerProfileCollection2::DefaultMode::CUSTOM:
            break;
    }

    //FIXME this is a memory leak
    // The problem is that some contact methods already have this person.
    // In theory slotContactRebased should help, but it's most probably already
    // too late. VCardUtils need to be fixed not to set the contact.
    if (target->contact() != source) {
        target->contact()->d_ptr->m_lParents << source;
        auto d = source->d_ptr;
        source->d_ptr = target->contact()->d_ptr;
        delete d;
    }


//     delete source;
}

///When we get a peer profile, its a vCard from a ContactRequest or a Call. We need to verify if
///this is Person which already exists, and so we simply need to update our existing vCard, or if
///this is a new Person, in which case we'll save a new vCard.
///We cannot trust the UID in the vCard for uniqueness. We can only rely on the RingID to be unique.
bool PeerProfileCollection2::importPayload(ContactMethod* cm, const QByteArray& payload)
{
    auto person = VCardUtils::mapToPerson(payload, true);

    if (Q_UNLIKELY(!person)) {
        qWarning() << "Expected a vCard, but got something else";
        return false;
    }

    const bool hasPerson = cm->contact();

    if (!hasPerson) {
        person->individual()->addPhoneNumber(cm);
        cm->setPerson(person);
        add(person);
        person->save();
        return true;
    }

    // If the other contact is also a peer profile, merging both should be
    // an higher priority
    //TODO const bool isPeer = cm->contact()->collection() == this;

    // Check if one of the phone number matches
    const auto iter = std::find_if(cm->contact()->individual()->phoneNumbers().constBegin(), cm->contact()->individual()->phoneNumbers().constEnd(), [cm](ContactMethod* cm2) {
        return (*cm2) == cm;
    });

    // Always assume the user wants Ring to do this for her/him
    if (iter == cm->contact()->individual()->phoneNumbers().constEnd())
        cm->contact()->individual()->addPhoneNumber(cm);

    d_ptr->mergePersons(person, cm);

    return true;
}
