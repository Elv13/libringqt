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

#include <icsloader.h>

#include <cassert>
#include <iostream>

/**
 * This test the windows and macOS9 line ending
 */
static constexpr const char fromRfc3[] = \
"BEGIN:VCALENDAR/r"
"PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN/r"
"VERSION:2.0/r"
"BEGIN:VEVENT/r"
"DESCRIPTION;foo=bar;foo2=\"bar\";foo3=b\\;\\:ar:Networld+Interop /r"
"  and Exhibit\\nAtlanta World Congress Center\\n/r"
" Atlanta\\, Georgia/r"
"END:VEVENT/r"
"END:VCALENDAR/r";

/**
 * A simple string collection that maps well into the VObject representation.
 *
 * This is intended to perform 1:1 comparison between the expected tree and
 * the parsed one.
 */
struct TestObj
{
    struct Property {
        struct Parameter {
            std::string name;
            std::string value;
        };
        std::string name;
        std::string value;
        std::vector<Parameter> parameters;
    };
    std::string name;
    std::vector<TestObj> children;
    std::vector<Property> properties;

    static ICSLoader* isEqual(const char* content, const TestObj& other);
};

ICSLoader* TestObj::isEqual(const char* content, const TestObj& other)
{
    auto genericAdapter = std::shared_ptr<VObjectAdapter<TestObj>>(new  VObjectAdapter<TestObj>);

    TestObj* root = nullptr;

    genericAdapter->setObjectFactory([&root](const std::basic_string<char>& object_type) -> TestObj* {
        auto ret = new TestObj;

        ret->name = object_type;

        if (!root)
            root = ret;

        return ret;
    });

    genericAdapter->setFallbackPropertyHandler(
        [](
           TestObj* self,
           const std::basic_string<char>& name,
           const std::basic_string<char>& value,
           const AbstractVObjectAdaptor::Parameters& params
        ) {
            std::vector<TestObj::Property::Parameter> parameters;

            for (const auto& p : params)
                parameters.push_back({p.first, p.second});

            self->properties.push_back({
                name, value, parameters
            });
    });

    genericAdapter->setFallbackObjectHandler<TestObj>(
        [](
           TestObj* self,
           TestObj* child,
           const std::basic_string<char>& name
        ) {

            self->children.push_back(*child);
    });

    // Add for all types used by the test
    ICSLoader loader;
    loader.registerVObjectAdaptor("VCALENDAR", genericAdapter );
    loader.registerVObjectAdaptor("VEVENT"   , genericAdapter );
    loader.registerVObjectAdaptor("VTODO"    , genericAdapter );
    loader.registerVObjectAdaptor("VJOURNAL" , genericAdapter );
    loader.registerVObjectAdaptor("VCARD"    , genericAdapter );

    loader.registerFallbackVObjectAdaptor(genericAdapter);

    loader._test_CharToObj(content);

    assert(root);

    std::function<void(const TestObj* parsed, const TestObj* expected)> digger;

    digger = [&digger](const TestObj* parsed, const TestObj* expected) {
        // Check the properties
        assert(parsed->properties.size() == expected->properties.size());
        for (int i = 0; i < parsed->properties.size(); i++) {
            auto pp(parsed->properties[i]), pe(expected->properties[i]);

            assert(pp.name  == pe.name );
            assert(pp.value == pe.value);

            // Check the properties
            assert(pp.parameters.size() == pe.parameters.size());
            for (int j = 0; j < pp.parameters.size(); j++) {
                assert(pp.parameters[j].name == pe.parameters[j].name);
                assert(pp.parameters[j].value == pe.parameters[j].value);
            }
        }

        // Check the children object
        assert(parsed->children.size() == expected->children.size());
        for (int i = 0; i < parsed->children.size(); i++)
            digger(&parsed->children[i], &expected->children[i]);
    };

    digger(root, &other);
}

/**
 * This test parameter quoting and single property handlers.
 */
void testQuoting()
{
    static constexpr const char fromRfc4[] = \
        "BEGIN:VCALENDAR\r\n"
        "PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
        "VERSION:2.0\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION;foo=bar;foo2=\"b:ar\";foo3=b\\;\\:ar:Networld+Interop \r\n"
        "  and Exhibit\\nAtlanta World Congress Center\\n\r\n"
        " Atlanta\\, Georgia\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n";

    int objectCount = 0;
    bool hasVersion = false;
    auto calAdapter = std::shared_ptr<VObjectAdapter<TestObj>>(new  VObjectAdapter<TestObj>);
    calAdapter->setObjectFactory([&objectCount](const std::basic_string<char>& object_type) -> TestObj* {
        objectCount++;
        return new TestObj;
    });

    calAdapter->addPropertyHandler("VERSION", [&hasVersion](TestObj* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        hasVersion = true;
        assert(value == "2.0");
    });


    auto evAdapter = std::shared_ptr<VObjectAdapter<TestObj>>(new  VObjectAdapter<TestObj>);
    evAdapter->setObjectFactory([&objectCount](const std::basic_string<char>& object_type) -> TestObj* {
        //objectCount++;
        return new TestObj;
    });
    evAdapter->addPropertyHandler("DESCRIPTION", [&hasVersion](TestObj* self, const std::basic_string<char>& value, const AbstractVObjectAdaptor::Parameters& params) {
        hasVersion = true;
        assert(value == "Networld+Interop  and Exhibit\\nAtlanta World Congress Center\\nAtlanta\\, Georgia");
    });

    ICSLoader loader;

    loader.registerVObjectAdaptor("VCALENDAR", calAdapter);
    loader.registerVObjectAdaptor("VEVENT"   , evAdapter );

    loader._test_CharToObj(fromRfc4);

    assert(objectCount);
    assert(hasVersion);
}

/**
* This is a basic example
*/
void testBasicEvent()
{
    static constexpr const char fromRfc[] = \
    "BEGIN:VCALENDAR\r\n"
    "PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
    "VERSION:2.0\r\n"
    "BEGIN:VEVENT\r\n"
    "DTSTAMP:19960704T120000Z\r\n"
    "UID:uid1@example.com\r\n"
    "ORGANIZER:mailto:jsmith@example.com\r\n"
    "DTSTART:19960918T143000Z\r\n"
    "DTEND:19960920T220000Z\r\n"
    "STATUS:CONFIRMED\r\n"
    "CATEGORIES;prop3=value3:CONFERENCE\r\n"
    "SUMMARY;prop=value;prop2=\"value2\":Networld+Interop Conference\r\n"
    "DESCRIPTION:Networld+Interop Conference\r\n"
    "  and Exhibit\\nAtlanta World Congress Center\\n\r\n"
    " Atlanta\\, Georgia\r\n"
    "END:VEVENT\r\n"
    "END:VCALENDAR\r\n";

    TestObj expected {
        "VCALENDAR",
        {
            {
                "VEVENT",
                {},
                {
                    {"DTSTAMP", "19960704T120000Z", {}},
                    {"UID", "uid1@example.com", {}},
                    {"ORGANIZER", "mailto:jsmith@example.com", {}},
                    {"DTSTART", "19960918T143000Z", {}},
                    {"DTEND", "19960920T220000Z", {}},
                    {"STATUS", "CONFIRMED", {}},
                    {"CATEGORIES", "CONFERENCE", {
                        {"prop3", "value3"},
                    }},
                    {"SUMMARY", "Networld+Interop Conference", {
                        // This tests that the state was properly restored if
                        // previous properties have parameters
                        {"prop", "value"},
                        {"prop2", "value2"},
                    }},
                    {"DESCRIPTION", "Networld+Interop Conference and Exhibit\\nAtlanta World Congress Center\\nAtlanta\\, Georgia", {}},
                }
            }
        },
        {
            {"PRODID", "-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN", {}},
            {"VERSION", "2.0", {}},
        }
    };

    TestObj::isEqual(fromRfc, expected);
}

/**
 * This test the ',' ';' and ':' escaping
 */
void testParametersQuoting()
{
    static constexpr const char fromRfc2[] = \
        "BEGIN:VCALENDAR\r\n"
        "PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
        "VERSION:2.0\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION;foo=bar;foo2=\"bar\";foo3=b\\;\\:ar:Networld+Interop Conference\r\n"
        "  and Exhibit\\nAtlanta World Cong;;;ress Center\\n\r\n"
        " Atlanta\\, Georgia\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n";

        TestObj expected {
        "VCALENDAR",
        {
            {
                "VEVENT",
                {},
                {
                    {
                        "DESCRIPTION",
                        "Networld+Interop Conference and Exhibit\\nAtlanta World"
                        " Cong;;;ress Center\\nAtlanta\\, Georgia",
                        {
                            { "foo" , "bar"   },
                            { "foo2", "bar"   },
                            { "foo3", "b;:ar" },
                        }
                    },
                }
            }
        },
        {
            {"PRODID", "-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN", {}},
            {"VERSION", "2.0", {}},
        }
    };

    TestObj::isEqual(fromRfc2, expected);
}

void testMultipleChildreb()
{
    static constexpr const char fromRfc2[] = \
        "BEGIN:VCALENDAR\r\n"
        "PRODID:-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN\r\n"
        "VERSION:2.0\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION:event1\r\n"
        "END:VEVENT\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION:event2\r\n"
        "END:VEVENT\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION:event3\r\n"
        "END:VEVENT\r\n"
        "BEGIN:VEVENT\r\n"
        "DESCRIPTION:event4\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n";

        TestObj expected {
        "VCALENDAR",
        {
            {
                "VEVENT",
                {},
                { { "DESCRIPTION", "event1", { } }, }
            },
            {
                "VEVENT",
                {},
                { { "DESCRIPTION", "event2", { } }, }
            },
            {
                "VEVENT",
                {},
                { { "DESCRIPTION", "event3", { } }, }
            },
            {
                "VEVENT",
                {},
                { { "DESCRIPTION", "event4", { } }, }
            }
        },
        {
            {"PRODID", "-//xyz Corp//NONSGML PDA Calendar Version 1.0//EN", {}},
            {"VERSION", "2.0", {}},
        }
    };

    TestObj::isEqual(fromRfc2, expected);
}

/**
 * Early batch of tests for the ICS loader and builder.
 *
 * They are not yet real unit tests, that will come later.
 */
int main()
{
    testQuoting();

    testBasicEvent();

    testParametersQuoting();

    testMultipleChildreb();

    return 0;
}
