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
#include <QtCore/QAbstractItemModel>

// Ring
class ContactMethod;
class Person;

class PeerTimelineModelPrivate;

/**
 * Merge everything related to a person in a single tree model. This is the
 * central model of the "timeline" user experience. This entity track just
 * about everything happening in LibRingClient.
 *
 * The layout has the "time categories" such as today or yesterday as top level
 * elements. The second tree layer is occupied by groups of calls, texts or
 * other media. Groups are usually delimited by time periods. If there is no
 * activity, then a group is "archived" a new one created when the activity
 * restart. The lenght of a group depends on the media type. The third and final
 * layer is occupied by the recordings (history call, audio file, texts, mails)
 * themselves.
 *
 */
class PeerTimelineModel final : public QAbstractItemModel
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    Q_OBJECT
    #pragma GCC diagnostic pop

public:

    /// The different types of element included in the timeline.
    enum class NodeType {
        SECTION_DELIMITER , /*!< Each "block" of text messages related to each other*/
        TEXT_MESSAGE      , /*!< A text message */
        TIME_CATEGORY     , /*!< Today, Yesterday and so on...    */
        CALL_GROUP        , /*!< Like: "2 incoming, 2 outgoind, 2 missed" one after the other*/
        CALL              , /*!< An history or active Call        */
        AUDIO_RECORDING   , /*!< An history call with autio media */
        SCREENSHOT, //TODO
        EMAIL, //TODO
        COUNT__
    };
    Q_ENUMS(NodeType);

    enum class Role {
        NodeType = 2 << 16,
        EndAt,
    };

    //Constructor
    explicit PeerTimelineModel(Person* cm);
    PeerTimelineModel(ContactMethod* cm);
    virtual ~PeerTimelineModel();

    //Abstract model function
    virtual QVariant      data     ( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    virtual int           rowCount ( const QModelIndex& parent = {}                       ) const override;
    virtual int        columnCount ( const QModelIndex& parent = {}                       ) const override;
    virtual Qt::ItemFlags flags    ( const QModelIndex& index                             ) const override;
    virtual QModelIndex   parent   ( const QModelIndex& index                             ) const override;
    virtual QModelIndex   index    ( int row, int column, const QModelIndex& parent = {}  ) const override;
    virtual bool  setData  ( const QModelIndex& index, const QVariant &value, int role)       override;

    virtual QHash<int,QByteArray> roleNames() const override;

    // Mutator
    void addContactMethod(ContactMethod* cm);

private:
    PeerTimelineModelPrivate* d_ptr;
    Q_DECLARE_PRIVATE(PeerTimelineModel)
};

Q_DECLARE_METATYPE(PeerTimelineModel::NodeType)
