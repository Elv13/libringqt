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

#include <picocms/collectioninterface.h>
#include <typedefs.h>

class InfoTemplate;
class LocalInfoTemplateCollectionPrivate;

template<typename T> class CollectionMediator;

/**
 * Store vCard templates to be used to manage the vidual fields in the user
 * interface.
 */
class LIB_EXPORT LocalInfoTemplateCollection : public CollectionInterface
{
public:
   explicit LocalInfoTemplateCollection(CollectionMediator<InfoTemplate>* mediator);
   virtual ~LocalInfoTemplateCollection();

   virtual bool load  () override;
   virtual bool reload() override;
   virtual bool clear () override;

   virtual QString    name     () const override;
   virtual QString    category () const override;
   virtual bool       isEnabled() const override;
   virtual QByteArray id       () const override;

   virtual FlagPack<SupportedFeatures> supportedFeatures() const override;

private:
   LocalInfoTemplateCollectionPrivate* d_ptr;
   Q_DECLARE_PRIVATE(LocalInfoTemplateCollection)
};
Q_DECLARE_METATYPE(LocalInfoTemplateCollection*)
