/***************************************************************************
 *   Copyright (C) 2015 by Savoir-Faire Linux                              *
 *   Author : Emmanuel Lepage Vallee <elv1313@gmail.com>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "accountbuilder.h"

// Qt
#include <QQmlEngine>
#include <QQmlContext>

// LRC
#include <account.h>
#include <session.h>
#include <profilemodel.h>
#include <accountmodel.h>


AccountBuilder::AccountBuilder(QObject* parent) : QIdentityProxyModel(parent)
{
    setSourceModel(m_pSource);
}

AccountBuilder::~AccountBuilder()
{
    delete m_pSource;
}

QItemSelectionModel* AccountBuilder::selectionModel() const
{
    return m_pSource->selectionModel();
}

QVariant AccountBuilder::data( const QModelIndex& index, int role ) const
{
    const int offset = index.row() - static_cast<int>(Account::Protocol::COUNT__);
    if (index.isValid() && offset >= 0 && role == Qt::DisplayRole && offset < static_cast<int>(ExtendedRole::COUNT__)) {
        switch(static_cast<ExtendedRole>(offset)) {
            case ExtendedRole::PROFILE:
                return tr("Profile");
            case ExtendedRole::IMPORT:
                return tr("Import");
            case ExtendedRole::COUNT__:
                break;
        }
    }
    else
        return m_pSource->data(mapToSource(index), role);

    return QVariant();
}

int AccountBuilder::rowCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : m_pSource->rowCount()
        + static_cast<int>(AccountBuilder::ExtendedRole::COUNT__);
}

QModelIndex AccountBuilder::index( int row, int column, const QModelIndex& parent) const
{
    constexpr static const int total = static_cast<int>(Account::Protocol::COUNT__)
        + static_cast<int>(ExtendedRole::COUNT__);

    if ( row >= static_cast<int>(Account::Protocol::COUNT__) && row < total)
        return createIndex(row, column, row);
    else
        return mapFromSource(m_pSource->index(row, column, mapToSource(parent)));
}

Account* AccountBuilder::buildFor(int i)
{
    return buildFor(index(i, 0));
}

Account* AccountBuilder::buildFor(const QModelIndex& index)
{
    const Account::Protocol proto = qvariant_cast<Account::Protocol>(
        data(index,Qt::UserRole)
    );

   // Add profile or import accounts
    if ((int)proto == ((int)Account::Protocol::COUNT__) + (int)ExtendedRole::PROFILE) {
        const bool ret = Session::instance()->profileModel()->add();

        if (!ret)
            return nullptr;

        const QModelIndex idx = Session::instance()->profileModel()->index(Session::instance()->profileModel()->rowCount(),0);

//         if (idx.isValid()) {
//             m_pAccountList->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
//         }

        return nullptr;
    }
    else if ((int)proto == ((int)Account::Protocol::COUNT__) + (int)ExtendedRole::IMPORT) {
//         const QString path = QFileDialog::getOpenFileName(this, tr("Import accounts"),  QDir::currentPath());

//         if (!path.isEmpty()) {
//             QPointer<KPasswordDialog> dlg = new KPasswordDialog(this);
//             dlg->setPrompt(tr("Enter the password"));
//
//             if( !dlg->exec() )
//                 return;
//
//             Session::instance()->accountModel()->importAccounts(path, dlg->password());
//
//             delete dlg;
//         }

        return nullptr;
    }

    const QString newAlias = tr("New account")+
        AccountModel::getSimilarAliasIndex(tr("New account"));

    Account* a = Session::instance()->accountModel()->add(newAlias, proto);

    QModelIndex accIdx = Session::instance()->profileModel()->accountIndex(a);
    accIdx = static_cast<QAbstractProxyModel*>(
        Session::instance()->profileModel()->sortedProxyModel()
    )->mapFromSource(accIdx);

    Session::instance()->profileModel()->sortedProxySelectionModel()->setCurrentIndex(
        accIdx, QItemSelectionModel::ClearAndSelect
    );

    // Yes, QML somehow takes ownership by default, don't ask me why
    const auto ctx = QQmlEngine::contextForObject(this);
    ctx->engine()->setObjectOwnership(a, QQmlEngine::CppOwnership);

    return a;
}

// kate: space-indent on; indent-width 4; replace-tabs on;
