#ifndef ROBOBASE_SAFE_APP_H
#define ROBOBASE_SAFE_APP_H

#include <stdint.h>

void robobase_safe_init(void);
void robobase_safe_tick_1ms(void);
uint8_t robobase_safe_process_pending_tick(void);
uint32_t robobase_safe_handle_frame(const uint8_t *rx, uint32_t rx_len, uint8_t *tx, uint32_t tx_size);

#endif /* ROBOBASE_SAFE_APP_H */
