/************************************************************************************
 *   Copyright (C) 2017 by BlueSystems GmbH                                         *
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

#include <usagestatistics.h>
#include <person.h>
#include <contactmethod.h>

class PersonStatistics : public UsageStatistics
{
    Q_OBJECT
public:
    explicit PersonStatistics(const Person* p) :
        UsageStatistics(const_cast<Person*>(p)), m_pPerson(p) {}

    virtual int totalSeconds() const override {
        return sum<int>(&UsageStatistics::totalSeconds);
    }

    virtual int lastWeekCount() const override {
        return sum<int>(&UsageStatistics::lastWeekCount);
    }

    virtual int lastTrimCount() const override {
        return sum<int>(&UsageStatistics::lastTrimCount);
    }

    virtual time_t lastUsed() const override {
        return sum<time_t>(&UsageStatistics::lastUsed);
    }

    virtual bool hasBeenCalled() const override {
        return sum<bool>(&UsageStatistics::hasBeenCalled);
    }

private:

    template<typename T> T sum(T(UsageStatistics::*M)(void)const) const {
        T ret {};

        const auto cms = m_pPerson->phoneNumbers();
        for (auto cm : qAsConst(cms)) {
            ret += (cm->usageStatistics()->*M)();
        }

        const auto cms2 = m_pPerson->relatedContactMethods();

        for (auto cm : qAsConst(cms2)) {
            ret += (cm->usageStatistics()->*M)();
        }

        return ret;
    }

    const Person* m_pPerson;
};
