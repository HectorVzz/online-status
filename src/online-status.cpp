/*
    Show an image or text when the streamer is having connection problems or dropping frames
*/

#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <string>
#include <cmath>

struct OnlineStatus {
	obs_source_t *status_text = nullptr;
	obs_source_t *status_image = nullptr;
	std::string text;
    std::string image_path;
    bool visible = false;
    bool auto_visible = false;
    int content_mode = 0; // 0 = text, 1 = image
    double drop_threshold_pct = 1.0; // show when pct dropped in interval >= this
    float hide_after_sec = 3.0f;     // hide after this many seconds without drops
    uint64_t prev_total = 0;
    uint64_t prev_dropped = 0;
    float since_last_drop = 0.0f;

    // Blink config/state
    bool blink_enabled = false;
    double blink_rate_hz = 1.0; // blinks per second
    double blink_phase = 0.0;   // seconds into current period
    bool blink_on = true;       // current half-cycle visible?
};

static const char *online_status_get_name(void *)
{
	return "Online Status";
}

static void online_status_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "status_text", "");
    obs_data_set_default_string(settings, "image_path", "");
    obs_data_set_default_int(settings, "content_mode", 0);
    obs_data_set_default_double(settings, "drop_threshold_pct", 1.0);
    obs_data_set_default_double(settings, "hide_after_sec", 3.0);
    obs_data_set_default_bool(settings, "visible", false);
    // Blink defaults
    obs_data_set_default_bool(settings, "blink_enabled", false);
    obs_data_set_default_double(settings, "blink_rate_hz", 1.0);
}

static void online_status_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<OnlineStatus *>(data);
	s->auto_visible = obs_data_get_bool(settings, "auto_visible");
    s->visible = obs_data_get_bool(settings, "visible");
    s->content_mode = (int)obs_data_get_int(settings, "content_mode");
	s->drop_threshold_pct = obs_data_get_double(settings, "drop_threshold_pct");
	s->hide_after_sec = (float)obs_data_get_double(settings, "hide_after_sec");
    // Blink settings
    s->blink_enabled = obs_data_get_bool(settings, "blink_enabled");
    s->blink_rate_hz = obs_data_get_double(settings, "blink_rate_hz");
    if (s->blink_rate_hz < 0.0)
        s->blink_rate_hz = 0.0;

	const char *txt = obs_data_get_string(settings, "status_text");
	s->text = txt ? txt : "";
    const char *img = obs_data_get_string(settings, "image_path");
    s->image_path = img ? img : "";

	if (s->status_text) {
		obs_data_t *child = obs_data_create();
		obs_data_set_string(child, "text", s->text.c_str());
		// Optional: font, color, outline, bg
		// obs_data_set_obj(child, "font", <obs_data with size/family/etc>);
		obs_source_update(s->status_text, child);
		obs_data_release(child);
	}

    if (s->status_image) {
        obs_data_t *imgset = obs_data_create();
        obs_data_set_string(imgset, "file", s->image_path.c_str());
        obs_source_update(s->status_image, imgset);
        obs_data_release(imgset);
	}

    const bool effective_visible = (s->auto_visible || s->visible);
    // Enable only the active child
    if (s->status_text)
        obs_source_set_enabled(s->status_text, effective_visible && s->content_mode == 0);
    if (s->status_image)
        obs_source_set_enabled(s->status_image, effective_visible && s->content_mode == 1);
}

static void online_status_video_tick(void *data, float seconds)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return;

    bool streaming_active = false;
    uint64_t total = 0, dropped = 0;

    obs_output_t *out = obs_frontend_get_streaming_output();
    if (out) {
        streaming_active = obs_output_active(out);
        if (streaming_active) {
            // Network dropped/total frames
            dropped = obs_output_get_frames_dropped(out);
            total = obs_output_get_total_frames(out);
        }
        obs_output_release(out);
    }

    if (!streaming_active) {
        // Not streaming: reset and hide
        s->prev_total = s->prev_dropped = 0;
        s->since_last_drop = 0.0f;
        s->auto_visible = false;
    } else {
        // Reset counters if OBS restarted stats
        if (total < s->prev_total || dropped < s->prev_dropped) {
            s->prev_total = total;
            s->prev_dropped = dropped;
        }

        uint64_t d_total = total - s->prev_total;
        uint64_t d_drop  = dropped - s->prev_dropped;
        s->prev_total = total;
        s->prev_dropped = dropped;

        bool spiked = false;
        if (d_total > 0 && d_drop > 0) {
            double pct = (double)d_drop * 100.0 / (double)d_total;
            spiked = (pct >= s->drop_threshold_pct);
        }

        if (spiked) {
            s->auto_visible = true;
            s->since_last_drop = 0.0f;
        } else {
            s->since_last_drop += seconds;
            if (s->since_last_drop >= s->hide_after_sec) {
                s->auto_visible = false;
            }
        }
    }

    // Blink update
    if (s->blink_enabled && s->blink_rate_hz > 0.0) {
        const double period = 1.0 / s->blink_rate_hz;
        s->blink_phase += seconds;
        if (s->blink_phase >= period)
            s->blink_phase = fmod(s->blink_phase, period);
        // 50% duty cycle: visible in first half of the period
        s->blink_on = (s->blink_phase < (period * 0.5));
    } else {
        s->blink_on = true;
        s->blink_phase = 0.0;
    }

    // Keep children enabled in sync with selected mode and blink
    const bool effective_visible = (s->auto_visible || s->visible) && (!s->blink_enabled || s->blink_on);
    if (s->status_text)
        obs_source_set_enabled(s->status_text, effective_visible && s->content_mode == 0);
    if (s->status_image)
        obs_source_set_enabled(s->status_image, effective_visible && s->content_mode == 1);
}

static void *online_status_create(obs_data_t *settings, obs_source_t *owner)
{
	UNUSED_PARAMETER(owner);
	auto *s = new OnlineStatus();

	// Create private child: "Text (FreeType 2)"
    obs_data_t *child = obs_data_create();
    obs_data_set_string(child, "text", obs_data_get_string(settings, "status_text"));
    const char *text_source_id = "text_ft2_source_v2";
#ifdef _WIN32
    text_source_id = "text_gdiplus"; // Prefer GDI+ on Windows
#endif
    s->status_text = obs_source_create_private(text_source_id, "online-status:text", child);
    if (!s->status_text) {
        // Fallback to the other text source if primary is unavailable
        const char *fallback_id =
#ifdef _WIN32
            "text_ft2_source_v2";
#else
            "text_gdiplus";
#endif
        s->status_text = obs_source_create_private(fallback_id, "online-status:text", child);
        if (!s->status_text) {
            blog(LOG_ERROR, "[online-status] Failed to create Text child (tried %s then %s)", text_source_id, fallback_id);
        }
    }
	obs_data_release(child);

    if (!s->status_text) {
        blog(LOG_ERROR, "[online-status] No text source could be created");
    }

    // Create private child: "Image"
    obs_data_t *imgset = obs_data_create();
    obs_data_set_string(imgset, "file", obs_data_get_string(settings, "image_path"));
    s->status_image = obs_source_create_private("image_source", "online-status:image", imgset);
    obs_data_release(imgset);

    if (!s->status_image) {
        blog(LOG_ERROR, "[online-status] Failed to create Image child (id=image_source)");
    }

	online_status_update(s, settings);
	return s;
}

static void online_status_destroy(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
	if (s) {
		if (s->status_text) {
			obs_source_release(s->status_text);
			s->status_text = nullptr;
		}
        if (s->status_image) {
            obs_source_release(s->status_image);
            s->status_image = nullptr;
        }
		delete s;
	}
}
static uint32_t online_status_get_width(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    if (s->content_mode == 1 && s->status_image)
        return obs_source_get_width(s->status_image);
	return s->status_text ? obs_source_get_width(s->status_text) : 0;
}

static uint32_t online_status_get_height(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    if (s->content_mode == 1 && s->status_image)
        return obs_source_get_height(s->status_image);
	return s->status_text ? obs_source_get_height(s->status_text) : 0;
}
static void online_status_video_render(void *data, gs_effect_t * /*effect*/)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return;
    const bool effective_visible = (s->auto_visible || s->visible) && (!s->blink_enabled || s->blink_on);
    if (!effective_visible)
        return;

    if (s->content_mode == 1) {
        if (s->status_image)
            obs_source_video_render(s->status_image);
    } else {
        if (s->status_text)
            obs_source_video_render(s->status_text);
    }
}

static obs_properties_t *online_status_properties(void * /*data*/)
{
	obs_properties_t *props = obs_properties_create();

    // Mode selector
    obs_property_t *mode = obs_properties_add_list(props, "content_mode", "Content Type",
                                                   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode, "Text", 0);
    obs_property_list_add_int(mode, "Image", 1);

	obs_properties_add_text(props, "status_text", "Enter the status text to display", OBS_TEXT_DEFAULT);
    obs_properties_add_path(props, "image_path", "Image file", OBS_PATH_FILE,
                            "Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)", nullptr);

	obs_properties_add_float_slider(props, "drop_threshold_pct", "Drop % threshold (per-interval)", 0.0, 100.0,
        0.1);
    obs_properties_add_float_slider(props, "hide_after_sec", "Hide after seconds without drops", 0.0, 30.0, 0.1);
    obs_properties_add_bool(props, "visible", "Test text that auto-shows when dropping frames(for debugging)");
    // Blink controls
    obs_properties_add_bool(props, "blink_enabled", "Blink");
    obs_properties_add_float_slider(props, "blink_rate_hz", "Blink rate (Hz)", 0.0, 10.0, 0.1);
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
    .get_defaults = online_status_defaults,
    .get_properties = online_status_properties,
    .update = online_status_update,
    .video_tick = online_status_video_tick,    // <- add tick
    .video_render = online_status_video_render,
};

void register_online_status_source(void)
{
	obs_register_source(&online_status_info);
}
