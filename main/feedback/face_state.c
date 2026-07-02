#include "feedback/face_state.h"

#include "esp_err.h"

__attribute__((weak)) esp_err_t brn_face_set(const char *emotion, uint32_t duration_ms)
{
    (void)emotion;
    (void)duration_ms;
    return ESP_ERR_NOT_SUPPORTED;
}
