/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
 *   Author : Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>*
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
#pragma once

#include <QtGlobal>
#include <QObject> //BUG As of Qt 5.9.5, Q_NAMESPACE isn't in QtGlobal
#include <QtCore/QHash>


//HACK Q_NAMESPACE requies to be exported. Newer C++ variants can export
// namespaces by placing Q_DECL_EXPORT in front. Older cannot. This uses
// __attribute__ to prevent the moc from doing magic.
namespace Ring __attribute__ ((visibility ("default")))  {

// #pragma GCC visibility push(default)

Q_NAMESPACE

// #pragma GCC visibility pop

/**
 * The purpose of this enum class is to mimic/extend the Qt::ItemDataRole in LRC so that the same
 * value is used when using a common role in the ::data() method of any model in LRC,
 * eg: the value of the Object role should not be different for the PersonDirectory and the CallModel.
 *
 * This is so that clients of LRC do need additional logic when trying to extract the same type of
 * data from multiple types of LRC models.
 *
 * Thus any data role which is common to multiple models in LRC should be defined here. Data roles
 * which are specific to the model can be defined within that model only and their value should
 * start with UserRole + 1
 */

enum class Role
{
    DisplayRole        = Qt::DisplayRole ,
    Object             = Qt::UserRole + 1,
    ObjectType         ,
    Name               ,
    Number             ,
    LastUsed           ,
    FormattedLastUsed  ,
    IndexedLastUsed    ,
    State              ,
    FormattedState     ,
    Length             ,
    DropState          ,
    IsPresent          ,
    UnreadTextMessageCount,
    URI                ,
    IsBookmarked       ,
    IsRecording        ,
    HasActiveCall      ,
    HasActiveVideo     ,
    UserRole           = Qt::UserRole + 100  // this should always be the last role in the list
};

/// Role names associated with Ring::Role
static const QHash<int, QByteArray> roleNames = {
    { static_cast<int>(Role::DisplayRole            ) , "display"                },
    { static_cast<int>(Role::Object                 ) , "object"                 },
    { static_cast<int>(Role::ObjectType             ) , "objectType"             },
    { static_cast<int>(Role::Name                   ) , "name"                   },
    { static_cast<int>(Role::Number                 ) , "number"                 },
    { static_cast<int>(Role::LastUsed               ) , "lastUsed"               },
    { static_cast<int>(Role::FormattedLastUsed      ) , "formattedLastUsed"      },
    { static_cast<int>(Role::IndexedLastUsed        ) , "indexedLastUsed"        },
    { static_cast<int>(Role::State                  ) , "state"                  },
    { static_cast<int>(Role::FormattedState         ) , "formattedState"         },
    { static_cast<int>(Role::Length                 ) , "length"                 },
    { static_cast<int>(Role::DropState              ) , "dropState"              },
    { static_cast<int>(Role::IsPresent              ) , "isPresent"              },
    { static_cast<int>(Role::UnreadTextMessageCount ) , "unreadTextMessageCount" },
    { static_cast<int>(Role::IsBookmarked           ) , "isBookmarked"           },
    { static_cast<int>(Role::IsRecording            ) , "isRecording"            },
    { static_cast<int>(Role::HasActiveCall          ) , "hasActiveCall"          },
    { static_cast<int>(Role::HasActiveVideo         ) , "hasActiveVideo"         },
    { static_cast<int>(Role::UserRole               ) , "userRole"               },
};

/**
 * All LRC models that store more than one type of class (eg: RecentModel) should return a member of
 * this enum when ::data(Ring::Role::ObjectType) is called on one of their indeces. This is to
 * simplify LRC and client logic by not having to dynamic_cast the pointer stored in the index.
 */
enum class ObjectType
{
    Person,
    ContactMethod,
    Call,
    Media,
    Certificate,
    ContactRequest,
    Event,
    Individual,
    COUNT__
};
Q_ENUM_NS(ObjectType)

} // namespace Ring

Q_DECLARE_METATYPE(Ring::ObjectType)
