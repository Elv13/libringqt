/************************************************************************************
 *   Copyright (C) 2018 by BlueSystems GmbH                                         *
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

#include <collectioninterface.h>
#include <typedefs.h>

class InfoTemplate;
class Individual;
class IndividualEditorPrivate;

/**
 * Create a table model of the GUI edition form for an ::Individual object.
 *
 * The layout is based on an ::InfoTemplate and can be modified.
 *
 * There is also a transactional subsystem to be able to edit the data without
 * creating ambiguity whether or not the original object can be restored
 * without saving.
 */
class LIB_EXPORT IndividualEditor : public QAbstractTableModel
{
    friend class Individual; // factory
public:
    virtual ~IndividualEditor();

    //Abstract model members
    virtual QVariant      data       (const QModelIndex& index, int role = Qt::DisplayRole                 ) const override;
    virtual int           rowCount   (const QModelIndex& parent = QModelIndex()                            ) const override;
    virtual int           columnCount(const QModelIndex& parent = QModelIndex()                            ) const override;
    virtual bool          setData    (const QModelIndex& index, const QVariant &value, int role            )       override;

    // Getter
    InfoTemplate* infoTemplate() const;

private:
    explicit IndividualEditor(Individual* parent, InfoTemplate* infoTemplate);

    IndividualEditorPrivate* d_ptr;
    Q_DECLARE_PRIVATE(IndividualEditor)
};
Q_DECLARE_METATYPE(IndividualEditor*)
