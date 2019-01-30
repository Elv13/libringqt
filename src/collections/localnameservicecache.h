/************************************************************************************
 *   Copyright (C) 2014-2016 by Savoir-faire Linux                                  *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>         *
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

class ContactMethod;

class LocalNameServiceCachePrivate;

/**
 * This collection manage a small CSV file mapping Ring registered names with
 * their RingId. Right now it has a single instance and is intended for internal
 * use, but eventually it could support multiple name service and external
 * instances.
 */
class LIB_EXPORT LocalNameServiceCache : public CollectionInterface
{
public:
   explicit LocalNameServiceCache(CollectionMediator<ContactMethod>* mediator);
   virtual ~LocalNameServiceCache();

   virtual bool load  () override;
   virtual bool reload() override;
   virtual bool clear () override;

   virtual QString    name     () const override;
   virtual QString    category () const override;
   virtual QVariant   icon     () const override;
   virtual bool       isEnabled() const override;
   virtual QByteArray id       () const override;

   virtual FlagPack<SupportedFeatures> supportedFeatures() const override;

private:
   LocalNameServiceCachePrivate* d_ptr;
   Q_DECLARE_PRIVATE(LocalNameServiceCache)
};

