/****************************************************************************
 *   Copyright (C) 2013-2016 by Savoir-faire Linux                          *
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
#include "numbercategorymodel.h"
#include "private/numbercategorymodel_p.h"
#include "contactmethod.h"
#include "numbercategory.h"
#include "session.h"

NumberCategoryModel::NumberCategoryModel(QObject* parent) : QAbstractListModel(parent),CollectionManagerInterface(this),d_ptr(new NumberCategoryModelPrivate())
{}

NumberCategoryModel::~NumberCategoryModel()
{
    qDeleteAll(d_ptr->m_lCategories);
    delete d_ptr;
}

QHash<int,QByteArray> NumberCategoryModel::roleNames() const
{
    static QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();

    static std::atomic_flag init_flag = ATOMIC_FLAG_INIT;
    if (!init_flag.test_and_set())
        roles[Role::KEY] = "key";

    return roles;
}

QVariant NumberCategoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    switch (role) {
        case Qt::DisplayRole: {
            const QString name = d_ptr->m_lCategories[index.row()]->category->name();
            return name.isEmpty()?tr("Uncategorized"):name;
        }
        case Qt::DecorationRole:
            return d_ptr->m_lCategories[index.row()]->category->icon();//m_pDelegate->icon(m_lCategories[index.row()]->icon);
        case Qt::CheckStateRole:
            return d_ptr->m_lCategories[index.row()]->enabled?Qt::Checked:Qt::Unchecked;
        case Qt::UserRole:
            return 'x'+QString::number(d_ptr->m_lCategories[index.row()]->counter);
        case Role::KEY:
            return d_ptr->m_lCategories[index.row()]->category->key();
    }

    return {};
}

int NumberCategoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : d_ptr->m_lCategories.size();
}

Qt::ItemFlags NumberCategoryModel::flags(const QModelIndex& index) const
{
    return (d_ptr->m_lCategories[index.row()]->category->name().isEmpty() ? Qt::NoItemFlags : Qt::ItemIsEnabled)
        | Qt::ItemIsSelectable
        | Qt::ItemIsUserCheckable;
}

bool NumberCategoryModel::setData(const QModelIndex& idx, const QVariant &value, int role)
{
    if (idx.isValid() && role == Qt::CheckStateRole) {
        d_ptr->m_lCategories[idx.row()]->enabled = value.toBool();
        emit dataChanged(idx,idx);
        return true;
    }

    return false;
}

/**
 * Manually add a new category
 */
NumberCategory* NumberCategoryModel::addCategory(const QString& name, const QVariant& icon, int key)
{
    if (name.isEmpty())
        return this->other();

    const auto lower = name.toLower();

    if (auto rep = d_ptr->m_hByName.value(lower))
        return rep->category;

    auto rep = new NumberCategoryModelPrivate::InternalTypeRepresentation();

    NumberCategory* cat = addCollection<NumberCategory,QString>(name, LoadOptions::NONE);
    cat->setKey ( key  );
    cat->setIcon( icon );

    rep->category = cat                        ;
    rep->index    = d_ptr->m_lCategories.size();
    rep->enabled  = false                      ;

    this->beginInsertRows(this->nameToIndex(name),d_ptr->m_lCategories.size(), d_ptr->m_lCategories.size());
    d_ptr->m_hToInternal[ cat   ] = rep;
    d_ptr->m_hByIdx     [ key   ] = rep;
    d_ptr->m_hByName    [ lower ] = rep;
    d_ptr->m_lCategories << rep;
    this->endInsertRows();

    return cat;
}

QModelIndex NumberCategoryModel::nameToIndex(const QString& name) const
{
    if (const auto rep = d_ptr->m_hByName.value(name.toLower()))
        return index(rep->index, 0);

    return {};
}

///Be sure the category exist, increment the counter
void NumberCategoryModelPrivate::registerNumber(ContactMethod* number)
{
    const QString lower = number->category()->name().toLower();

    auto rep = m_hByName.value(lower);

    if (!rep) {
        Session::instance()->numberCategoryModel()->addCategory(number->category()->name(), {});
        rep = m_hByName[lower];
    }

    rep->counter++;
}

void NumberCategoryModelPrivate::unregisterNumber(ContactMethod* number)
{
    const QString lower = number->category()->name().toLower();

    if (auto rep = m_hByName[lower])
        rep->counter--;
}

///Get the category (case insensitive)
NumberCategory* NumberCategoryModel::getCategory(const QString& type) const
{
    const QString lower = type.toLower();

    if (lower.isEmpty())
        return this->other();
    else if(auto internal = d_ptr->m_hByName.value(lower))
        return internal->category;

    return Session::instance()->numberCategoryModel()->addCategory(lower, {});
}

NumberCategory* NumberCategoryModel::getCategory(const QModelIndex& index) const
{
    return (index.row() < 0 || index.row() >= rowCount()) ?
        nullptr : d_ptr->m_lCategories[index.row()]->category;
}

NumberCategory* NumberCategoryModel::forKey(int key) const
{
    if (const auto ret = d_ptr->m_hByIdx.value(key))
        return ret->category;

    return nullptr;
}

NumberCategory* NumberCategoryModel::other()
{
    static QString translated = QObject::tr("Other");
    static auto ret = Session::instance()->numberCategoryModel()->getCategory(translated);
    return ret;
}

int NumberCategoryModelPrivate::getSize(const NumberCategory* cat) const
{
    const auto i = m_hToInternal.value(cat);
    return i ? i->counter : 0;
}

void NumberCategoryModel::collectionAddedCallback(CollectionInterface* collection)
{
    Q_UNUSED(collection)
}

bool NumberCategoryModel::addItemCallback(const ContactMethod* item)
{
    Q_UNUSED(item)
    return false;
}

bool NumberCategoryModel::removeItemCallback(const ContactMethod* item)
{
    Q_UNUSED(item)
    return false;
}
