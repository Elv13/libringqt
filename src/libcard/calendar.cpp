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
#include "calendar.h"

#include "event.h"

class CalendarEditor final : public CollectionEditor<Event>
{
public:
   explicit CalendarEditor(CollectionMediator<Event>* m);
   virtual bool save       ( const Event* item ) override;
   virtual bool remove     ( const Event* item ) override;
   virtual bool edit       ( Event*       item ) override;
   virtual bool addNew     ( Event*       item ) override;
   virtual bool addExisting( const Event* item ) override;

   //Attributes
   QVector<Event*> m_lItems;

private:
   virtual QVector<Event*> items() const override;


};

class CalendarPrivate final
{
public:
    Account* m_pAccount {nullptr};
    CalendarEditor* m_pEditor {nullptr};
    QByteArray m_Path;
};

Calendar::Calendar(CollectionMediator<Event>* mediator, Account* a) : CollectionInterface(new CalendarEditor(mediator)),
d_ptr(new CalendarPrivate)
{
    d_ptr->m_pAccount = a;
    d_ptr->m_pEditor  = static_cast<CalendarEditor*>(editor<Event>());
}

Calendar::~Calendar()
{
    delete d_ptr;
}

bool Calendar::load()
{
    return true;
}

bool Calendar::reload()
{
    return true;
}

bool Calendar::clear()
{
    return false;
}

QString Calendar::name() const
{
    return QStringLiteral("iCalendar");
}

QString Calendar::category() const
{
    return QStringLiteral("History");
}

QByteArray Calendar::id() const
{
    return "ical"+d_ptr->m_Path;
}

FlagPack<CollectionInterface::SupportedFeatures> Calendar::supportedFeatures() const
{
    return
    CollectionInterface::SupportedFeatures::NONE       |
    CollectionInterface::SupportedFeatures::LOAD       |
    CollectionInterface::SupportedFeatures::CLEAR      |
    CollectionInterface::SupportedFeatures::MANAGEABLE |
    CollectionInterface::SupportedFeatures::REMOVE     |
    CollectionInterface::SupportedFeatures::EDIT       |
    CollectionInterface::SupportedFeatures::SAVE       |
    CollectionInterface::SupportedFeatures::ADD        ;
}

CalendarEditor::CalendarEditor(CollectionMediator<Event>* m) : CollectionEditor<Event>(m)
{

}

bool CalendarEditor::save( const Event* item )
{
    return true;
}

bool CalendarEditor::remove( const Event* item )
{
    return true;
}

bool CalendarEditor::edit( Event* item )
{
    return true;
}

bool CalendarEditor::addNew( Event* item )
{
    return true;
}

bool CalendarEditor::addExisting( const Event* item )
{
    return true;
}

QVector<Event*> CalendarEditor::items() const
{
    return m_lItems;
}

bool Calendar::isEnabled() const
{
    return true;
}

QList<QTimeZone*> Calendar::timezones() const
{
    return {};
}

Event* Calendar::eventAt(int position) const
{
    if (position < 0 || position >= d_ptr->m_pEditor->m_lItems.size())
        return nullptr;

    return d_ptr->m_pEditor->m_lItems[position];
}

Account* Calendar::account() const
{
    return d_ptr->m_pAccount;
}
