// state.cpp

#include "state.h"
#include "log.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

static ap_state_t s_state;
static SemaphoreHandle_t s_mutex;

static const char *TAG = "state";

void ap_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.status = AP_STATUS_BOOT;
    s_state.progress = 255;
    s_mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(s_mutex != NULL);
}

void ap_state_lock(void)   { xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY); }
void ap_state_unlock(void) { xSemaphoreGiveRecursive(s_mutex); }

void ap_state_get(ap_state_t *out)
{
    ap_state_lock();
    *out = s_state;
    ap_state_unlock();
}

void ap_state_set(const ap_state_t *in)
{
    if (!in) return;
    ap_state_lock();
    s_state = *in;
    if (s_state.progress > 100 && s_state.progress != 255) {
        s_state.progress = 100;
    }
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

void ap_state_set_status(ap_status_t s)
{
    ap_state_lock();
    if (s_state.status != s) {
        s_state.status = s;
        s_state.last_update_ms = esp_timer_get_time() / 1000;
    }
    ap_state_unlock();
}

void ap_state_set_tool(const char *tool)
{
    if (!tool) return;
    ap_state_lock();
    strncpy(s_state.tool, tool, sizeof(s_state.tool) - 1);
    s_state.tool[sizeof(s_state.tool) - 1] = '\0';
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

void ap_state_set_message(const char *msg)
{
    if (!msg) return;
    ap_state_lock();
    strncpy(s_state.message, msg, sizeof(s_state.message) - 1);
    s_state.message[sizeof(s_state.message) - 1] = '\0';
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

void ap_state_set_progress(uint8_t p)
{
    ap_state_lock();
    s_state.progress = p;
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

void ap_state_set_seq(uint32_t seq)
{
    ap_state_lock();
    s_state.seq = seq;
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

void ap_state_mark_updated(void)
{
    ap_state_lock();
    s_state.last_update_ms = esp_timer_get_time() / 1000;
    ap_state_unlock();
}

bool ap_state_stale_ms(uint32_t ms)
{
    ap_state_lock();
    int64_t now = esp_timer_get_time() / 1000;
    bool stale = (now - s_state.last_update_ms) > (int64_t)ms;
    ap_state_unlock();
    return stale;
}

const char *ap_status_label(ap_status_t s)
{
    switch (s) {
        case AP_STATUS_BOOT:          return "BOOT";
        case AP_STATUS_IDLE:          return "IDLE";
        case AP_STATUS_PROCESSING:    return "PROCESSING";
        case AP_STATUS_ERROR:         return "ERROR";
        case AP_STATUS_NO_CONNECTION: return "NO LINK";
    }
    return "?";
}
