/****************************************************************************
 *   Copyright (C) 2009-2016 by Savoir-faire Linux                          *
 *   Author : Alexandre Lision <alexandre.lision@savoirfairelinux.com>      *
 *            Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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

#include <QtCore/QSharedPointer>
#include <QtCore/QString>

class AddressPrivate;

///Represent the physical address of a contact
class Address {
public:
    explicit Address();
    virtual ~Address();

    enum class Role {
        ADDRESSLINE = Qt::UserRole+1,
        CITY,
        ZIPCODE,
        STATE,
        COUNTRY,
        TYPE,
    };

    //Getters
    QString addressLine() const;
    QString city       () const;
    QString zipCode    () const;
    QString state      () const;
    QString country    () const;
    QString type       () const;

    //Setters
    void setAddressLine(const QString& value);
    void setCity       (const QString& value);
    void setZipCode    (const QString& value);
    void setState      (const QString& value);
    void setCountry    (const QString& value);
    void setType       (const QString& value);

private:
    QSharedPointer<AddressPrivate> d_ptr;
};
