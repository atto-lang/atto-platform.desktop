/* Host-only symbol fixture. NOT a compiler-emitted Oreslang guest.
 * Executes the same C i64 exports expected from the real LLVM smoke, so
 * ordinary platform CTest can cover ABI boundaries without claiming parity. */
#include <stdint.h>
int64_t desktop_next_frame(int64_t frame) { return frame + 1; }
int64_t desktop_initial_width(void) { return 800; }
int64_t desktop_initial_height(void) { return desktop_next_frame(499); }
