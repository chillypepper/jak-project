#pragma once

#include <string>

#include "common/common_types.h"

#include "game/kernel/common/kernel_types.h"
#include "game/kernel/jak1/kscheme.h"
#include "game/system/newpad.h"

#include "third-party/json.hpp"

namespace TAS {
struct TASKeyValue {
  std::string key;
  std::string value;
};

struct TASGoalVector {
  float x;
  float y;
  float z;
  float w;

  nlohmann::json toJSON() {
    return nlohmann::json::object({{"x", x}, {"y", y}, {"z", z}, {"w", w}});
  }
};

struct TASInput {
  // Metadata for this line
  u64 first_frame;
  u64 last_frame;
  std::string file_name;
  u64 file_line;

  // Command settings, should be set at the start of this line
  u32 frame_rate;
  std::string marker;
  u32 skip_spool_movies;

  // Inputs for this line
  u32 button0;
  float player_angle;
  float player_speed;
  float camera_angle;
  float camera_zoom;
};

// Matches tas-input-frame in tas.gc
struct TASInputFrameGOAL {
  u64 tas_frame;
  u32 frame_rate;
  u32 skip_spool_movies;
  u32 is_recording_input;
  u32 button0;
  float player_angle;
  float player_speed;
  float camera_angle;
  float camera_zoom;
};

// Matches tas-input-frame-results in tas.gc
struct TASInputFrameResultsGOAL {
  u64 tas_frame;
  float fuel_cell_total;
  float money_total;
  float buzzer_total;
  u32 input_button0;
  float input_player_angle;
  float input_player_speed;
  float input_camera_angle;
  float input_camera_zoom;
  float pad1;
  float pad2;
  TASGoalVector player_position;
  TASGoalVector camera_position;

  nlohmann::json toJSON() {
    return nlohmann::json::object({
        {"tas-frame", tas_frame},
        {"fuel-cell-total", fuel_cell_total},
        {"money-total", money_total},
        {"buzzer-total", buzzer_total},
        {"input-button0", input_button0},
        {"input-player-angle", input_player_angle},
        {"input-player-speed", input_player_speed},
        {"input-camera-angle", input_camera_angle},
        {"input-camera-zoom", player_position.toJSON()},
        {"input-camera-zoom", camera_position.toJSON()},
    });
  }
};

// Strings for tas folders and files
const std::string tas_folder_path = "tas/jak1/";
const std::string tas_main_file_name = "main";
const std::string tas_file_extension = ".jaktas";
const std::string tas_recording_file_extension = ".recording" + tas_file_extension;
const std::string tas_results_file_extension = ".automated.results.json";

const std::pair<std::string, Pad::Button> gamepad_map[] = {{"Select", Pad::Button::Select},
                                                           {"L3", Pad::Button::L3},
                                                           {"R3", Pad::Button::R3},
                                                           {"Start", Pad::Button::Start},
                                                           {"Up", Pad::Button::Up},
                                                           {"Right", Pad::Button::Right},
                                                           {"Down", Pad::Button::Down},
                                                           {"Left", Pad::Button::Left},
                                                           {"L2", Pad::Button::L2},
                                                           {"R2", Pad::Button::R2},
                                                           {"L1", Pad::Button::L1},
                                                           {"R1", Pad::Button::R1},
                                                           {"Triangle", Pad::Button::Triangle},
                                                           {"Circle", Pad::Button::Circle},
                                                           {"X", Pad::Button::X},
                                                           {"Square", Pad::Button::Square}};

// Functions for managing the TAS
void tas_init();
TASInputFrameGOAL tas_read_current_frame();
void tas_update_goal_input_frame();
void tas_handle_pad_inputs(CPadInfo* cpad);
void tas_update_frame_results();
}  // namespace TAS
