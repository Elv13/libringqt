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
#pragma once

// Qt
#include <QtCore/QObject>
#include <QtCore/QAbstractItemModel>

// LibRingQt
class ContactMethod;
class Person;
class Call;
class Individual;


class SharedModelLockerPrivate;

/**
 * This class implements a very common usage pattern where a single individual
 * is currently selected.
 *
 * Because LibRingQt uses shared pointers and QML support for them
 * "is unsuitable for production", it also make sure to keep the pointers
 * alive as long as some QML code may use them.
 *
 * By default, it will select the most recent individual.
 */
class SharedModelLocker : public QObject
{
    Q_OBJECT

public:
    Q_PROPERTY(QModelIndex suggestedTimelineIndex READ suggestedTimelineIndex)
    Q_PROPERTY(ContactMethod* currentContactMethod READ currentContactMethod WRITE setContactMethod NOTIFY changed)
    Q_PROPERTY(Individual* currentIndividual READ currentIndividual WRITE setIndividual NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* timelineModel READ timelineModel NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* unsortedListView READ unsortedListView NOTIFY changed)
    Q_PROPERTY(Person* currentPerson READ currentPerson WRITE setPerson NOTIFY changed)
    Q_PROPERTY(bool hasLinks    READ hasLinks    NOTIFY changed)
    Q_PROPERTY(bool hasBookmark READ hasBookmark NOTIFY changed)
    Q_PROPERTY(bool hasFiles    READ hasFiles    NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* linksLens READ linksLens NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* bookmarksLens READ bookmarksLens NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel* filesLens READ filesLens NOTIFY changed)

    Q_INVOKABLE explicit SharedModelLocker(QObject* parent = nullptr);
    virtual ~SharedModelLocker();

    QModelIndex suggestedTimelineIndex() const;
    ContactMethod* currentContactMethod() const;
    Individual* currentIndividual() const;
    QAbstractItemModel* timelineModel() const;
    QAbstractItemModel* unsortedListView() const;
    Person* currentPerson() const;

    bool hasLinks() const;
    bool hasBookmark() const;
    bool hasFiles() const;

    QAbstractItemModel* linksLens() const;
    QAbstractItemModel* bookmarksLens() const;
    QAbstractItemModel* filesLens() const;

public Q_SLOTS:
    void setContactMethod(ContactMethod* cm);
    void setIndividual(Individual* ind);
    void setPerson(Person* p);
    void showVideo(Call* c);

Q_SIGNALS:
    void suggestSelection(Individual* individual, const QModelIndex& modelIndex);
    void changed();

private:
    SharedModelLockerPrivate* d_ptr;
    Q_DECLARE_PRIVATE(SharedModelLocker)
};

Q_DECLARE_METATYPE(SharedModelLocker*)
