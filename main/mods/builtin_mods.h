#pragma once

#include <stddef.h>

#include "core/mod/brn_mod.h"

const brn_mod_t *const *brn_builtin_mods_get(void);
size_t brn_builtin_mods_count(void);
