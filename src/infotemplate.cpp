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
#include "infotemplate.h"

// Ring
#include <infotemplatemanager.h>
#include <vcardutils.h>

// Std
#include <random>

struct TemplateField
{
    enum class Type {
        STRING,
        NUMBER,
        LIST,
    } m_Type {Type::STRING};

    QByteArray m_Property;
    QByteArray m_DisplayName;
};

class InfoTemplatePrivate
{
public:

    static const QByteArray X_PHONENUMBERS;
    static const QByteArray X_EMAILS      ;
    static const QByteArray X_ADDRESSES   ;

    QVector<TemplateField*> m_lFields;

    QByteArray m_Uid;

};

const QByteArray InfoTemplatePrivate::X_PHONENUMBERS { "X-Ring-PHONENUMBERS" };
const QByteArray InfoTemplatePrivate::X_EMAILS       { "X-Ring-EMAILS"       };
const QByteArray InfoTemplatePrivate::X_ADDRESSES    { "X-Ring-ADDRESSES"    };


InfoTemplate::InfoTemplate(const QByteArray& data, QObject* parent) : ItemBase(parent), d_ptr(new InfoTemplatePrivate)
{
    const auto fields = VCardUtils::parseFields(data);

    for (const auto& f : qAsConst(fields)) {
        Q_UNUSED(f);
        //TODO
    }
}

InfoTemplate::~InfoTemplate()
{
    delete d_ptr;
}

QByteArray InfoTemplate::uid() const
{
    if (d_ptr->m_Uid.isEmpty()) {
        static std::random_device rdev;
        static std::seed_seq seq {rdev(), rdev()};
        static std::mt19937_64 rand {seq};
        static std::uniform_int_distribution<uint64_t> id_generator;

        while (d_ptr->m_Uid.isEmpty()
            || (InfoTemplateManager::instance().getByUid(d_ptr->m_Uid)
                && InfoTemplateManager::instance().getByUid(d_ptr->m_Uid) != this)) {
            d_ptr->m_Uid = QString::number(id_generator(rand)).toLatin1();
        }
    }

    return d_ptr->m_Uid;
}

QByteArray InfoTemplate::toVCard() const
{
    VCardUtils maker;

    maker.startVCard(QStringLiteral("2.1"));

    for (const auto& prop : d_ptr->m_lFields) {
        maker.addProperty(prop->m_Property, prop->m_DisplayName);
    }

    return maker.endVCard();
}

int InfoTemplate::formRowCount() const
{
    return d_ptr->m_lFields.size();
}

QString InfoTemplate::formLabel(int row) const
{
    if (row < 0 || row >= d_ptr->m_lFields.size())
        return {};

    return d_ptr->m_lFields[row]->m_DisplayName;
}
