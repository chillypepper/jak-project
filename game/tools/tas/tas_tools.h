#pragma once

#include <string>

#include "common/common_types.h"

#include "game/kernel/common/kernel_types.h"
#include "game/kernel/jak1/kscheme.h"
#include "game/system/newpad.h"

namespace TAS {
struct TASKeyValue {
  std::string key;
  std::string value;
};

struct TASInput {
  // Metadata for this line
  u64 first_frame;
  u64 last_frame;
  std::string file_name;
  u64 file_line;

  // Command settings, should be set at the start of this line
  u16 frame_rate;
  u16 skip_spool_movies;

  // Inputs for this line
  u16 button0;
  u16 player_angle;
  float player_speed;
  u16 camera_angle;
  float camera_zoom;

  // Quick debug output just to make sure things look right
  // TODO For some reason print braces causes a crash, so I use @^ = { and ,@ = } to get valid JSON
  std::string toString() {
    return "@^" + std::string("\"first_frame\": \"") + std::to_string(first_frame) + "\"," +
           std::string("\"last_frame\": \"") + std::to_string(last_frame) + "\"," +
           std::string("\"file_name\": \"") + std::string(file_name) + "\"," +
           std::string("\"file_line\": \"") + std::to_string(file_line) + "\"," +
           std::string("\"frame_rate\": \"") + std::to_string(frame_rate) + "\"," +
           std::string("\"skip_spool_movies\": \"") + std::to_string(skip_spool_movies) + "\"," +
           std::string("\"button0\": \"") + std::to_string(button0) + "\"," +
           std::string("\"player_angle\": \"") + std::to_string(player_angle) + "\"," +
           std::string("\"player_speed\": \"") + std::to_string(player_speed) + "\"," +
           std::string("\"camera_angle\": \"") + std::to_string(camera_angle) + "\"," +
           std::string("\"camera_zoom\": \"") + std::to_string(camera_zoom) + "\"," + "@";
  }
};

// Matches tas-input-frame in tas.gc
struct TASInputFrameGOAL {
  u64 tas_frame;
  u16 frame_rate;
  u16 skip_spool_movies;
  u16 button0;
  u16 player_angle;
  float player_speed;
  float camera_zoom;
  u16 camera_angle;
};

// Strings for tas folders and files
const std::string tas_folder_path = "tas/jak1/";
const std::string tas_main_file_name = "main";
const std::string tas_file_extension = ".jaktas";
const std::string tas_recording_file_extension = ".recording" + tas_file_extension;
const std::string tas_results_file_extension = ".results" + tas_file_extension;

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
void tas_update_goal_input_frame();
void tas_handle_pad_inputs(CPadInfo* cpad);
void tas_update_frame_results();
}  // namespace TAS
