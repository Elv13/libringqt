/************************************************************************************
 *   Copyright (C) 2019 by BlueSystems GmbH                                         *
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

#include <typedefs.h>

// LibRingQt
class CallModel;
class AccountModel;
class CategorizedHistoryModel;
class CategorizedContactModel;
class CategorizedBookmarkModel;
class AvailableAccountModel;
class NameDirectory;
class PeersTimelineModel;
class NumberCategoryModel;
class IndividualDirectory;
class ProfileModel;

/**
 * All uncreatable single instance objects of the base namespace.
 */
class LIB_EXPORT Session final : public QObject
{
    Q_OBJECT
public:
    Q_PROPERTY(CallModel* callModel READ callModel CONSTANT)
    Q_PROPERTY(AccountModel* accountModel READ accountModel CONSTANT)
    Q_PROPERTY(CategorizedHistoryModel* historyModel READ historyModel CONSTANT)
    Q_PROPERTY(CategorizedContactModel* contactModel READ contactModel CONSTANT)
    Q_PROPERTY(CategorizedBookmarkModel* bookmarkModel READ bookmarkModel CONSTANT)
    Q_PROPERTY(AvailableAccountModel* availableAccountModel READ availableAccountModel CONSTANT)
    Q_PROPERTY(NameDirectory* nameDirectory READ nameDirectory CONSTANT)
    Q_PROPERTY(PeersTimelineModel* peersTimelineModel READ peersTimelineModel CONSTANT)
    Q_PROPERTY(NumberCategoryModel* numberCategoryModel READ numberCategoryModel CONSTANT)
    Q_PROPERTY(ProfileModel* profileModel READ profileModel CONSTANT)

    CallModel*    callModel   () const;
    AccountModel* accountModel() const;
    CategorizedHistoryModel* historyModel() const;
    CategorizedContactModel* contactModel() const;
    CategorizedBookmarkModel* bookmarkModel() const;
    AvailableAccountModel* availableAccountModel() const;
    NameDirectory* nameDirectory() const;
    PeersTimelineModel* peersTimelineModel() const;
    NumberCategoryModel* numberCategoryModel() const;
    IndividualDirectory* individualDirectory() const;
    ProfileModel* profileModel() const;

    static Session* instance();

private:
    explicit Session(QObject* parent = nullptr);
    virtual ~Session();

};
