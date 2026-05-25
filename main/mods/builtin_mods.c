#include "mods/builtin_mods.h"

extern const brn_mod_t brn_mod_tool_web_search;
extern const brn_mod_t brn_mod_tool_get_time;
extern const brn_mod_t brn_mod_tool_files;
extern const brn_mod_t brn_mod_tool_tts;
extern const brn_mod_t brn_mod_tool_gpio;
extern const brn_mod_t brn_mod_tool_cron;
extern const brn_mod_t brn_mod_tool_memory;

static const brn_mod_t *const s_builtin_mods[] = {
    &brn_mod_tool_get_time,
    &brn_mod_tool_files,
    &brn_mod_tool_tts,
    &brn_mod_tool_gpio,
    &brn_mod_tool_cron,
    &brn_mod_tool_web_search,
    &brn_mod_tool_memory,
};

const brn_mod_t *const *brn_builtin_mods_get(void)
{
    return s_builtin_mods;
}

size_t brn_builtin_mods_count(void)
{
    return sizeof(s_builtin_mods) / sizeof(s_builtin_mods[0]);
}
