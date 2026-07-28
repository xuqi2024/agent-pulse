// state.h — global shared state (status/tool/message + watchdog timestamps)

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    AP_STATUS_BOOT = 0,        // initial state before first host message
    AP_STATUS_IDLE,            // host says: nothing happening
    AP_STATUS_PROCESSING,      // host says: agent is working
    AP_STATUS_ERROR,           // host says: error / permission required
    AP_STATUS_NO_CONNECTION,   // watchdog tripped
} ap_status_t;

typedef struct {
    ap_status_t status;
    char        tool[16];      // e.g. "Bash", "Read", "Edit" — null-terminated
    char        message[60];   // short action description
    uint8_t     progress;      // 0-100, 255 = N/A
    uint32_t    seq;           // last sequence number observed
    int64_t     last_update_ms;// esp_timer_get_time() / 1000 of last message
} ap_state_t;

// Initialize the global state, create its mutex, and reset to BOOT.
void ap_state_init(void);

// Lock / unlock helpers (FreeRTOS recursive mutex).
void ap_state_lock(void);
void ap_state_unlock(void);

// Snapshot copy of the current state. Caller does NOT need to hold the lock.
void ap_state_get(ap_state_t *out);

// Atomic replace. String fields are truncated; progress is clamped.
void ap_state_set(const ap_state_t *in);

// Convenience setters used by protocol.cpp.
void ap_state_set_status(ap_status_t s);
void ap_state_set_tool(const char *tool);
void ap_state_set_message(const char *msg);
void ap_state_set_progress(uint8_t p);
void ap_state_set_seq(uint32_t seq);
void ap_state_mark_updated(void);

// Watchdog helper: returns true if we have not heard from the host in `ms`.
bool ap_state_stale_ms(uint32_t ms);

// Human-readable label for a status.
const char *ap_status_label(ap_status_t s);
