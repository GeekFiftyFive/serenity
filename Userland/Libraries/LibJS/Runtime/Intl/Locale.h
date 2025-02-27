/*
 * Copyright (c) 2021-2022, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Value.h>
#include <LibLocale/Forward.h>

namespace JS::Intl {

class Locale final : public Object {
    JS_OBJECT(Locale, Object);

public:
    static ThrowCompletionOr<NonnullGCPtr<Locale>> create(Realm&, ::Locale::LocaleID);

    static constexpr auto relevant_extension_keys()
    {
        // 14.2.2 Internal slots, https://tc39.es/ecma402/#sec-intl.locale-internal-slots
        // The value of the [[RelevantExtensionKeys]] internal slot is « "ca", "co", "hc", "kf", "kn", "nu" ».
        // If %Collator%.[[RelevantExtensionKeys]] does not contain "kf", then remove "kf" from %Locale%.[[RelevantExtensionKeys]].
        // If %Collator%.[[RelevantExtensionKeys]] does not contain "kn", then remove "kn" from %Locale%.[[RelevantExtensionKeys]].

        // FIXME: We do not yet have an Intl.Collator object. For now, we behave as if "kf" and "kn" exist, as test262 depends on it.
        return AK::Array { "ca"sv, "co"sv, "hc"sv, "kf"sv, "kn"sv, "nu"sv };
    }

    virtual ~Locale() override = default;

    String const& locale() const { return m_locale; }
    void set_locale(String locale) { m_locale = move(locale); }

    bool has_calendar() const { return m_calendar.has_value(); }
    String const& calendar() const { return m_calendar.value(); }
    void set_calendar(String calendar) { m_calendar = move(calendar); }

    bool has_case_first() const { return m_case_first.has_value(); }
    String const& case_first() const { return m_case_first.value(); }
    void set_case_first(String case_first) { m_case_first = move(case_first); }

    bool has_collation() const { return m_collation.has_value(); }
    String const& collation() const { return m_collation.value(); }
    void set_collation(String collation) { m_collation = move(collation); }

    bool has_hour_cycle() const { return m_hour_cycle.has_value(); }
    String const& hour_cycle() const { return m_hour_cycle.value(); }
    void set_hour_cycle(String hour_cycle) { m_hour_cycle = move(hour_cycle); }

    bool has_numbering_system() const { return m_numbering_system.has_value(); }
    String const& numbering_system() const { return m_numbering_system.value(); }
    void set_numbering_system(String numbering_system) { m_numbering_system = move(numbering_system); }

    bool numeric() const { return m_numeric; }
    void set_numeric(bool numeric) { m_numeric = numeric; }

private:
    explicit Locale(Object& prototype);

    String m_locale;                     // [[Locale]]
    Optional<String> m_calendar;         // [[Calendar]]
    Optional<String> m_case_first;       // [[CaseFirst]]
    Optional<String> m_collation;        // [[Collation]]
    Optional<String> m_hour_cycle;       // [[HourCycle]]
    Optional<String> m_numbering_system; // [[NumberingSystem]]
    bool m_numeric { false };            // [[Numeric]]
};

// Table 1: WeekInfo Record Fields, https://tc39.es/proposal-intl-locale-info/#table-locale-weekinfo-record
struct WeekInfo {
    u8 minimal_days { 0 }; // [[MinimalDays]]
    u8 first_day { 0 };    // [[FirstDay]]
    Vector<u8> weekend;    // [[Weekend]]
};

Array* calendars_of_locale(VM&, Locale const&);
Array* collations_of_locale(VM&, Locale const& locale);
Array* hour_cycles_of_locale(VM&, Locale const& locale);
Array* numbering_systems_of_locale(VM&, Locale const&);
Array* time_zones_of_locale(VM&, StringView region);
StringView character_direction_of_locale(Locale const&);
WeekInfo week_info_of_locale(Locale const&);

}
