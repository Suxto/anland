#include "../producers/kde/anland_backend/src/backends/anland/anland_frame_diagnostic.h"
#include <cassert>

int main()
{
    AnlandFrameCounters d;
    d.render(0, true, true);
    auto s = d.present(0, false, true, true);
    assert(s.painted && !s.regression && s.revision == 1);
    d.render(1, true, false); // catch-up, same scene revision
    s = d.present(1, false, true, true);
    assert(!s.regression && s.revision == 1);
    d.render(0, true, true); // new scene content
    d.present(0, false, true, true);
    s = d.present(1, true, false, true); // old slot without a paint
    assert(!s.painted && s.regression && s.revision == 1 && s.previousRevision == 2);
    assert(d.unpaintedWithDebt == 1 && d.regressions == 1 && d.unfenced == 1);
    d.render(2, true, true);
    s = d.present(1, true, true, false); // rejected mismatch is not a displayed regression
    assert(s.mismatch && !s.regression && d.mismatches == 1);
    s = d.present(-1, false, false, false);
    assert(!s.regression && !s.mismatch);
    d.resetSession();
    d.render(0, true, true);
    s = d.present(0, false, true, true);
    assert(!s.regression && s.revision == 1 && s.previousRevision == 0);
    d.render(8, false, false); // no array access for invalid slots
    s = d.present(8, false, true, false);
    assert(s.revision == 0 && !s.regression);
}
