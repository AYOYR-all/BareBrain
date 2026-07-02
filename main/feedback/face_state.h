#pragma once

#include <stdint.h>

#include "esp_err.h"

/*
 * Optional face/emotion feedback hook.
 *
 * The core firmware calls this without depending on a display plugin. A face
 * plugin may provide a strong implementation; otherwise the weak default is a
 * no-op.
 */
esp_err_t brn_face_set(const char *emotion, uint32_t duration_ms);
