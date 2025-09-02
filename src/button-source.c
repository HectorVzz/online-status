/*
 Minimal button source: shows a single button in Properties. When pressed, logs a message.
*/

#include <obs-module.h>
#include <plugin-support.h>

struct button_source {
    obs_source_t *source;
};

static const char *button_source_get_name(void *unused)
{
    UNUSED_PARAMETER(unused);
    return obs_module_text("ButtonSource.Name");
}

static void *button_source_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    struct button_source *ctx = bzalloc(sizeof(*ctx));
    ctx->source = source;
    return ctx;
}

static void button_source_destroy(void *data)
{
    struct button_source *ctx = data;
    bfree(ctx);
}

static obs_properties_t *button_source_get_properties(void *data)
{
    struct button_source *ctx = data;
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p = obs_properties_add_button(props, "press",
                                                 obs_module_text("ButtonSource.Press"),
                                                 NULL);
    UNUSED_PARAMETER(p);
    UNUSED_PARAMETER(ctx);
    return props;
}

static void button_source_update(void *data, obs_data_t *settings)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(settings);
}

static void button_source_get_defaults(obs_data_t *settings)
{
    UNUSED_PARAMETER(settings);
}

static void button_source_tick(void *data, float seconds)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(seconds);
}

static void button_source_render(void *data, gs_effect_t *effect)
{
    UNUSED_PARAMETER(data);
    UNUSED_PARAMETER(effect);
}

static uint32_t button_source_get_width(void *data)
{
    UNUSED_PARAMETER(data);
    return 0; // no video output
}

static uint32_t button_source_get_height(void *data)
{
    UNUSED_PARAMETER(data);
    return 0; // no video output
}

static bool on_press(obs_properties_t *props, obs_property_t *property, void *data)
{
    UNUSED_PARAMETER(props);
    UNUSED_PARAMETER(property);
    struct button_source *ctx = data;
    const char *name = obs_source_get_name(ctx->source);
    obs_log(LOG_INFO, "Button pressed in source '%s'", name ? name : "<unnamed>");
    return true; // cause UI to refresh
}

static obs_properties_t *button_source_get_properties_with_handler(void *data)
{
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_button2(props, "press",
                               obs_module_text("ButtonSource.Press"),
                               on_press, data);
    return props;
}

static struct obs_source_info button_source_info = {
    .id = "button-source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO, // trivial, no actual video
    .get_name = button_source_get_name,
    .create = button_source_create,
    .destroy = button_source_destroy,
    .get_properties = button_source_get_properties_with_handler,
    .update = button_source_update,
    .get_defaults = button_source_get_defaults,
    .video_tick = button_source_tick,
    .video_render = button_source_render,
    .get_width = button_source_get_width,
    .get_height = button_source_get_height,
};

void register_button_source(void)
{
    obs_register_source(&button_source_info);
}
