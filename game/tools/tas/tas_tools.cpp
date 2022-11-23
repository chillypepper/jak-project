#include "tas_tools.h"

#include <fstream>
#include <sstream>

#include "common/log/log.h"
#include "common/symbols.h"
#include "common/util/FileUtil.h"

#include "game/graphics/gfx.h"
#include "game/kernel/common/Ptr.h"

#include "third-party/json.hpp"

// Keep references to the structs shared with GOAL
u64 tas_input_frame_goal_ptr = 0;
u64 tas_input_frame_results_goal_ptr = 0;

// Track our inputs for both playbacks and recordings
std::vector<TAS::TASInput> tas_inputs;
std::vector<TAS::TASInput> tas_recording_inputs;
std::vector<TAS::TASInputFrameResultsGOAL> tas_results;

// Track our true frame number as well as what input index we're in
u64 tas_input_frame = 0;
u64 tas_input_index = 0;

// This is true if the last input was at least 1 frame long, we'll need to create a new TASInput
bool last_input_had_frame_data = false;

namespace TAS {
// Fill the given pointers with data from the current tas frame
TASInputFrameGOAL tas_read_current_frame() {
  if (tas_input_frame > 0 && tas_input_index < tas_inputs.size()) {
    auto input = tas_inputs[tas_input_index];

    return {.tas_frame = tas_input_frame,
            .frame_rate = input.frame_rate,
            .skip_spool_movies = input.skip_spool_movies,
            .button0 = input.button0,
            .player_angle = input.player_angle,
            .player_speed = input.player_speed,
            .camera_zoom = input.camera_zoom,
            .camera_angle = input.camera_angle};
  }

  // If we're not running a TAS (or just finished it) reset to empty
  return {.tas_frame = 0,
          .frame_rate = 60,
          .skip_spool_movies = 0,
          .button0 = 0,
          .player_angle = 0,
          .player_speed = 0,
          .camera_zoom = 0,
          .camera_angle = 0};
};

// Update the shared pointer with the inputs for the current frame
void tas_update_goal_input_frame() {
  *Ptr<TASInputFrameGOAL>(tas_input_frame_goal_ptr).c() = tas_read_current_frame();

  // If TAS is enabled we take over the controller, until it's finished or cancelled
  if (tas_input_frame > 0 && tas_input_index < tas_inputs.size()) {
    auto input = tas_inputs[tas_input_index];

    // Handle commands on the first frame of the input
    if (tas_input_frame == input.first_frame) {
      if (Gfx::get_frame_rate() != input.frame_rate) {
        Gfx::set_frame_rate(input.frame_rate);
      }
    }
  }
}

void tas_init() {
  // Copied and changed behaviour from jak1::make_string_from_c
  // NOTE There's no check for failed allocation here!
  if (tas_input_frame_goal_ptr == 0) {
    tas_input_frame_goal_ptr = jak1::alloc_heap_object(
        (s7 + jak1_symbols::FIX_SYM_GLOBAL_HEAP).offset, *(s7 + jak1_symbols::FIX_SYM_STRING_TYPE),
        sizeof(TASInputFrameGOAL) + BASIC_OFFSET + 4, UNKNOWN_PP);
  }

  if (tas_input_frame_results_goal_ptr == 0) {
    tas_input_frame_results_goal_ptr = jak1::alloc_heap_object(
        (s7 + jak1_symbols::FIX_SYM_GLOBAL_HEAP).offset, *(s7 + jak1_symbols::FIX_SYM_STRING_TYPE),
        sizeof(TASInputFrameResultsGOAL) + BASIC_OFFSET + 4, UNKNOWN_PP);
  }

  jak1::intern_from_c("*pc-tas-input-frame*")->value = tas_input_frame_goal_ptr;
  jak1::intern_from_c("*pc-tas-input-frame-results*")->value = tas_input_frame_results_goal_ptr;

  tas_update_goal_input_frame();

  *Ptr<TASInputFrameResultsGOAL>(tas_input_frame_results_goal_ptr).c() = {.tas_frame = 0,
                                                                          .fuel_cell_total = 0,
                                                                          .money_total = 0,
                                                                          .buzzer_total = 0,
                                                                          .actual_player_angle = 0};
}

void tas_end_inputs() {
  nlohmann::json json;
  json["key-frames"] = nlohmann::json::array();

  // TODO Might also be good to log the start and ends of files?
  for (auto result : tas_results) {
    json["key-frames"].push_back({{"tas-frame", result.tas_frame},
                                  {"fuel-cell-total", result.fuel_cell_total},
                                  {"money-total", result.money_total},
                                  {"buzzer-total", result.buzzer_total},
                                  {"money-total", result.money_total}});
  }

  // TODO Some error checking/logging
  file_util::write_text_file(
      tas_folder_path + std::to_string(std::time(nullptr)) + tas_results_file_extension,
      json.dump(2));

  tas_input_frame = 0;
  tas_input_index = 0;
  tas_results.clear();
  tas_inputs.clear();
  lg::debug("[TAS Playback] TAS complete.");
}

void tas_update_frame_results() {
  if (tas_input_frame > 0 && tas_input_index < tas_inputs.size()) {
    // lg::debug("[TAS Playback] Finished frame: " + std::to_string(tas_input_frame));

    TASInputFrameResultsGOAL results =
        *Ptr<TASInputFrameResultsGOAL>(tas_input_frame_results_goal_ptr).c();

    if (tas_results.size()) {
      size_t last_index = tas_results.size() - 1;

      if (results.fuel_cell_total > tas_results[last_index].fuel_cell_total ||
          results.money_total > tas_results[last_index].money_total ||
          results.buzzer_total > tas_results[last_index].buzzer_total) {
        lg::debug("[TAS Playback] Frame results: " +
                  Ptr<TASInputFrameResultsGOAL>(tas_input_frame_results_goal_ptr).c()->toString());

        tas_results.push_back(results);
      }
    } else {
      // Store the first frame every time
      tas_results.push_back(results);
    }

    ++tas_input_frame;

    if (tas_input_frame > tas_inputs[tas_input_index].last_frame) {
      ++tas_input_index;

      if (tas_input_index >= tas_inputs.size()) {
        // Store the final frame if we haven't already
        if (results.tas_frame != tas_results[tas_results.size() - 1].tas_frame) {
          tas_results.push_back(results);
        }

        tas_end_inputs();
      }
    }
  }
}

const TASKeyValue tas_read_line_key_value(std::string line, std::vector<std::string> types) {
  const size_t line_size = line.size();

  for (auto type : types) {
    const std::string prefix = type + "=";
    const size_t prefix_size = prefix.size();

    if (line_size > prefix_size && line._Starts_with(prefix)) {
      return {.key = type, .value = line.substr(prefix_size)};
    }
  }

  return {.key = "", .value = ""};
}

s8 tas_get_input_bool(std::string value) {
  if (value == "true" || value == "false") {
    return value == "true" ? 1 : 0;
  }

  // NOTE Make sure to check for -1 here!
  return -1;
}

void tas_add_new_input_if_needed(std::string file_name, u64 file_line) {
  // If the last input was a command we can edit the last input rather than making a new one
  if (last_input_had_frame_data) {
    size_t last_input_index = tas_inputs.size() - 1;
    last_input_had_frame_data = false;
    tas_inputs.push_back({.first_frame = tas_inputs[last_input_index].last_frame + 1,
                          .last_frame = tas_inputs[last_input_index].last_frame + 1,
                          .file_name = file_name,
                          .file_line = file_line,
                          .frame_rate = tas_inputs[last_input_index].frame_rate,
                          .skip_spool_movies = tas_inputs[last_input_index].skip_spool_movies,
                          .button0 = 0,
                          .player_angle = tas_inputs[last_input_index].player_angle,
                          .player_speed = tas_inputs[last_input_index].player_speed,
                          .camera_angle = tas_inputs[last_input_index].camera_angle,
                          .camera_zoom = tas_inputs[last_input_index].camera_zoom});
  }
}

// TODO This shouldn't be adding to a global var, should just import and return
void tas_load_inputs(std::string file_name) {
  // First load the file contents, we'll be reading line by line
  const std::string full_file_name = tas_folder_path +
                                     (file_name.size() == 0 ? tas_main_file_name : file_name) +
                                     tas_file_extension;
  std::ifstream inputs_file(full_file_name);

  // Log an error if we can't read this file and return early
  if (!inputs_file.is_open()) {
    return lg::error("[TAS Input] Failed to open inputs. " +
                     (file_name == tas_main_file_name
                          ? "Make sure to create a " + tas_main_file_name + tas_file_extension +
                                " file (full path \"<PROJECT_PATH>/" + file_name +
                                "\") to get started! You can just copy one of the existing " +
                                tas_file_extension + " files to test it."
                          : "File " + file_name + " not found - looking for \"<PROJECT_PATH>/" +
                                file_name + "\" but found nothing."));
  }

  std::string raw_line;
  u64 file_line = 0;

  // Read each line in the file
  while (std::getline(inputs_file, raw_line)) {
    std::string line = raw_line;
    ++file_line;

    // lg::debug("[TAS Input] Reading File: " + file_name + " - Line: " + std::to_string(file_line)
    // + " - " + raw_line);

    // Remove all whitespace from the line
    line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

    // Ignore comments and empty lines
    if (line.size() == 0 || line._Starts_with("#")) {
      continue;
    }

    // Handle all of the special commands
    TASKeyValue pair = tas_read_line_key_value(line, {"import", "frame-rate", "skip-spool-movies"});

    if (pair.key != "") {
      // Import another tas file, and just read it in like it was originally part of this file
      if (pair.key == "import") {
        // TODO Some protection against cyclical imports maybe?
        // TODO Some protection against file walking
        tas_load_inputs(pair.value);
      } else if (pair.key == "frame-rate") {
        tas_add_new_input_if_needed(file_name, file_line);
        // TODO Check this differently, throwing an exception will crash even if it's caught
        tas_inputs[tas_inputs.size() - 1].frame_rate = std::stoul(pair.value);
      } else if (pair.key == "skip-spool-movies") {
        tas_add_new_input_if_needed(file_name, file_line);
        tas_inputs[tas_inputs.size() - 1].skip_spool_movies = tas_get_input_bool(pair.value);
      }

      continue;
    }

    // If we're here the only option left to test for is an input frame
    std::stringstream line_stream(line);
    std::string field;
    uint16_t field_index = 0;

    // Break the line apart by the separator
    while (std::getline(line_stream, field, ',')) {
      if (field_index == 0) {
        // TODO Check this differently, throwing an exception will crash even if it's caught
        u64 frame_count = std::stoul(field);

        // 0 frames are valid lines, but we don't do anything with them
        if (frame_count == 0) {
          break;
        }

        // TODO Handle cases where the start isn't a valid frame count, stoul above will just
        // crash
        if (frame_count <= 0) {
          lg::warn("[TAS Input] Skipping unhandled line, it might be a comment: " + raw_line);

          break;
        }

        tas_add_new_input_if_needed(file_name, file_line);
        tas_inputs[tas_inputs.size() - 1].last_frame += frame_count - 1;
      } else {
        bool matched_button = false;

        // Match buttons by their names
        for (auto button : gamepad_map) {
          if (button.first == field) {
            tas_inputs[tas_inputs.size() - 1].button0 +=
                std::pow(2, static_cast<int>(button.second));
            matched_button = true;
            break;
          }
        }

        // Look for other input options if we didn't match one of the buttons
        if (!matched_button) {
          TASKeyValue pair = tas_read_line_key_value(
              field, {"player-angle", "player-speed", "camera-angle", "camera-zoom"});

          // Get the target directions
          // TODO Check this differently, throwing an exception will crash even if it's caught
          if (pair.key == "player-angle") {
            tas_inputs[tas_inputs.size() - 1].player_angle = std::stoul(pair.value);
          } else if (pair.key == "player-speed") {
            tas_inputs[tas_inputs.size() - 1].player_speed = std::stoul(pair.value);
          } else if (pair.key == "camera-angle") {
            tas_inputs[tas_inputs.size() - 1].camera_angle = std::stoul(pair.value);
          } else if (pair.key == "camera-zoom") {
            tas_inputs[tas_inputs.size() - 1].camera_zoom = std::stoul(pair.value);
          } else {
            // If we reach here we passed something in the valid commands list but didn't handle
            // it, the only case this should happen is if we added a new field and forgot to
            // implement
            lg::warn("[TAS Input] Skipping unhandled field: " + field);
          }
        }
      }

      last_input_had_frame_data = true;
      ++field_index;
    }
  }

  inputs_file.close();
}

void tas_handle_pad_inputs(CPadInfo* cpad) {
  // Use L2 to enable recording inputs. This isn't a toggle to avoid multi frame button
  // presses
  if (tas_input_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::L2)) &&
      tas_recording_inputs.size() == 0) {
    lg::debug("[TAS Recording] Starting recording ...");

    // FrameInputs input;

    // input.first_frame = 0;
    // input.last_frame = 0;

    // tas_recording_inputs.clear();
    // tas_recording_inputs.push_back(input);
  }

  // Use R2 to disable recording inputs. This isn't a toggle to avoid multi frame button
  // presses
  if (tas_input_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::R2)) &&
      tas_recording_inputs.size() != 0) {
    lg::debug("[TAS Recording] Ending recording");

    std::ofstream frame_output_file;
    u64 index = 0;
    frame_output_file.open(tas_folder_path + std::to_string(std::time(nullptr)) +
                           tas_recording_file_extension);

    // TODO Move all of the code for reading these inputs into GOAL and load them directly in
    // movement, no more cpad faking
    for (auto input : tas_recording_inputs) {
      if (index != 0) {
        std::string controls =
            std::string("") +
            (index == 1 || tas_recording_inputs[index - 1].player_angle != input.player_angle
                 ? ",player-angle=" + std::to_string(input.player_angle)
                 : "") +
            (index == 1 || tas_recording_inputs[index - 1].player_speed != input.player_speed
                 ? ",player-speed=" + std::to_string(input.player_speed)
                 : "") +
            (index == 1 || tas_recording_inputs[index - 1].camera_angle != input.camera_angle
                 ? ",camera-angle=" + std::to_string(input.camera_angle)
                 : "") +
            (index == 1 || tas_recording_inputs[index - 1].camera_zoom != input.camera_zoom
                 ? ",camera-zoom=" + std::to_string(input.camera_zoom)
                 : "");

        for (auto button : gamepad_map) {
          if (input.button0 & (u16)std::pow(2, static_cast<int>(button.second))) {
            controls += "," + button.first;
          }
        }

        // frame_output_file << std::to_string((input.last_frame - input.first_frame) + 1) +
        // controls << std::endl;
      }

      ++index;
    }

    frame_output_file.close();

    tas_recording_inputs.clear();
  }

  if (tas_recording_inputs.size() != 0) {
    // size_t lastIndex = tas_recording_inputs.size() - 1;

    // if (tas_recording_inputs[lastIndex].button0 == cpad->button0 &&
    //     tas_recording_inputs[lastIndex].leftx == cpad->leftx &&
    //     tas_recording_inputs[lastIndex].lefty == cpad->lefty &&
    //     tas_recording_inputs[lastIndex].rightx == cpad->rightx &&
    //     tas_recording_inputs[lastIndex].righty == cpad->righty) {
    //   ++tas_recording_inputs[lastIndex].last_frame;
    // } else {
    //   FrameInputs input;

    //   input.first_frame = tas_recording_inputs[lastIndex].last_frame + 1;
    //   input.last_frame = tas_recording_inputs[lastIndex].last_frame + 1;
    //   input.button0 = cpad->button0;
    //   input.leftx = cpad->leftx;
    //   input.lefty = cpad->lefty;
    //   input.rightx = cpad->rightx;
    //   input.righty = cpad->righty;

    //   tas_recording_inputs.push_back(input);
    // }
  }

  // Use L3 to enable the TAS. This isn't a toggle to avoid multi frame button presses
  if (tas_input_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::L3)) &&
      tas_recording_inputs.size() == 0) {
    lg::debug("[TAS Playback] Starting TAS ...");

    // Set up first input with default values
    tas_results.clear();
    tas_inputs.clear();
    tas_inputs.push_back({.first_frame = 1,
                          .last_frame = 1,
                          .file_name = tas_main_file_name,
                          .file_line = 1,
                          .frame_rate = 60,
                          .skip_spool_movies = 0,
                          .button0 = 0,
                          .player_angle = 0,
                          .player_speed = 0,
                          .camera_angle = 0,
                          .camera_zoom = 0});
    last_input_had_frame_data = false;
    tas_input_frame = 1;
    tas_input_index = 0;
    tas_load_inputs(tas_main_file_name);
    tas_update_goal_input_frame();

    // std::string inputs_display;
    // for (auto& input : tas_inputs)
    //   inputs_display += input.toString() + ", ";
    // lg::debug("TAS INPUTS: " + inputs_display);

    if (tas_inputs.size() == 0) {
      lg::debug("[TAS Playback] No valid inputs, start cancelled.");
      tas_end_inputs();
    } else {
      lg::debug("[TAS Playback] Inputs loaded!");
    }
  }

  // Use Triangle to disable the TAS. Automatically disabled if we reach the end. This isn't a
  // toggle to avoid multi frame button presses
  if ((cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::Triangle))) &&
      tas_input_frame > 0) {
    lg::debug("[TAS Playback] Ending TAS ...");
    tas_end_inputs();
  }
}
}  // namespace TAS
