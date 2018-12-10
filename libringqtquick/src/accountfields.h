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

#include <QtCore/QObject>
#include <QQuickItem>

class AttachedAccountFieldStatusPrivate;
class AccountFieldsPrivate;
class QModelIndexBinder;
class Account;

/**
 * To be used as attached properties within an AccountFields container.
 */
class Q_DECL_EXPORT AttachedAccountFieldStatus : public QObject
{
    Q_OBJECT
public:

    /**
     * The name of the property to track.
     */
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)

    /**
     * If this property is available given the current account settings.
     */
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)

    /**
     * If this property is read-only given the current account settings.
     *
     * Some properties can only be set once.
     */
    Q_PROPERTY(bool readOnly READ isReadOnly NOTIFY availableChanged)

    /**
     * If the property require a non-empty value.
     */
    Q_PROPERTY(bool required READ isRequired NOTIFY availableChanged)

    /**
     * If the current value of the property is valid.
     */
    Q_PROPERTY(bool valid READ isValid NOTIFY availableChanged)

    virtual ~AttachedAccountFieldStatus();

    QString name() const;
    void setName(const QString& n);

    bool isAvailable() const;

    bool isReadOnly() const;

    bool isRequired() const;

    bool isValid() const;

    static AttachedAccountFieldStatus* qmlAttachedProperties(QObject *object);

Q_SIGNALS:
    void availableChanged();
    void changed();

private:
    explicit AttachedAccountFieldStatus(QObject* parent);

    AttachedAccountFieldStatusPrivate *d_ptr;
    Q_DECLARE_PRIVATE(AttachedAccountFieldStatus)
};

QML_DECLARE_TYPEINFO(AttachedAccountFieldStatus, QML_HAS_ATTACHED_PROPERTIES)

/**
 * Wrap a form and show/hide fields based on the current account settings.
 */
class Q_DECL_EXPORT AccountFields : public QQuickItem
{
    Q_OBJECT
public:
    explicit AccountFields(QQuickItem* parent = nullptr);
    virtual ~AccountFields();

    Q_PROPERTY(Account* account READ account WRITE setAccount NOTIFY accountChanged)

    Account* account() const;
    void setAccount(Account* a);

Q_SIGNALS:
    void accountChanged();
    void accountStatusChanged();

private:
    AccountFieldsPrivate* d_ptr;
};
