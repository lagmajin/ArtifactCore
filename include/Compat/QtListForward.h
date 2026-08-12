#pragma once

// Qt's QList declares this global test friend unconditionally.  The forward
// declaration must precede every imported Qt header, including module units.
class tst_QList;
