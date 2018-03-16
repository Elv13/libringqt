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
void VObjectAdapter<T>::setObjectFactory(std::function<T*()> factory)
{
    setAbstractFactory([factory]() {
        return factory();
    });
}

template<typename T>
void VObjectAdapter<T>::addPropertyHandler(char* name, std::function<void(T* self, const std::basic_string<char>& value, const Parameters& params)> handler)
{
    setAbstractPropertyHandler(name, [handler, this](const std::basic_string<char>& value, const Parameters& params) {
        auto self = nullptr;
        handler(self, value, params);
    });
}

template<typename T>
void VObjectAdapter<T>::setFallbackPropertyHandler(std::function<void(T* self, const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params)> handler)
{
    setAbstractFallbackPropertyHandler([this, handler](const std::basic_string<char>& name, const std::basic_string<char>& value, const Parameters& params) {
        auto self = nullptr;
        handler(self, name, value, params);
    });
}

template<typename T>
template<typename T2>
void VObjectAdapter<T>::addObjectHandler(char* name, std::shared_ptr< VObjectAdapter<T2> > handler) const
{

}

template<typename T>
template<typename T2>
void VObjectAdapter<T>::setFallbackObjectHandler(char* name, std::shared_ptr< VObjectAdapter<T2> > handler) const
{

}
