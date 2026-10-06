#pragma once

// M0: print chip, flash and heap information once.
void m0_print_chip_info(void);

// M0: print a seconds counter forever (never returns, so call it last).
void m0_run_counter(void);
