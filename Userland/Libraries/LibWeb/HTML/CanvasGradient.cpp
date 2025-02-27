/*
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/QuickSort.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/HTML/CanvasGradient.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

namespace Web::HTML {

JS::NonnullGCPtr<CanvasGradient> CanvasGradient::create_radial(JS::Realm& realm, double x0, double y0, double r0, double x1, double y1, double r1)
{
    (void)x0;
    (void)y0;
    (void)r0;
    (void)x1;
    (void)y1;
    (void)r1;
    return realm.heap().allocate<CanvasGradient>(realm, realm, Type::Radial);
}

JS::NonnullGCPtr<CanvasGradient> CanvasGradient::create_linear(JS::Realm& realm, double x0, double y0, double x1, double y1)
{
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
    return realm.heap().allocate<CanvasGradient>(realm, realm, Type::Linear);
}

JS::NonnullGCPtr<CanvasGradient> CanvasGradient::create_conic(JS::Realm& realm, double start_angle, double x, double y)
{
    (void)start_angle;
    (void)x;
    (void)y;
    return realm.heap().allocate<CanvasGradient>(realm, realm, Type::Conic);
}

CanvasGradient::CanvasGradient(JS::Realm& realm, Type type)
    : PlatformObject(realm)
    , m_type(type)
{
}

CanvasGradient::~CanvasGradient() = default;

void CanvasGradient::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::CanvasGradientPrototype>(realm, "CanvasGradient"));
}

// https://html.spec.whatwg.org/multipage/canvas.html#dom-canvasgradient-addcolorstop
WebIDL::ExceptionOr<void> CanvasGradient::add_color_stop(double offset, DeprecatedString const& color)
{
    // 1. If the offset is less than 0 or greater than 1, then throw an "IndexSizeError" DOMException.
    if (offset < 0 || offset > 1)
        return WebIDL::IndexSizeError::create(realm(), "CanvasGradient color stop offset out of bounds");

    // 2. Let parsed color be the result of parsing color.
    auto parsed_color = Color::from_string(color);

    // 3. If parsed color is failure, throw a "SyntaxError" DOMException.
    if (!parsed_color.has_value())
        return WebIDL::SyntaxError::create(realm(), "Could not parse color for CanvasGradient");

    // 4. Place a new stop on the gradient, at offset offset relative to the whole gradient, and with the color parsed color.
    m_color_stops.append(ColorStop { offset, parsed_color.value() });

    // FIXME: If multiple stops are added at the same offset on a gradient, then they must be placed in the order added,
    //        with the first one closest to the start of the gradient, and each subsequent one infinitesimally further along
    //        towards the end point (in effect causing all but the first and last stop added at each point to be ignored).
    quick_sort(m_color_stops, [](auto& a, auto& b) { return a.offset < b.offset; });

    return {};
}

}
