// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <string>
#include <memory>

// Smart wrapper for obs_source_t (calls obs_source_release automatically)
struct SourceReleaser {
    void operator()(obs_source_t *p) const noexcept { if (p) obs_source_release(p); }
};
using SourceHandle = std::unique_ptr<obs_source_t, SourceReleaser>;

// All runtime data is kept in this struct
struct OnlineStatus {
    // Dropping children
    SourceHandle status_text;
    SourceHandle status_image;

    // Stable-state children
    SourceHandle status_text_stable;
    SourceHandle status_image_stable;

    // Dropping content
    std::string text;
    std::string image_path;

    // Stable content
    std::string stable_text_msg;
    std::string stable_image_path;

    // Visibility/state
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

// OBS source callbacks
const char *online_status_get_name(void *);
void *online_status_create(obs_data_t *settings, obs_source_t *owner);
void online_status_destroy(void *data);
uint32_t online_status_get_width(void *data);
uint32_t online_status_get_height(void *data);
void online_status_defaults(obs_data_t *settings);
obs_properties_t *online_status_properties(void *data);
void online_status_update(void *data, obs_data_t *settings);
void online_status_video_tick(void *data, float seconds);
void online_status_video_render(void *data, gs_effect_t *effect);

// Registration
void register_online_status_source(void);
