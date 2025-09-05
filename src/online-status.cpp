// Order includes so OBS types are visible before we declare callbacks
#include "online_status.hpp"
#include <string>
#include <cmath>
#include <utility>

/*
    Show an image or text when the streamer is having connection problems or dropping frames
*/

// OnlineStatus is declared in online_status.hpp

// ------------------------ Readability helpers ------------------------
static inline bool dropping_blink_ok(const OnlineStatus *s)
{
    return !s->drop_blink_enabled || s->drop_blink_on;
}

static inline bool stable_blink_ok(const OnlineStatus *s)
{
    return !s->stable_blink_enabled || s->stable_blink_on;
}

static inline bool should_show_dropping(const OnlineStatus *s)
{
    return (s->auto_visible || s->visible) && dropping_blink_ok(s);
}

static inline bool should_show_stable(const OnlineStatus *s)
{
    return s->stable_visible && s->stable_enabled && stable_blink_ok(s);
}

static inline void sync_child_enabled(OnlineStatus *s)
{
    const bool show_drop = should_show_dropping(s);
    const bool show_stable = should_show_stable(s);
    if (s->status_text)
        obs_source_set_enabled(s->status_text, show_drop && s->content_mode == 0);
    if (s->status_image)
        obs_source_set_enabled(s->status_image, show_drop && s->content_mode == 1);
    if (s->status_text_stable)
        obs_source_set_enabled(s->status_text_stable, show_stable && s->stable_mode == 0);
    if (s->status_image_stable)
        obs_source_set_enabled(s->status_image_stable, show_stable && s->stable_mode == 1);
}

static inline void release_source(obs_source_t *&src)
{
    if (src) {
        obs_source_release(src);
        src = nullptr;
    }
}

const char *online_status_get_name(void *)
{
	return "Online Status";
}

void online_status_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "status_text", "");
    obs_data_set_default_string(settings, "image_path", "");
    obs_data_set_default_int(settings, "content_mode", 0);
    // UI section (pseudo-tabs): 0=Dropping,1=Stable,2=Blink,3=Advanced
    obs_data_set_default_int(settings, "ui_section", 0);
    // Stable overlay defaults
    obs_data_set_default_bool(settings, "stable_enabled", true);
    obs_data_set_default_int(settings, "stable_mode", 0);
    obs_data_set_default_string(settings, "stable_text", "Connection is stable again");
    obs_data_set_default_string(settings, "stable_image_path", "");
    obs_data_set_default_double(settings, "stable_duration_sec", 3.0);
    obs_data_set_default_double(settings, "drop_threshold_pct", 1.0);
    obs_data_set_default_double(settings, "hide_after_sec", 3.0);
    obs_data_set_default_bool(settings, "visible", false);
    // Advanced test defaults
    obs_data_set_default_bool(settings, "test_force_drop", false);
    // Blink defaults (separate)
    obs_data_set_default_bool(settings, "drop_blink_enabled", false);
    obs_data_set_default_double(settings, "drop_blink_rate_hz", 1.0);
    obs_data_set_default_bool(settings, "stable_blink_enabled", false);
    obs_data_set_default_double(settings, "stable_blink_rate_hz", 1.0);
}

void online_status_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<OnlineStatus *>(data);
    s->visible = obs_data_get_bool(settings, "visible");
    s->content_mode = (int)obs_data_get_int(settings, "content_mode");
	s->drop_threshold_pct = obs_data_get_double(settings, "drop_threshold_pct");
	s->hide_after_sec = (float)obs_data_get_double(settings, "hide_after_sec");
    // Advanced test flag
    s->test_force_drop = obs_data_get_bool(settings, "test_force_drop");
    // Blink settings (separate)
    s->drop_blink_enabled = obs_data_get_bool(settings, "drop_blink_enabled");
    s->drop_blink_rate_hz = obs_data_get_double(settings, "drop_blink_rate_hz");
    if (s->drop_blink_rate_hz < 0.0)
        s->drop_blink_rate_hz = 0.0;
    s->stable_blink_enabled = obs_data_get_bool(settings, "stable_blink_enabled");
    s->stable_blink_rate_hz = obs_data_get_double(settings, "stable_blink_rate_hz");
    if (s->stable_blink_rate_hz < 0.0)
        s->stable_blink_rate_hz = 0.0;

	const char *txt = obs_data_get_string(settings, "status_text");
	s->text = txt ? txt : "";
    const char *img = obs_data_get_string(settings, "image_path");
    s->image_path = img ? img : "";

    // Stable overlay settings
    s->stable_enabled = obs_data_get_bool(settings, "stable_enabled");
    s->stable_mode = (int)obs_data_get_int(settings, "stable_mode");
    s->stable_duration_sec = (float)obs_data_get_double(settings, "stable_duration_sec");
    const char *stxt = obs_data_get_string(settings, "stable_text");
    s->stable_text_msg = stxt ? stxt : "";
    const char *simg = obs_data_get_string(settings, "stable_image_path");
    s->stable_image_path = simg ? simg : "";

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

    // Update stable children
    if (s->status_text_stable) {
        obs_data_t *child = obs_data_create();
        obs_data_set_string(child, "text", s->stable_text_msg.c_str());
        obs_source_update(s->status_text_stable, child);
        obs_data_release(child);
    }
    if (s->status_image_stable) {
        obs_data_t *imgset = obs_data_create();
        obs_data_set_string(imgset, "file", s->stable_image_path.c_str());
        obs_source_update(s->status_image_stable, imgset);
        obs_data_release(imgset);
    }

    // Enable only the active child
    sync_child_enabled(s);
}

void online_status_video_tick(void *data, float seconds)
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

    bool prev_auto_visible = s->auto_visible;

    // Test override: force dropping overlay regardless of streaming state
    if (s->test_force_drop) {
        s->auto_visible = true;
        s->since_last_drop = 0.0f;
        s->stable_visible = false;
        s->stable_timer = 0.0f;
    } else if (!streaming_active) {
        // Not streaming: reset and hide
        s->prev_total = s->prev_dropped = 0;
        s->since_last_drop = 0.0f;
        s->auto_visible = false;
        s->stable_visible = false;
        s->stable_timer = 0.0f;
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
            // Any new drop cancels stable overlay
            s->stable_visible = false;
            s->stable_timer = 0.0f;
        } else {
            s->since_last_drop += seconds;
            if (s->since_last_drop >= s->hide_after_sec) {
                s->auto_visible = false;
            }
        }

        // Handle transition to stable overlay
        if (s->stable_enabled) {
            if (prev_auto_visible && !s->auto_visible) {
                s->stable_visible = true;
                s->stable_timer = s->stable_duration_sec;
            }
            if (s->stable_visible) {
                s->stable_timer -= seconds;
                if (s->stable_timer <= 0.0f) {
                    s->stable_visible = false;
                    s->stable_timer = 0.0f;
                }
            }
        } else {
            s->stable_visible = false;
            s->stable_timer = 0.0f;
        }
    }

    // Blink update (separate)
    if (s->drop_blink_enabled && s->drop_blink_rate_hz > 0.0) {
        const double period_d = 1.0 / s->drop_blink_rate_hz;
        s->drop_blink_phase += seconds;
        if (s->drop_blink_phase >= period_d)
            s->drop_blink_phase = fmod(s->drop_blink_phase, period_d);
        s->drop_blink_on = (s->drop_blink_phase < (period_d * 0.5));
    } else {
        s->drop_blink_on = true;
        s->drop_blink_phase = 0.0;
    }
    if (s->stable_blink_enabled && s->stable_blink_rate_hz > 0.0) {
        const double period_s = 1.0 / s->stable_blink_rate_hz;
        s->stable_blink_phase += seconds;
        if (s->stable_blink_phase >= period_s)
            s->stable_blink_phase = fmod(s->stable_blink_phase, period_s);
        s->stable_blink_on = (s->stable_blink_phase < (period_s * 0.5));
    } else {
        s->stable_blink_on = true;
        s->stable_blink_phase = 0.0;
    }

    // Keep children enabled in sync with selected mode, blink and stable state
    sync_child_enabled(s);
}

// Test buttons are implemented in properties translation unit

void *online_status_create(obs_data_t *settings, obs_source_t *owner)
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

    // Create stable children
    obs_data_t *stable_text = obs_data_create();
    obs_data_set_string(stable_text, "text", obs_data_get_string(settings, "stable_text"));
    const char *stable_text_id = "text_ft2_source_v2";
#ifdef _WIN32
    stable_text_id = "text_gdiplus";
#endif
    s->status_text_stable = obs_source_create_private(stable_text_id, "online-status:text-stable", stable_text);
    if (!s->status_text_stable) {
        const char *fallback_id2 =
#ifdef _WIN32
            "text_ft2_source_v2";
#else
            "text_gdiplus";
#endif
        s->status_text_stable = obs_source_create_private(fallback_id2, "online-status:text-stable", stable_text);
        if (!s->status_text_stable) {
            blog(LOG_ERROR, "[online-status] Failed to create Stable Text child (tried %s then %s)", stable_text_id, fallback_id2);
        }
    }
    obs_data_release(stable_text);

    obs_data_t *stable_img = obs_data_create();
    obs_data_set_string(stable_img, "file", obs_data_get_string(settings, "stable_image_path"));
    s->status_image_stable = obs_source_create_private("image_source", "online-status:image-stable", stable_img);
    obs_data_release(stable_img);
    if (!s->status_image_stable) {
        blog(LOG_ERROR, "[online-status] Failed to create Stable Image child");
    }

	online_status_update(s, settings);
	return s;
}

void online_status_destroy(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (s) {
        release_source(s->status_text);
        release_source(s->status_image);
        release_source(s->status_text_stable);
        release_source(s->status_image_stable);
        delete s;
    }
}
uint32_t online_status_get_width(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    const bool show_alert = should_show_dropping(s);
    const bool show_stable = should_show_stable(s);
    if (show_alert) {
        if (s->content_mode == 1 && s->status_image)
            return obs_source_get_width(s->status_image);
        return s->status_text ? obs_source_get_width(s->status_text) : 0;
    }
    if (show_stable) {
        if (s->stable_mode == 1 && s->status_image_stable)
            return obs_source_get_width(s->status_image_stable);
        return s->status_text_stable ? obs_source_get_width(s->status_text_stable) : 0;
    }
    // Fallback to dropping-mode size
    if (s->content_mode == 1 && s->status_image)
        return obs_source_get_width(s->status_image);
    return s->status_text ? obs_source_get_width(s->status_text) : 0;
}

uint32_t online_status_get_height(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    const bool show_alert = should_show_dropping(s);
    const bool show_stable = should_show_stable(s);
    if (show_alert) {
        if (s->content_mode == 1 && s->status_image)
            return obs_source_get_height(s->status_image);
        return s->status_text ? obs_source_get_height(s->status_text) : 0;
    }
    if (show_stable) {
        if (s->stable_mode == 1 && s->status_image_stable)
            return obs_source_get_height(s->status_image_stable);
        return s->status_text_stable ? obs_source_get_height(s->status_text_stable) : 0;
    }
    if (s->content_mode == 1 && s->status_image)
        return obs_source_get_height(s->status_image);
    return s->status_text ? obs_source_get_height(s->status_text) : 0;
}
void online_status_video_render(void *data, gs_effect_t * /*effect*/)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return;
    if (should_show_dropping(s)) {
        if (s->content_mode == 1) {
            if (s->status_image)
                obs_source_video_render(s->status_image);
        } else {
            if (s->status_text)
                obs_source_video_render(s->status_text);
        }
        return;
    }
    if (should_show_stable(s)) {
        if (s->stable_mode == 1) {
            if (s->status_image_stable)
                obs_source_video_render(s->status_image_stable);
        } else {
            if (s->status_text_stable)
                obs_source_video_render(s->status_text_stable);
        }
    }
}

// online_status_properties is implemented in properties translation unit

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
