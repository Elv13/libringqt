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
#include "textrecordingmodel.h"

// Ring
#include "contactmethod.h"
#include "media/textrecording.h"
#include "media/mimemessage.h"

///Constructor
TextRecordingModel::TextRecordingModel(Media::TextRecording* recording) : QAbstractListModel(recording),m_pRecording(recording)
{
    connect(recording, &Media::TextRecording::aboutToInsertMessage, this, [this]() {
        const int rc = rowCount();
        beginInsertRows({}, rc, rc);
    });

    connect(recording, &Media::TextRecording::messageInserted, this, [this]() {
        endInsertRows();
    });

    connect(recording, &Media::TextRecording::aboutToClear, this, [this]() {
        beginResetModel();
    });

    connect(recording, &Media::TextRecording::cleared, this, [this]() {
        endResetModel();
    });
}

TextRecordingModel::~TextRecordingModel()
{
//    delete d_ptr;
}

QHash<int,QByteArray> TextRecordingModel::roleNames() const
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
        roles.insert((int)Media::TextRecording::Role::ContactMethod       , "contactMethod"       );
        roles.insert((int)Media::TextRecording::Role::HTML                , "HTML"                );
        roles.insert((int)Media::TextRecording::Role::HasText             , "hasText"             );
    }
    return roles;
}

///Get data from the model
QVariant TextRecordingModel::data( const QModelIndex& idx, int role) const
{
    if ((!idx.isValid()) || idx.column())
        return {};

    return m_pRecording->roleData(idx.row(), role);
}

///Number of row
int TextRecordingModel::rowCount(const QModelIndex& parentIdx) const
{
    if (!parentIdx.isValid())
        return m_pRecording->count();

    return 0;
}

///Model flags
Qt::ItemFlags TextRecordingModel::flags(const QModelIndex& idx) const
{
    Q_UNUSED(idx)
    return Qt::ItemIsEnabled;
}

///Set model data
bool TextRecordingModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
    if (idx.column() || !idx.isValid())
        return false;

    auto m = m_pRecording->messageAt(idx.row());

    if (!m)
        return false;

    switch (role) {
        case (int)Media::TextRecording::Role::IsRead               :
            //TODO delegate that logic to MimeMessage::
            if ((value.toBool() && m->status() != Media::MimeMessage::State::READ)
              || ((!value.toBool()) && m->status() != Media::MimeMessage::State::UNREAD)
            ) {

                if (value.toBool())
                    m_pRecording->setAsRead(m);
                else
                    m_pRecording->setAsUnread(m);

                emit dataChanged(idx,idx);
            }
            break;
        default:
            return false;
    }

    return true;
}
