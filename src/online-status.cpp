/*
    Show an image or text when the streamer is having connection problems or dropping frames
*/

#include <obs-module.h>
#include <plugin-support.h>

struct OnlineStatus { /* empty for now */ };

static const char *online_status_get_name(void *)
{
    return "Online Status";
}

static void *online_status_create(obs_data_t *settings, obs_source_t *source)
{
    UNUSED_PARAMETER(settings);
    UNUSED_PARAMETER(source);
    return new OnlineStatus(); // could be nullptr, but a tiny struct is fine
}

static void online_status_destroy(void *data)
{
    delete static_cast<OnlineStatus *>(data);
}

static uint32_t online_status_get_width(void * /*data*/)
{
    return 300; // placeholder
}

static uint32_t online_status_get_height(void * /*data*/)
{
    return 80; // placeholder
}

static void online_status_video_render(void * /*data*/, gs_effect_t * /*effect*/)
{
    // no-op for now (nothing drawn)
}

static obs_properties_t *online_status_properties(void * /*data*/)
{
    return obs_properties_create();
}

static obs_source_info online_status_info = {
    .id = "online-status",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO,
    .get_name = online_status_get_name,
    .create = online_status_create,
    .destroy = online_status_destroy,
    .get_width = online_status_get_width,
    .get_height = online_status_get_height,
    .get_properties = online_status_properties,
    .video_render = online_status_video_render,
};

void register_online_status_source(void)
{
    obs_register_source(&online_status_info);
}
