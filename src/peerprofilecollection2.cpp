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
