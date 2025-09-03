/*
    Show an image or text when the streamer is having connection problems or dropping frames
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <string>

struct OnlineStatus { 
    obs_source_t *status_text = nullptr;
    std::string text;
};

static const char *online_status_get_name(void *)
{
    return "Online Status";
}

static void online_status_update(void *data, obs_data_t *settings)
{
    auto *s = static_cast<OnlineStatus *>(data);
    const char *txt = obs_data_get_string(settings, "status_text");
    s->text = txt ? txt : "";

    if (s->status_text) {
        obs_data_t *child = obs_data_create();
        obs_data_set_string(child, "text", s->text.c_str());
        // Optional: font, color, outline, bg
        // obs_data_set_obj(child, "font", <obs_data with size/family/etc>);
        obs_source_update(s->status_text, child);
        obs_data_release(child);
    }
}

static void *online_status_create(obs_data_t *settings, obs_source_t *owner)
{
    UNUSED_PARAMETER(owner);
    auto *s = new OnlineStatus();

    // Create private child: "Text (FreeType 2)"
    obs_data_t *child = obs_data_create();
    obs_data_set_string(child, "text", obs_data_get_string(settings, "status_text"));
    s->status_text =
        obs_source_create_private("text_ft2_source_v2", "online-status:text", child);
    obs_data_release(child);

    online_status_update(s, settings);
    return s;
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
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_text(props, "status_text","Enter the status text to display", OBS_TEXT_DEFAULT);
    return props;
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
