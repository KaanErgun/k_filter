/* Host-side harness: runs the C fixed-point EMA (ema_fixed) over int16 samples
 * read from stdin (one decimal per line), printing each output. Used by the HDL
 * parity harness to prove C (MCU) == Python == Verilog == VHDL for the EMA.
 * ALPHA must match sim/fixed_models.EMA_ALPHA_Q15.
 */
#include <stdio.h>
#include "k_filter_fixed.h"

int main(void) {
    EMAFixed e;
    long v;
    if (ema_fixed_init(&e, (int16_t)3277) != KF_OK) return 2;
    while (scanf("%ld", &v) == 1) {
        printf("%d\n", (int)ema_fixed_update(&e, (int16_t)v));
    }
    return 0;
}
