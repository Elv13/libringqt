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
#include "individualeditor.h"

#include <individual.h>
#include <infotemplate.h>

class IndividualEditorPrivate
{
public:
    Individual*   m_pParent;
    mutable InfoTemplate* m_pTemplate {nullptr};
};

IndividualEditor::IndividualEditor(Individual* parent, InfoTemplate* infoTemplate) : QAbstractTableModel(parent),
    d_ptr(new IndividualEditorPrivate())
{
    d_ptr->m_pParent   = parent;
    d_ptr->m_pTemplate = infoTemplate;
}

IndividualEditor::~IndividualEditor()
{
    delete d_ptr;
}

InfoTemplate* IndividualEditor::infoTemplate() const
{
    return d_ptr->m_pTemplate;
}

QVariant IndividualEditor::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    switch (index.column()) {
        case 0:
            if (role == Qt::DisplayRole)
                return infoTemplate()->formLabel(index.row());
            break;
    }

    return {};
}

int IndividualEditor::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : infoTemplate()->formRowCount();
}

int IndividualEditor::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 2;
}

bool IndividualEditor::setData(const QModelIndex& index, const QVariant &value, int role)
{
    return false;
}
