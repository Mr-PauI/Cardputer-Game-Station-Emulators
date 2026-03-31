#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void genesis_save_init(const char* romPathOrName);
void genesis_save_load(void);
void genesis_save_tick(void);
void genesis_save_request_flush(void);
void genesis_save_force_flush(void);
void genesis_save_mark_dirty_c(void);

#ifdef __cplusplus
}
#endif