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
#include "globalinstances.h"

#include <memory>

#include "interfaces/accountlistcolorizeri.h"
#include "interfaces/contactmethodselectori.h"
#include "interfaces/dbuserrorhandleri.h"
#include "interfaces/itemmodelstateserializeri.h"
#include "interfaces/pixmapmanipulatori.h"
#include "interfaces/presenceserializeri.h"
#include "interfaces/shortcutcreatori.h"
#include "interfaces/actionextenderi.h"
#include "interfaces/fileprovideri.h"
#include "interfaces/audioformati.h"
#include "interfaces/videoformati.h"

#include "interfaces/default/accountlistcolorizerdefault.h"
#include "interfaces/default/dbuserrorhandlerdefault.h"
#include "interfaces/default/pixmapmanipulatordefault.h"
#include "interfaces/default/presenceserializerdefault.h"
#include "interfaces/default/shortcutcreatordefault.h"
#include "interfaces/default/actionextenderdefault.h"

namespace GlobalInstances {

struct InstanceManager
{
    std::unique_ptr<Interfaces::AccountListColorizerI>     m_accountListColorizer;
    std::unique_ptr<Interfaces::ContactMethodSelectorI>    m_contactMethodSelector;
    std::unique_ptr<Interfaces::DBusErrorHandlerI>         m_dBusErrorHandler;
    std::unique_ptr<Interfaces::ItemModelStateSerializerI> m_itemModelStateSerializer;
    std::unique_ptr<Interfaces::PixmapManipulatorI>        m_pixmapManipulator;
    std::unique_ptr<Interfaces::PresenceSerializerI>       m_presenceSerializer;
    std::unique_ptr<Interfaces::ShortcutCreatorI>          m_shortcutCreator;
    std::unique_ptr<Interfaces::ActionExtenderI>           m_actionExtender;
    std::unique_ptr<Interfaces::FileProviderI>             m_fileProvider;
    std::unique_ptr<Interfaces::AudioFormatI>              m_audioFormats;
    std::unique_ptr<Interfaces::VideoFormatI>              m_videoFormats;
};

static InstanceManager&
instanceManager()
{
    static std::unique_ptr<InstanceManager> manager{new InstanceManager};
    return *manager.get();
}

Interfaces::AccountListColorizerI&
accountListColorizer()
{
    if (!instanceManager().m_accountListColorizer)
        instanceManager().m_accountListColorizer.reset(new Interfaces::AccountListColorizerDefault);
    return *instanceManager().m_accountListColorizer.get();
}

void
setAccountListColorizer(std::unique_ptr<Interfaces::AccountListColorizerI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_accountListColorizer = std::move(instance);
}

Interfaces::ContactMethodSelectorI*
contactMethodSelector()
{
    if (!instanceManager().m_contactMethodSelector)
        return nullptr;
    return instanceManager().m_contactMethodSelector.get();
}

void
setContactMethodSelector(std::unique_ptr<Interfaces::ContactMethodSelectorI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_contactMethodSelector = std::move(instance);
}

Interfaces::DBusErrorHandlerI&
dBusErrorHandler()
{
    if (!instanceManager().m_dBusErrorHandler)
        instanceManager().m_dBusErrorHandler.reset(new Interfaces::DBusErrorHandlerDefault);
    return *instanceManager().m_dBusErrorHandler;
}

void
setDBusErrorHandler(std::unique_ptr<Interfaces::DBusErrorHandlerI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_dBusErrorHandler = std::move(instance);
}

Interfaces::ItemModelStateSerializerI*
itemModelStateSerializer()
{
    if (!instanceManager().m_itemModelStateSerializer) {
        Q_ASSERT(false);
        qWarning() << "The collection model serializer isn't set, expect a crash";
        return nullptr;
    }

    return instanceManager().m_itemModelStateSerializer.get();
}

void
setItemModelStateSerializer(std::unique_ptr<Interfaces::ItemModelStateSerializerI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_itemModelStateSerializer = std::move(instance);
}

Interfaces::PixmapManipulatorI&
pixmapManipulator()
{
    if (!instanceManager().m_pixmapManipulator)
        instanceManager().m_pixmapManipulator.reset(new Interfaces::PixmapManipulatorDefault);
    return *instanceManager().m_pixmapManipulator.get();
}

void
setPixmapManipulator(std::unique_ptr<Interfaces::PixmapManipulatorI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_pixmapManipulator = std::move(instance);
}

Interfaces::PresenceSerializerI&
presenceSerializer()
{
    if (!instanceManager().m_presenceSerializer)
        instanceManager().m_presenceSerializer.reset(new Interfaces::PresenceSerializerDefault);
    return *instanceManager().m_presenceSerializer.get();
}

void
setPresenceSerializer(std::unique_ptr<Interfaces::PresenceSerializerI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_presenceSerializer = std::move(instance);
}

Interfaces::ShortcutCreatorI&
shortcutCreator()
{
    if (!instanceManager().m_shortcutCreator)
        instanceManager().m_shortcutCreator.reset(new Interfaces::ShortcutCreatorDefault);
    return *instanceManager().m_shortcutCreator.get();
}

void
setShortcutCreator(std::unique_ptr<Interfaces::ShortcutCreatorI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_shortcutCreator = std::move(instance);
}

Interfaces::ActionExtenderI&
actionExtender()
{
    if (!instanceManager().m_actionExtender)
        instanceManager().m_actionExtender.reset(new Interfaces::ActionExtenderDefault);
    return *instanceManager().m_actionExtender.get();
}

void
setActionExtender(std::unique_ptr<Interfaces::ActionExtenderI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_actionExtender = std::move(instance);
}

Interfaces::FileProviderI&
fileProvider()
{
//     if (!instanceManager().m_fileProvider)
//         instanceManager().m_fileProvider.reset(new Interfaces::ActionExtenderDefault);
    return *instanceManager().m_fileProvider.get();
}

void setFileProvider(std::unique_ptr<Interfaces::FileProviderI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_fileProvider = std::move(instance);
}

Interfaces::AudioFormatI&
audioFormatHandler()
{
    // There is no coming back from this. The only way it can happen is if it
    // isn't initialized properly, in which case it will not work anyway.
    if (!instanceManager().m_audioFormats) {
        qWarning() << "LibrRingQt wasn't compiled properly, it will not work, bye";
        exit(1);
    }

    return *instanceManager().m_audioFormats.get();
}

void setAudioFormatHandler(std::unique_ptr<Interfaces::AudioFormatI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_audioFormats = std::move(instance);
}

Interfaces::VideoFormatI&
videoFormatHandler()
{
    // There is no coming back from this. The only way it can happen is if it
    // isn't initialized properly, in which case it will not work anyway.
    if (!instanceManager().m_videoFormats) {
        qWarning() << "LibrRingQt wasn't compiled properly, it will not work, bye";
        exit(1);
    }

    return *instanceManager().m_videoFormats.get();
}

void setVideoFormatHandler(std::unique_ptr<Interfaces::VideoFormatI> instance)
{
    // do not allow empty pointers
    if (!instance) {
        qWarning() << "ignoring empty unique_ptr";
        return;
    }
    instanceManager().m_videoFormats = std::move(instance);
}

/*
 * This API have some advantage over a more "explicit" one
 * 1) It treat interfaces as class instead of as objects, making conceptual sense
 * 2) It remove the boilerplate code related to creating the unique_ptr away from
 *    the client
 * 3) It offer a transparent entry point for interface without having to
 *    extend the API when adding new interfaces
 * 4) It mimic the addCollection interface, making the API more consistent. It
 *    also does so without the tick layer of black magic used in the Media and
 *    collection APIs.
 */
#define REGISTER_INTERFACE(I,m) \
void setInterfaceInternal(I* i)\
{\
   instanceManager().m = std::unique_ptr<I>(i);\
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-declarations"

REGISTER_INTERFACE(Interfaces::AccountListColorizerI    , m_accountListColorizer    )
REGISTER_INTERFACE(Interfaces::ContactMethodSelectorI   , m_contactMethodSelector   )
REGISTER_INTERFACE(Interfaces::DBusErrorHandlerI        , m_dBusErrorHandler        )
REGISTER_INTERFACE(Interfaces::ItemModelStateSerializerI, m_itemModelStateSerializer)
REGISTER_INTERFACE(Interfaces::PixmapManipulatorI       , m_pixmapManipulator       )
REGISTER_INTERFACE(Interfaces::PresenceSerializerI      , m_presenceSerializer      )
REGISTER_INTERFACE(Interfaces::ShortcutCreatorI         , m_shortcutCreator         )
REGISTER_INTERFACE(Interfaces::ActionExtenderI          , m_actionExtender          )
REGISTER_INTERFACE(Interfaces::FileProviderI            , m_fileProvider            )
REGISTER_INTERFACE(Interfaces::VideoFormatI             , m_videoFormats            )
REGISTER_INTERFACE(Interfaces::AudioFormatI             , m_audioFormats            )

#pragma GCC diagnostic pop

#undef REGISTER_INTERFACE

} // namespace GlobalInstances
