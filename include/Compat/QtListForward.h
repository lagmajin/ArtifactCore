#pragma once

// Qt's QList declares this global test friend unconditionally.  The forward
// declaration must precede every imported Qt header, including module units.
class tst_QList;

// Qt 6.10 QList uses std::partial_ordering in its comparison operators.  MSVC
// does not pull in <compare> transitively when Qt headers arrive via /external:I
// inside a C++ module unit, so include it here alongside the friend fix.
#include <compare>
