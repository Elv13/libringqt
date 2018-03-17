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

template<typename T>
void VObjectAdapter<T>::setObjectFactory(std::function<T*(const std::basic_string<char>& object_type)> factory)
{
    setAbstractFactory([factory](const std::basic_string<char>& object_type) {
        return factory(object_type);
    });
}

template<typename T>
void VObjectAdapter<T>::addPropertyHandler(char* name, std::function<void(T* self, const std::basic_string<char>& value, const Parameters& params)> handler)
{
    setAbstractPropertyHandler(name, [handler, this](void* p, const std::basic_string<char>& value, const Parameters& params) {
        auto self = static_cast<T*>(p);
        handler(self, value, params);
    });
}

template<typename T>
void VObjectAdapter<T>::setFallbackPropertyHandler(std::function<void(T* self, const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params)> handler)
{
    setAbstractFallbackPropertyHandler([this, handler](void* p, const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params) {
        auto self = static_cast<T*>(p);
        handler(self, name, value, params);
    });
}

template<typename T>
template<typename T2>
void VObjectAdapter<T>::addObjectHandler(char* name, std::shared_ptr< VObjectAdapter<T2> > handler)
{
    //
}

template<typename T>
template<typename T2>
void VObjectAdapter<T>::setFallbackObjectHandler(std::function<void(T* self, T2* object, const std::basic_string<char>& name)> handler)
{
    setAbstractFallbackObjectHandler(VObjectAdapter<T2>::getTypeId(), [this, handler](void* p, void* p2, const std::basic_string<char>& name) {
        auto self = static_cast<T*>(p);
        auto obj  = static_cast<T2*>(p2);
        handler(self, obj, name);
    });
}

template<typename T>
int VObjectAdapter<T>::getTypeId()
{
    static int id = getNewTypeId();

    return id;
}
