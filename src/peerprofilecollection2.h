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
#pragma once

#include <fallbackpersoncollection.h>
#include <person.h>
#include <typedefs.h>

class Person;
class PeerProfileCollection2Private;

template<typename T> class CollectionMediator;

/**
 * This call replaces the older PeerProfileCollection due to a copyright
 * header issue and extensive code duplication with the FallbackPersonCollection
 * class. The extra code duplication caused extra maintenance and isn't in line
 * with the KDE need to have strong built-it contact management.
 */
class LIB_EXPORT PeerProfileCollection2 : public FallbackPersonCollection
{
public:
    /**
     * When a contact is already set for a contact method and a remote vCard
     * arrives, full overwrite isn't going to work. This enum offers more
     * options.
     */
    enum class DefaultMode {
        NEW_CONTACT,
        IGNORE_DUPLICATE,
        QUICK_MERGE,
        ALWAYS_ASK,
        CUSTOM,
    };

    /**
     * If DefaultMode is set to `CUSTOM`, this enum offer options for each fields.
     * The fields are the Person::Role.
     */
    enum class MergeOption {
        IGNORE,
        MERGE,
        ASK,
    };

    explicit PeerProfileCollection2(CollectionMediator<Person>* mediator, PeerProfileCollection2* parent = nullptr);
    virtual ~PeerProfileCollection2();

    virtual QString    name     () const override;
    virtual QString    category () const override;
    virtual QByteArray id       () const override;

    void setDefaultMode(DefaultMode m);
    DefaultMode defaultMode() const;

    void setMergeOption(Person::Role role, MergeOption option);
    MergeOption mergeOption(Person::Role);

    void mergePersons(Person* p);

private:
    PeerProfileCollection2Private* d_ptr;
    Q_DECLARE_PRIVATE(PeerProfileCollection2)
};

Q_DECLARE_METATYPE(PeerProfileCollection2*)
