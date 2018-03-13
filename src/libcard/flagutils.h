/****************************************************************************
 *   Copyright (C) 2009-2017 Savoir-faire Linux                             *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
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

/**
 * This function add a safe way to get an enum class size.
 * @note it cannot be "const" due to some compiler issues
 * @note it cannot be unsigned to avoid some compiler warnings
 * @note The 2 optional parameters should not be used directly. They are used
 *  internally for the enum_iterator
 */
template<typename A, A from = static_cast<A>(0), A to = A::COUNT__ >
constexpr int enum_class_size() {
   return static_cast<int>(to) - static_cast<int>(from);
}

/**
 * Create a safe pack of flags from an enum class.
 *
 * This class exist to ensure all sources come from the same enum and that it is
 * never accidentally accidentally into an integer.
 *
 * This assume that the enum has been setup as flags.
 */
template<class T>
class __attribute__ ((visibility ("default"))) FlagPack
{
public:
   FlagPack() : m_Flags(0) {}
   FlagPack(const T& base) : m_Flags(static_cast<unsigned>(base)) {}
   FlagPack(const FlagPack<T>& other) : m_Flags(other.m_Flags) {}

   //Operator
   FlagPack<T>& operator|(const T& other) {
      m_Flags |= static_cast<unsigned>(other);
      return *this;
   }

   FlagPack<T>& operator|(const FlagPack<T>& other) {
      m_Flags |= other.m_Flags;
      return *this;
   }

   FlagPack<T>& operator|=(const T& other) {
      m_Flags |= static_cast<unsigned>(other);
      return *this;
   }

   FlagPack<T>& operator|=(const FlagPack<T>& other) {
      m_Flags |= other.m_Flags;
      return *this;
   }

   FlagPack<T>& operator^=(const T& other) {
      m_Flags ^= static_cast<unsigned>(other);
      return *this;
   }

   FlagPack<T>& operator^=(const FlagPack<T>& other) {
      m_Flags ^= other.m_Flags;
      return *this;
   }

   FlagPack<T> operator&(const T& other) const {
      return FlagPack<T>(m_Flags & static_cast<unsigned>(other));
   }

   FlagPack<T> operator&(const FlagPack<T>& other) const {
      return FlagPack<T>(m_Flags & other.m_Flags);
   }

   FlagPack<T>&operator=(const FlagPack<T>& other) {
      m_Flags = other.m_Flags;
      return *this;
   }

   bool operator!=(const T& other) const {
      return  m_Flags != static_cast<unsigned>(other);
   }

   bool operator==(const T& other) const {
      return  m_Flags == static_cast<unsigned>(other);
   }

   bool operator==(const FlagPack<T>& other) const {
      return  m_Flags == other.m_Flags;
   }

   bool operator!() const {
      return !m_Flags;
   }

   operator bool() const {
      return m_Flags != 0;
   }

   unsigned value() const {
      return m_Flags;
   }

private:
   FlagPack(unsigned base) : m_Flags(base) {}
   unsigned m_Flags;
};

#define DO_PRAGMA(x) _Pragma (#x)

//Globally disable the "-Wunused-function" warning for GCC
//refs: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55578
#if ((__GNUC_MINOR__ > 8) || (__GNUC_MINOR__ == 8))
#pragma GCC diagnostic ignored "-Wunused-function"
#endif


#define DECLARE_ENUM_FLAGS(T)\
DO_PRAGMA(GCC diagnostic push)\
DO_PRAGMA(GCC diagnostic ignored "-Wunused-function")\
__attribute__ ((unused)) static FlagPack<T> operator|(const T& first, const T& second) { \
   FlagPack<T> p (first); \
   return p | second; \
} \
DO_PRAGMA(GCC diagnostic pop)
