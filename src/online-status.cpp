// Order includes so OBS types are visible before we declare callbacks
#include <obs-module.h>
#include <plugin-support.h>
#include <obs-frontend-api.h>
#include <string>
#include <cmath>
#include <utility>

// Property UI visibility refresher (C-callable for OBS callbacks)
static bool online_status_properties_refresh(obs_properties_t *props, obs_property_t * /*property*/, obs_data_t *settings)
{
    auto set_vis = [&](const char *name, bool v) {
        if (obs_property_t *pp = obs_properties_get(props, name))
            obs_property_set_visible(pp, v);
    };

    int section = (int)obs_data_get_int(settings, "ui_section");
    int mode_val = (int)obs_data_get_int(settings, "content_mode");

    // Dropping group inner properties
    if (obs_property_t *grp = obs_properties_get(props, "dropping_group")) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            bool show_drop = (section == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "content_mode"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "status_text"))
                obs_property_set_visible(pp, show_drop && mode_val == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "image_path"))
                obs_property_set_visible(pp, show_drop && mode_val == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_threshold_pct"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "hide_after_sec"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_blink_enabled"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_blink_rate_hz"))
                obs_property_set_visible(pp, show_drop);
        }
    }

    // Stable group inner properties
    if (obs_property_t *grp = obs_properties_get(props, "stable_group")) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            bool s_enabled_v = obs_data_get_bool(settings, "stable_enabled");
            int s_mode = (int)obs_data_get_int(settings, "stable_mode");
            bool show_inner = (section == 1) && s_enabled_v;

            if (obs_property_t *pp = obs_properties_get(inner, "stable_mode"))
                obs_property_set_visible(pp, section == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_text"))
                obs_property_set_visible(pp, show_inner && s_mode == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_image_path"))
                obs_property_set_visible(pp, show_inner && s_mode == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_enabled"))
                obs_property_set_visible(pp, section == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_duration_sec"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_blink_enabled"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_blink_rate_hz"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
        }
    }

    // Pseudo-tabs visibility (no separate Blink tab)
    bool show_dropping = (section == 0);
    bool show_stable   = (section == 1);
    bool show_adv      = (section == 2);

    set_vis("visible", show_adv);
    set_vis("test_force_drop", show_adv);
    set_vis("test_simulate_spike", show_adv);
    set_vis("test_show_stable", show_adv);
    set_vis("test_hide_all", show_adv);

    if (obs_property_t *grp = obs_properties_get(props, "dropping_group"))
        obs_property_set_visible(grp, show_dropping);
    if (obs_property_t *grp = obs_properties_get(props, "stable_group"))
        obs_property_set_visible(grp, show_stable);
    return true;
}
/*
    Show an image or text when the streamer is having connection problems or dropping frames
*/

struct OnlineStatus {
	obs_source_t *status_text = nullptr;
	obs_source_t *status_image = nullptr;
	// Stable-state children
	obs_source_t *status_text_stable = nullptr;
	obs_source_t *status_image_stable = nullptr;
	std::string text;
    std::string image_path;
    // Stable content
    std::string stable_text_msg;
    std::string stable_image_path;
    bool visible = false;
    bool auto_visible = false;
    int content_mode = 0; // 0 = text, 1 = image
    double drop_threshold_pct = 1.0; // show when pct dropped in interval >= this
    float hide_after_sec = 3.0f;     // hide after this many seconds without drops
    uint64_t prev_total = 0;
    uint64_t prev_dropped = 0;
    float since_last_drop = 0.0f;

    // Blink config/state (separate for dropping and stable overlays)
    bool drop_blink_enabled = false;
    double drop_blink_rate_hz = 1.0; // blinks per second
    double drop_blink_phase = 0.0;   // seconds into current period
    bool drop_blink_on = true;       // current half-cycle visible?

    bool stable_blink_enabled = false;
    double stable_blink_rate_hz = 1.0;
    double stable_blink_phase = 0.0;
    bool stable_blink_on = true;

    // Stable overlay options/state
    bool  stable_enabled = true;
    int   stable_mode = 0;            // 0=text, 1=image
    float stable_duration_sec = 3.0f; // how long to show after recovery
    float stable_timer = 0.0f;
    bool  stable_visible = false;
    // Testing helpers
    bool  test_force_drop = false;
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

static void online_status_update(void *data, obs_data_t *settings)
{
	auto *s = static_cast<OnlineStatus *>(data);
	s->auto_visible = obs_data_get_bool(settings, "auto_visible");
    s->visible = obs_data_get_bool(settings, "visible");
    s->content_mode = (int)obs_data_get_int(settings, "content_mode");
	s->drop_threshold_pct = obs_data_get_double(settings, "drop_threshold_pct");
	s->hide_after_sec = (float)obs_data_get_double(settings, "hide_after_sec");
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

    const bool drop_blink_ok = (!s->drop_blink_enabled || s->drop_blink_on);
    const bool stable_blink_ok = (!s->stable_blink_enabled || s->stable_blink_on);
    const bool show_alert = (s->auto_visible || s->visible) && drop_blink_ok;
    const bool show_stable = s->stable_visible && s->stable_enabled && stable_blink_ok;
    // Enable only the active child
    if (s->status_text)
        obs_source_set_enabled(s->status_text, show_alert && s->content_mode == 0);
    if (s->status_image)
        obs_source_set_enabled(s->status_image, show_alert && s->content_mode == 1);
    if (s->status_text_stable)
        obs_source_set_enabled(s->status_text_stable, show_stable && s->stable_mode == 0);
    if (s->status_image_stable)
        obs_source_set_enabled(s->status_image_stable, show_stable && s->stable_mode == 1);
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
    const bool blink_ok2_drop = (!s->drop_blink_enabled || s->drop_blink_on);
    const bool blink_ok2_stable = (!s->stable_blink_enabled || s->stable_blink_on);
    const bool show_alert2 = (s->auto_visible || s->visible) && blink_ok2_drop;
    const bool show_stable2 = s->stable_visible && s->stable_enabled && blink_ok2_stable;
    if (s->status_text)
        obs_source_set_enabled(s->status_text, show_alert2 && s->content_mode == 0);
    if (s->status_image)
        obs_source_set_enabled(s->status_image, show_alert2 && s->content_mode == 1);
    if (s->status_text_stable)
        obs_source_set_enabled(s->status_text_stable, show_stable2 && s->stable_mode == 0);
    if (s->status_image_stable)
        obs_source_set_enabled(s->status_image_stable, show_stable2 && s->stable_mode == 1);
}

// Advanced testing buttons
static bool online_status_btn_simulate_spike(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = true;
    s->since_last_drop = 0.0f;
    s->stable_visible = false;
    s->stable_timer = 0.0f;
    return true; // refresh UI
}

static bool online_status_btn_show_stable(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = false;
    s->stable_visible = true;
    s->stable_timer = s->stable_duration_sec;
    return true;
}

static bool online_status_btn_hide_all(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = false;
    s->stable_visible = false;
    s->stable_timer = 0.0f;
    return true;
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
        if (s->status_text_stable) {
            obs_source_release(s->status_text_stable);
            s->status_text_stable = nullptr;
        }
        if (s->status_image_stable) {
            obs_source_release(s->status_image_stable);
            s->status_image_stable = nullptr;
        }
		delete s;
	}
}
static uint32_t online_status_get_width(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    const bool drop_blink_ok = (!s->drop_blink_enabled || s->drop_blink_on);
    const bool stable_blink_ok = (!s->stable_blink_enabled || s->stable_blink_on);
    const bool show_alert = (s->auto_visible || s->visible) && drop_blink_ok;
    const bool show_stable = s->stable_visible && s->stable_enabled && stable_blink_ok;
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

static uint32_t online_status_get_height(void *data)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return 0;
    const bool drop_blink_ok = (!s->drop_blink_enabled || s->drop_blink_on);
    const bool stable_blink_ok = (!s->stable_blink_enabled || s->stable_blink_on);
    const bool show_alert = (s->auto_visible || s->visible) && drop_blink_ok;
    const bool show_stable = s->stable_visible && s->stable_enabled && stable_blink_ok;
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
static void online_status_video_render(void *data, gs_effect_t * /*effect*/)
{
	auto *s = static_cast<OnlineStatus *>(data);
    if (!s)
        return;
    const bool drop_blink_ok = (!s->drop_blink_enabled || s->drop_blink_on);
    if ((s->auto_visible || s->visible) && drop_blink_ok) {
        if (s->content_mode == 1) {
            if (s->status_image)
                obs_source_video_render(s->status_image);
        } else {
            if (s->status_text)
                obs_source_video_render(s->status_text);
        }
        return;
    }
    const bool stable_blink_ok = (!s->stable_blink_enabled || s->stable_blink_on);
    if (s->stable_visible && s->stable_enabled && stable_blink_ok) {
        if (s->stable_mode == 1) {
            if (s->status_image_stable)
                obs_source_video_render(s->status_image_stable);
        } else {
            if (s->status_text_stable)
                obs_source_video_render(s->status_text_stable);
        }
    }
}

static obs_properties_t *online_status_properties(void *data)
{
    UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

    // Section selector to simulate tabs
    obs_property_t *section = obs_properties_add_list(
        props, "ui_section", "Section",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(section, "Dropping", 0);
    obs_property_list_add_int(section, "Stable", 1);
    obs_property_list_add_int(section, "Advanced", 2);

    // Dropping overlay group
    obs_properties_t *dropping = obs_properties_create();
    obs_property_t *mode = obs_properties_add_list(dropping, "content_mode", "Content Type (when dropping)",
                                                   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode, "Text", 0);
    obs_property_list_add_int(mode, "Image", 1);

	obs_properties_add_text(dropping, "status_text", "Text to show while dropping", OBS_TEXT_DEFAULT);
    obs_properties_add_path(dropping, "image_path", "Image file (while dropping)", OBS_PATH_FILE,
                            "Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)", nullptr);

	obs_properties_add_float_slider(dropping, "drop_threshold_pct", "Drop % threshold (per-interval)", 0.0, 100.0,
        0.1);
    obs_properties_add_float_slider(dropping, "hide_after_sec", "Hide after seconds without drops", 0.0, 30.0, 0.1);
    // Blink controls (Dropping)
    obs_properties_add_bool(dropping, "drop_blink_enabled", "Blink (while dropping)");
    obs_properties_add_float_slider(dropping, "drop_blink_rate_hz", "Blink rate (Hz)", 0.0, 10.0, 0.1);

    obs_property_t *dropping_group = obs_properties_add_group(props, "dropping_group", "When dropping frames",
                                                             OBS_GROUP_NORMAL, dropping);
    obs_properties_add_bool(props, "visible", "Test text that auto-shows when dropping frames(for debugging)");

    // Stable overlay group
    obs_properties_t *stable = obs_properties_create();
    obs_properties_add_bool(stable, "stable_enabled", "Show message when connection is stable");
    obs_property_t *smode = obs_properties_add_list(stable, "stable_mode", "Content Type (when stable)",
                                                    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(smode, "Text", 0);
    obs_property_list_add_int(smode, "Image", 1);
    obs_properties_add_text(stable, "stable_text", "Stable text", OBS_TEXT_DEFAULT);
    obs_properties_add_path(stable, "stable_image_path", "Stable image file", OBS_PATH_FILE,
                            "Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)", nullptr);
    obs_properties_add_float_slider(stable, "stable_duration_sec", "Stable message duration (seconds)",
                                    0.0, 60.0, 0.1);
    // Blink controls (Stable)
    obs_properties_add_bool(stable, "stable_blink_enabled", "Blink (stable message)");
    obs_properties_add_float_slider(stable, "stable_blink_rate_hz", "Blink rate (Hz)", 0.0, 10.0, 0.1);

    obs_property_t *stable_group = obs_properties_add_group(props, "stable_group", "When connection stabilizes", OBS_GROUP_NORMAL, stable);

    // Advanced: testing controls
    obs_properties_add_bool(props, "test_force_drop", "Test: Force dropping overlay");
    obs_properties_add_button(props, "test_simulate_spike", "Test: Simulate drop spike", online_status_btn_simulate_spike);
    obs_properties_add_button(props, "test_show_stable", "Test: Show stable message now", online_status_btn_show_stable);
    obs_properties_add_button(props, "test_hide_all", "Test: Hide overlays", online_status_btn_hide_all);

    // Dynamic visibility handled by C-callback online_status_properties_refresh()

    // Hook callbacks
    obs_property_set_modified_callback(section, online_status_properties_refresh);
    obs_property_set_modified_callback(mode,    online_status_properties_refresh);
    // Hook inner dropping toggles
    if (obs_property_t *grp = dropping_group) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            if (obs_property_t *pp = obs_properties_get(inner, "content_mode"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
        }
    }
    // Also attach to stable group inner toggles
    if (obs_property_t *grp = stable_group) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            if (obs_property_t *pp = obs_properties_get(inner, "stable_enabled"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_mode"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
        }
    }
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
