/****************************************************************************
 *   Copyright (C) 2015-2016 by Savoir-faire Linux                               *
 *   Author : Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>*
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

#include <typedefs.h>

#include <memory>

namespace Interfaces {
class AccountListColorizerI;
class ContactMethodSelectorI;
class DBusErrorHandlerI;
class ItemModelStateSerializerI;
class PixmapManipulatorI;
class PresenceSerializerI;
class ShortcutCreatorI;
class ActionExtenderI;
class FileProviderI;
} // namespace Interfaces

/**
 * Use these functions to get and set the global instance of the implementation of each interface.
 *
 * The setter functions demand an std::unique_ptr because they become the object owners.
 *
 * Note that certain interfaces do not have a default implementation in LRC, in this case the getter
 * function will throw an exception if no instance has been set by the client.
 */
namespace GlobalInstances {

LIB_EXPORT Interfaces::AccountListColorizerI& accountListColorizer();
void LIB_EXPORT setAccountListColorizer(std::unique_ptr<Interfaces::AccountListColorizerI> instance);

/**
 * LRC does not provide a default implementation of this interface, thus an exception will be thrown
 * if this getter is called without an instance being set by the client
 */
LIB_EXPORT Interfaces::ContactMethodSelectorI& contactMethodSelector();
void LIB_EXPORT setContactMethodSelector(std::unique_ptr<Interfaces::ContactMethodSelectorI> instance);

LIB_EXPORT Interfaces::DBusErrorHandlerI& dBusErrorHandler();
void setDBusErrorHandler(std::unique_ptr<Interfaces::DBusErrorHandlerI> instance);

/**
 * LRC does not provide a default implementation of this interface, thus an exception will be thrown
 * if this getter is called without an instance being set by the client
 */
LIB_EXPORT Interfaces::ItemModelStateSerializerI& itemModelStateSerializer();
void LIB_EXPORT setItemModelStateSerializer(std::unique_ptr<Interfaces::ItemModelStateSerializerI> instance);

LIB_EXPORT Interfaces::PixmapManipulatorI& pixmapManipulator();
void LIB_EXPORT setPixmapManipulator(std::unique_ptr<Interfaces::PixmapManipulatorI> instance);

LIB_EXPORT Interfaces::PresenceSerializerI& presenceSerializer();
void LIB_EXPORT setPresenceSerializer(std::unique_ptr<Interfaces::PresenceSerializerI> instance);

LIB_EXPORT Interfaces::ShortcutCreatorI& shortcutCreator();
void LIB_EXPORT setShortcutCreator(std::unique_ptr<Interfaces::ShortcutCreatorI> instance);

LIB_EXPORT Interfaces::ActionExtenderI& actionExtender();
void LIB_EXPORT setActionExtender(std::unique_ptr<Interfaces::ActionExtenderI> instance);

LIB_EXPORT Interfaces::FileProviderI& fileProvider();
void LIB_EXPORT setFileProvider(std::unique_ptr<Interfaces::FileProviderI> instance);


//Private use only
void LIB_EXPORT setInterfaceInternal(Interfaces::AccountListColorizerI    *);
void LIB_EXPORT setInterfaceInternal(Interfaces::ContactMethodSelectorI   *);
void LIB_EXPORT setInterfaceInternal(Interfaces::DBusErrorHandlerI        *);
void LIB_EXPORT setInterfaceInternal(Interfaces::ItemModelStateSerializerI*);
void LIB_EXPORT setInterfaceInternal(Interfaces::PixmapManipulatorI       *);
void LIB_EXPORT setInterfaceInternal(Interfaces::PresenceSerializerI      *);
void LIB_EXPORT setInterfaceInternal(Interfaces::ShortcutCreatorI         *);
void LIB_EXPORT setInterfaceInternal(Interfaces::ActionExtenderI          *);
void LIB_EXPORT setInterfaceInternal(Interfaces::FileProviderI            *);

/**
 * Generic interface setter. This metamethod can set any type of interface
 * dynamically using variadic template, compile time type deduction and
 * the macro subsystem. Passing an invalid interface should trigger compile
 * time errors.
 *
 * The interface object is created internally and additional parameters
 * can be passed.
 */
template<class I, typename ...Ts>
void LIB_EXPORT setInterface(Ts... args)
{
    auto i = new I(args...);
    setInterfaceInternal(i);
}

} // namespace GlobalInstances
