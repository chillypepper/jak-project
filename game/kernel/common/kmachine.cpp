#include "kmachine.h"

#include <numeric>
#include <sstream>
#include <string>

#include "common/global_profiler/GlobalProfiler.h"
#include "common/log/log.h"
#include "common/symbols.h"
#include "common/util/FileUtil.h"
#include "common/util/Timer.h"

#include "game/graphics/gfx.h"
#include "game/kernel/common/Ptr.h"
#include "game/kernel/common/kernel_types.h"
#include "game/kernel/common/kprint.h"
#include "game/kernel/common/kscheme.h"
#include "game/mips2c/mips2c_table.h"
#include "game/sce/libcdvd_ee.h"
#include "game/sce/libpad.h"
#include "game/sce/libscf.h"
#include "game/sce/sif_ee.h"

/*!
 * Where does OVERLORD load its data from?
 */
OverlordDataSource isodrv;

// Get IOP modules from DVD or from dsefilesv
u32 modsrc;

// Reboot IOP with IOP kernel from DVD/CD on boot
u32 reboot;

const char* init_types[] = {"fakeiso", "deviso", "iso_cd"};
u8 pad_dma_buf[2 * SCE_PAD_DMA_BUFFER_SIZE];

// added
u32 vif1_interrupt_handler = 0;
u32 vblank_interrupt_handler = 0;

Timer ee_clock_timer;

const std::string frame_inputs_folder_path = "tas/jak1/";
const std::string frame_inputs_main_file_name = "main";
const std::string frame_inputs_file_extension = ".jaktas";
const std::string frame_inputs_output_file_extension = ".output" + frame_inputs_file_extension;
u64 tas_frame;
u64 tas_inputs_line;
u64 tas_commands_line;
u64 skip_spool_movies;

// I am also Not crazy about this declaration
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

struct InputValue {
  std::string key;
  std::string value;
};

struct FrameInputs {
  uint64_t start_frame;
  uint64_t end_frame;
  u16 button0;
  u8 leftx;
  u8 lefty;
  u8 rightx;
  u8 righty;

  // Quick debug output just to make sure things look right
  std::string toString() {
    return "@" + std::string("\"start_frame\": \"") + std::to_string(start_frame) + "\"," +
           std::string("\"end_frame\": \"") + std::to_string(end_frame) + "\"," +
           std::string("\"button0\": \"") + std::to_string(button0) + "\"," +
           std::string("\"leftx\": \"") + std::to_string(leftx) + "\"," +
           std::string("\"lefty\": \"") + std::to_string(lefty) + "\"," +
           std::string("\"rightx\": \"") + std::to_string(rightx) + "\"," +
           std::string("\"righty\": \"") + std::to_string(righty) + "\"," + "@";
  }
};

struct FrameCommands {
  u64 frame_index;
  u64 frame_rate;
  u8 skip_spool_movies;

  // Quick debug output just to make sure things look right
  std::string toString() {
    return "@" + std::string("\"frame_index\": \"") + std::to_string(frame_index) + "\"," +
           std::string("\"frame_rate\": \"") + std::to_string(frame_rate) + "\"," +
           std::string("\"skip_spool_movies\": \"") + std::to_string(skip_spool_movies) + "\"," +
           "@";
  }
};

std::vector<FrameInputs> frame_inputs;
std::vector<FrameCommands> frame_commands;
std::vector<FrameInputs> input_recordings;

InputValue get_input_value_from_field(std::string value, std::vector<std::string> types) {
  size_t value_size = value.size();

  for (auto type : types) {
    std::string prefix = type + "=";
    size_t prefix_size = prefix.size();

    if (value_size > prefix_size && value._Starts_with(prefix)) {
      return {.key = type, .value = value.substr(prefix_size)};
    }
  }

  throw std::runtime_error("Invalid field");
}

u8 get_input_bool(std::string value) {
  if (value == "true" || value == "false") {
    return value == "true" ? 1 : 0;
  }

  throw std::runtime_error("Invalid bool");
}

// TODO This shouldn't be adding to a global var, should just import and return
void load_tas_inputs(std::string file_name = "") {
  if (file_name.size() == 0) {
    frame_inputs.clear();
    frame_commands.clear();
  }

  std::ifstream frame_input_file;
  frame_input_file.clear();
  frame_input_file.open(frame_inputs_folder_path +
                        (file_name.size() == 0 ? frame_inputs_main_file_name : file_name) +
                        frame_inputs_file_extension);

  // First load the file contents, we'll be reading line by line
  if (frame_input_file.is_open()) {
    std::string raw_line;

    // Read each line in the file
    while (std::getline(frame_input_file, raw_line)) {
      std::string line = raw_line;

      // Remove all whitespace from the line
      line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());

      // Ignore comments and empty lines
      if (line.size() == 0 || line._Starts_with("#")) {
        continue;
      }

      // Handle all of the special commands
      try {
        InputValue pair =
            get_input_value_from_field(line, {"import", "frame-rate", "skip-spool-movies"});

        // Import another tas file, and just read it in like it was originally part of this file
        if (pair.key == "import") {
          // TODO Some protection against cyclical imports maybe?
          // TODO Some protection against file walking
          load_tas_inputs(pair.value);

          continue;
        }

        // Set defaults for the first index, and carry commands for the rest
        if (frame_commands.size() == 0) {
          frame_commands.push_back({.frame_index = 1, .frame_rate = 60, .skip_spool_movies = 0});
        } else {
          // Use the latest frame from the inputs for the command
          size_t inputs_size = frame_inputs.size();
          u64 frame_index = inputs_size == 0 ? 1 : frame_inputs[inputs_size - 1].end_frame + 1;
          size_t last_index = frame_commands.size() - 1;

          // Multiple commands in a row will have the same frame so don't create another
          if (frame_index != frame_commands[last_index].frame_index) {
            FrameCommands command;
            memcpy(&command, &frame_commands[last_index], sizeof(frame_commands[last_index]));
            command.frame_index = frame_index;
            frame_commands.push_back(command);
          }
        }

        if (pair.key == "frame-rate") {
          frame_commands[frame_commands.size() - 1].frame_rate = std::stoul(pair.value);
        } else if (pair.key == "skip-spool-movies") {
          frame_commands[frame_commands.size() - 1].skip_spool_movies = get_input_bool(pair.value);
        } else {
          // If we reach here we passed something in the valid commands list but didn't handle it,
          // the only real case this could happen is if we added a new field and forgot to implement
          lg::warn("[TAS Input Error] Valid command not implemented: " + raw_line);
        }

        continue;
      } catch (...) {
        // Intentionally left empty. We throw line errors down in the input frame number check
      }

      // If we're here the only option left to test for is an input frame
      std::stringstream line_stream(line);
      std::string value;
      uint16_t index = 0;

      // Break the line apart by the separator
      while (std::getline(line_stream, value, ',')) {
        if (index == 0) {
          try {
            u64 frame_count = std::stoul(value);

            // 0 frames are valid lines, but we don't do anything with them
            if (frame_count == 0) {
              break;
            }

            // Set defaults for the first index, and carry only directions for the rest
            if (frame_inputs.size() == 0) {
              frame_inputs.push_back({.start_frame = 1,
                                      .end_frame = frame_count,
                                      .button0 = 0,
                                      .leftx = 128,
                                      .lefty = 128,
                                      .rightx = 128,
                                      .righty = 128});
            } else {
              size_t last_index = frame_inputs.size() - 1;
              frame_inputs.push_back({.start_frame = frame_inputs[last_index].end_frame + 1,
                                      .end_frame = frame_inputs[last_index].end_frame + frame_count,
                                      .button0 = 0,
                                      .leftx = frame_inputs[last_index].leftx,
                                      .lefty = frame_inputs[last_index].lefty,
                                      .rightx = frame_inputs[last_index].rightx,
                                      .righty = frame_inputs[last_index].righty});
            }
          } catch (...) {
            lg::warn("[TAS Input Error] Ignoring invalid line, it might be a comment: " + raw_line);

            break;
          }
        } else {
          // Match buttons by their names
          for (auto button : gamepad_map) {
            if (button.first == value) {
              frame_inputs[frame_inputs.size() - 1].button0 +=
                  std::pow(2, static_cast<int>(button.second));
              break;
            }
          }

          try {
            InputValue pair =
                get_input_value_from_field(value, {"leftx", "lefty", "rightx", "righty"});

            // Get the stick positions
            if (pair.key == "leftx") {
              frame_inputs[frame_inputs.size() - 1].leftx = std::stoul(pair.value);
            } else if (pair.key == "lefty") {
              frame_inputs[frame_inputs.size() - 1].lefty = std::stoul(pair.value);
            } else if (pair.key == "rightx") {
              frame_inputs[frame_inputs.size() - 1].rightx = std::stoul(pair.value);
            } else if (pair.key == "righty") {
              frame_inputs[frame_inputs.size() - 1].righty = std::stoul(pair.value);
            }
          } catch (...) {
            lg::warn("[TAS Input Error] Ignoring invalid input: " + value);
          }
        }

        index += 1;
      }
    }

    // std::string frame_commands_display;
    // for (auto& piece : frame_commands)
    //   frame_commands_display += piece.toString() + ", ";
    // lg::debug("TAS COMMANDS: " + frame_commands_display);

    // std::string frame_inputs_display;
    // for (auto& piece : frame_inputs)
    //   frame_inputs_display += piece.toString() + ", ";
    // lg::debug("TAS INPUTS: " + frame_inputs_display);

    frame_input_file.close();
  } else {
    lg::error("[TAS Input Error] Failed to open inputs. " +
              (file_name.size() == 0
                   ? "Make sure to create a " + frame_inputs_main_file_name +
                         frame_inputs_file_extension + " file in \"jak-project/" +
                         frame_inputs_folder_path +
                         "\" to get started! You can just copy one of the existing " +
                         frame_inputs_file_extension + " files to test it."
                   : "File " + file_name + frame_inputs_file_extension + " not found in " +
                         frame_inputs_folder_path));
  }
}

void handleTASPad(CPadInfo* cpad) {
  // Use L2 to enable recording inputs. This isn't a toggle to avoid multi frame button
  // presses
  if (tas_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::L2)) &&
      input_recordings.size() == 0) {
    lg::debug("Starting recording ...");

    FrameInputs input;

    input.start_frame = 0;
    input.end_frame = 0;

    input_recordings.clear();
    input_recordings.push_back(input);
  }

  // Use R2 to disable recording inputs. This isn't a toggle to avoid multi frame button
  // presses
  if (tas_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::R2)) &&
      input_recordings.size() != 0) {
    lg::debug("Ending recording");

    std::ofstream frame_output_file;
    u64 index = 0;
    frame_output_file.open(frame_inputs_folder_path + std::to_string(std::time(nullptr)) +
                           frame_inputs_output_file_extension);

    for (auto input : input_recordings) {
      if (index != 0) {
        std::string controls = std::string("") +
                               (index == 1 || input_recordings[index - 1].leftx != input.leftx
                                    ? ",leftx=" + std::to_string(input.leftx)
                                    : "") +
                               (index == 1 || input_recordings[index - 1].lefty != input.lefty
                                    ? ",lefty=" + std::to_string(input.lefty)
                                    : "") +
                               (index == 1 || input_recordings[index - 1].rightx != input.rightx
                                    ? ",rightx=" + std::to_string(input.rightx)
                                    : "") +
                               (index == 1 || input_recordings[index - 1].righty != input.righty
                                    ? ",righty=" + std::to_string(input.righty)
                                    : "");

        for (auto button : gamepad_map) {
          if (input.button0 & (u16)std::pow(2, static_cast<int>(button.second))) {
            controls += "," + button.first;
          }
        }

        frame_output_file << std::to_string((input.end_frame - input.start_frame) + 1) + controls
                          << std::endl;
      }

      ++index;
    }

    frame_output_file.close();

    input_recordings.clear();
  }

  if (input_recordings.size() != 0) {
    size_t lastIndex = input_recordings.size() - 1;

    if (input_recordings[lastIndex].button0 == cpad->button0 &&
        input_recordings[lastIndex].leftx == cpad->leftx &&
        input_recordings[lastIndex].lefty == cpad->lefty &&
        input_recordings[lastIndex].rightx == cpad->rightx &&
        input_recordings[lastIndex].righty == cpad->righty) {
      input_recordings[lastIndex].end_frame += 1;
    } else {
      FrameInputs input;

      input.start_frame = input_recordings[lastIndex].end_frame + 1;
      input.end_frame = input_recordings[lastIndex].end_frame + 1;
      input.button0 = cpad->button0;
      input.leftx = cpad->leftx;
      input.lefty = cpad->lefty;
      input.rightx = cpad->rightx;
      input.righty = cpad->righty;

      input_recordings.push_back(input);
    }
  }

  // Use L3 to enable the TAS. This isn't a toggle to avoid multi frame button presses
  if (tas_frame == 0 && cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::L3)) &&
      input_recordings.size() == 0) {
    lg::debug("Starting TAS ...");
    tas_frame = 1;
    tas_inputs_line = 0;
    tas_commands_line = 0;
    skip_spool_movies = 0;
    load_tas_inputs();
  }

  // Use Triangle to disable the TAS. Automatically disabled if we reach the end. This isn't a
  // toggle to avoid multi frame button presses
  if ((tas_inputs_line >= frame_inputs.size() ||
       cpad->button0 == std::pow(2, static_cast<int>(Pad::Button::Triangle))) &&
      tas_frame > 0) {
    lg::debug("Ending TAS");
    tas_frame = 0;
    tas_inputs_line = 0;
    tas_commands_line = 0;
    skip_spool_movies = 0;
    set_frame_rate(60);
  }

  // If TAS is enabled we take over the controller, until it's finished or cancelled
  if (tas_frame > 0) {
    cpad->button0 = 0;
    cpad->leftx = 128;
    cpad->lefty = 128;
    cpad->rightx = 128;
    cpad->righty = 128;

    if (tas_commands_line < frame_commands.size()) {
      auto command = frame_commands[tas_commands_line];

      if (tas_frame == command.frame_index) {
        if (Gfx::get_frame_rate() != command.frame_rate) {
          set_frame_rate(command.frame_rate);
        }

        skip_spool_movies = command.skip_spool_movies;

        ++tas_commands_line;
      }
    }

    if (tas_inputs_line < frame_inputs.size()) {
      auto input = frame_inputs[tas_inputs_line];

      if (tas_frame <= input.end_frame) {
        cpad->button0 = input.button0;
        cpad->leftx = input.leftx;
        cpad->lefty = input.lefty;
        cpad->rightx = input.rightx;
        cpad->righty = input.righty;

        if (tas_frame == input.end_frame) {
          ++tas_inputs_line;
        }
      }
    }

    // lg::debug("Running frame: " + std::to_string(tas_frame));

    tas_frame += 1;
  }
}

void kmachine_init_globals_common() {
  memset(pad_dma_buf, 0, sizeof(pad_dma_buf));
  isodrv = fakeiso;  // changed. fakeiso is the only one that works in opengoal.
  modsrc = 1;
  reboot = 1;
  vif1_interrupt_handler = 0;
  vblank_interrupt_handler = 0;
  ee_clock_timer = Timer();
  tas_frame = 0;
  tas_inputs_line = 0;
  tas_commands_line = 0;
  skip_spool_movies = 0;
}

/*!
 * Initialize the CD Drive
 * DONE, EXACT
 */
void InitCD() {
  lg::info("Initializing CD drive. This may take a while...");
  ee::sceCdInit(SCECdINIT);
  ee::sceCdMmode(SCECdDVD);
  while (ee::sceCdDiskReady(0) == SCECdNotReady) {
    lg::debug("Drive not ready... insert a disk!");
  }
  lg::debug("Disk type {}\n", ee::sceCdGetDiskType());
}

/*!
 * Initialize the GS and display the splash screen.
 * Not yet implemented. TODO
 */
void InitVideo() {}

/*!
 * Flush caches.  Does all the memory, regardless of what you specify
 */
void CacheFlush(void* mem, int size) {
  (void)mem;
  (void)size;
  // FlushCache(0);
  // FlushCache(2);
}

/*!
 * Open a new controller pad.
 * Set the new_pad flag to 1 and state to 0.
 * Prints an error if it fails to open.
 */
u64 CPadOpen(u64 cpad_info, s32 pad_number) {
  auto cpad = Ptr<CPadInfo>(cpad_info).c();
  if (cpad->cpad_file == 0) {
    // not open, so we will open it
    cpad->cpad_file =
        ee::scePadPortOpen(pad_number, 0, pad_dma_buf + pad_number * SCE_PAD_DMA_BUFFER_SIZE);
    if (cpad->cpad_file < 1) {
      MsgErr("dkernel: !open cpad #%d (%d)\n", pad_number, cpad->cpad_file);
    }
    cpad->new_pad = 1;
    cpad->state = 0;
  }
  return cpad_info;
}

/*!
 * Not checked super carefully for jak 2, but looks the same
 */
u64 CPadGetData(u64 cpad_info) {
  using namespace ee;
  auto cpad = Ptr<CPadInfo>(cpad_info).c();
  auto pad_state = scePadGetState(cpad->number, 0);
  if (pad_state == scePadStateDiscon) {
    cpad->state = 0;
  }
  cpad->valid = pad_state | 0x80;
  switch (cpad->state) {
    // case 99: // functional
    default:  // controller is functioning as normal
      if (pad_state == scePadStateStable || pad_state == scePadStateFindCTP1) {
        scePadRead(cpad->number, 0, (u8*)cpad);
        // ps2 controllers would send an enabled bit if the button was NOT pressed, but we don't do
        // that here. removed code that flipped the bits.

        if (cpad->change_time) {
          scePadSetActDirect(cpad->number, 0, cpad->direct);
        }
        cpad->valid = pad_state;

        // We only check for TAS controls from the first pad
        if (cpad->number == 0) {
          handleTASPad(cpad);
        }
      }
      break;
    case 0:  // unavailable
      if (pad_state == scePadStateStable || pad_state == scePadStateFindCTP1) {
        auto pad_mode = scePadInfoMode(cpad->number, 0, InfoModeCurID, 0);
        if (pad_mode != 0) {
          auto vibration_mode = scePadInfoMode(cpad->number, 0, InfoModeCurExID, 0);
          if (vibration_mode > 0) {
            // vibration supported
            pad_mode = vibration_mode;
          }
          if (pad_mode == 4) {
            // controller mode
            cpad->state = 40;
          } else if (pad_mode == 7) {
            // dualshock mode
            cpad->state = 70;
          } else {
            // who knows mode
            cpad->state = 90;
          }
        }
      }
      break;
    case 40:  // controller mode - check for extra modes
      // cpad->change_time = 0;
      cpad->change_time = 0;
      if (scePadInfoMode(cpad->number, 0, InfoModeIdTable, -1) == 0) {
        // no controller modes
        cpad->state = 90;
        return cpad_info;
      }
      cpad->state = 41;
    case 41:  // controller mode - change to dualshock mode!
      // try to enter the 2nd controller mode (dualshock for ds2's)
      if (scePadSetMainMode(cpad->number, 0, 1, 3) == 1) {
        cpad->state = 42;
      }
      break;
    case 42:  // controller mode change check
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateFailed) {
        // failed to change to DS2
        cpad->state = 41;
      }
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateComplete) {
        // change successful. go back to the beginning.
        cpad->state = 0;
      }
      break;
    case 70:  // dualshock mode - check vibration
      // get number of actuators (2 for DS2)
      if (scePadInfoAct(cpad->number, 0, -1, 0) < 1) {
        // no actuators means no vibration. skip to end!
        // cpad->change_time = 0;
        cpad->change_time = 0;
        cpad->state = 99;
      } else {
        // we have actuators to use.
        // cpad->change_time = 1;  // remember to update pad times.
        cpad->change_time = 1;
        cpad->state = 75;
      }
      break;
    case 75:  // set actuator vib param info
      if (scePadSetActAlign(cpad->number, 0, cpad->align) != 0) {
        if (scePadInfoPressMode(cpad->number, 0) == 1) {
          // pressure buttons supported
          cpad->state = 76;
        } else {
          // no pressure buttons, done with controller setup
          cpad->state = 99;
        }
      }
      break;
    case 76:  // enter pressure mode
      if (scePadEnterPressMode(cpad->number, 0) == 1) {
        cpad->state = 78;
      }
      break;
    case 78:  // pressure mode request check
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateFailed) {
        cpad->state = 76;
      }
      if (scePadGetReqState(cpad->number, 0) == scePadReqStateComplete) {
        cpad->state = 99;
      }
      break;
    case 90:
      break;  // unsupported controller. too bad!
  }
  return cpad_info;
}

// should make sure this works the same way in jak 2
void InstallHandler(u32 handler_idx, u32 handler_func) {
  switch (handler_idx) {
    case 3:
      vblank_interrupt_handler = handler_func;
      break;
    case 5:
      vif1_interrupt_handler = handler_func;
      break;
    default:
      printf("unknown handler: %d\n", handler_idx);
      ASSERT(false);
  }
}

// nothing used this in jak1, hopefully same for 2
void InstallDebugHandler() {
  ASSERT(false);
}

/*!
 * Get length of a file.
 */
s32 klength(u64 fs) {
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    // first flag bit not set. This means no errors
    auto end_seek = ee::sceLseek(file_stream->file, 0, SCE_SEEK_END);
    auto reset_seek = ee::sceLseek(file_stream->file, 0, SCE_SEEK_SET);
    if (reset_seek < 0 || end_seek < 0) {
      // seeking failed, flag it
      file_stream->flags |= 1;
    }
    return end_seek;
  } else {
    return 0;
  }
}

/*!
 * Seek a file stream.
 */
s32 kseek(u64 fs, s32 offset, s32 where) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceLseek(file_stream->file, offset, where);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Read from a file stream.
 */
s32 kread(u64 fs, u64 buffer, s32 size) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceRead(file_stream->file, Ptr<u8>(buffer).c(), size);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Write to a file stream.
 */
s32 kwrite(u64 fs, u64 buffer, s32 size) {
  s32 result = -1;
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    result = ee::sceWrite(file_stream->file, Ptr<u8>(buffer).c(), size);
    if (result < 0) {
      file_stream->flags |= 1;
    }
  }
  return result;
}

/*!
 * Close a file stream.
 */
u64 kclose(u64 fs) {
  auto file_stream = Ptr<FileStream>(fs).c();
  if ((file_stream->flags ^ 1) & 1) {
    ee::sceClose(file_stream->file);
    file_stream->file = -1;
  }
  file_stream->flags = 0;
  return fs;
}

// TODO dma_to_iop
void dma_to_iop() {
  ASSERT(false);
}

u64 DecodeLanguage() {
  return masterConfig.language;
}

u64 DecodeAspect() {
  return masterConfig.aspect;
}

u64 DecodeVolume() {
  return masterConfig.volume;
}

// NOTE: this is originally hardcoded, and returns different values depending on the disc region.
// it returns 0 for NTSC-U, 1 for PAL and 2 for NTSC-J
u64 DecodeTerritory() {
  return GAME_TERRITORY_SCEA;
}

u64 DecodeTimeout() {
  return masterConfig.timeout;
}

u64 DecodeInactiveTimeout() {
  return masterConfig.inactive_timeout;
}

void DecodeTime(u32 ptr) {
  Ptr<ee::sceCdCLOCK> clock(ptr);
  // in jak2, if this fails, they do a sceScfGetLocalTimefromRTC
  sceCdReadClock(clock.c());
}

/*!
 * PC PORT FUNCTIONS BEGIN
 */
/*!
 * Get a 300MHz timer value.
 * Called from EE thread
 */
u64 read_ee_timer() {
  u64 ns = ee_clock_timer.getNs();
  return (ns * 3) / 10;
}

/*!
 * Do a fast memory copy.
 */
void c_memmove(u32 dst, u32 src, u32 size) {
  memmove(Ptr<u8>(dst).c(), Ptr<u8>(src).c(), size);
}

/*!
 * Returns size of window. Called from game thread
 */
void get_window_size(u32 w_ptr, u32 h_ptr) {
  if (w_ptr) {
    auto w = Ptr<u32>(w_ptr).c();
    *w = Gfx::get_window_width();
  }
  if (h_ptr) {
    auto h = Ptr<u32>(h_ptr).c();
    *h = Gfx::get_window_height();
  }
}

/*!
 * Returns scale of window. This is for DPI stuff.
 */
void get_window_scale(u32 x_ptr, u32 y_ptr) {
  float* x = x_ptr ? Ptr<float>(x_ptr).c() : NULL;
  float* y = y_ptr ? Ptr<float>(y_ptr).c() : NULL;
  Gfx::get_window_scale(x, y);
}

/*!
 * Returns resolution of the monitor.
 */
void get_screen_size(s64 vmode_idx, u32 w_ptr, u32 h_ptr) {
  s32 *w_out = 0, *h_out = 0;
  if (w_ptr) {
    w_out = Ptr<s32>(w_ptr).c();
  }
  if (h_ptr) {
    h_out = Ptr<s32>(h_ptr).c();
  }
  Gfx::get_screen_size(vmode_idx, w_out, h_out);
}

/*!
 * Returns refresh rate of the monitor.
 */
s64 get_screen_rate(s64 vmode_idx) {
  return Gfx::get_screen_rate(vmode_idx);
}

/*!
 * Returns amount of video modes of the monitor.
 */
s64 get_screen_vmode_count() {
  return Gfx::get_screen_vmode_count();
}

/*!
 * Returns the number of available monitors.
 */
int get_monitor_count() {
  return Gfx::get_monitor_count();
}

int get_unix_timestamp() {
  return std::time(nullptr);
}

void mkdir_path(u32 filepath) {
  auto filepath_str = std::string(Ptr<String>(filepath).c()->data());
  file_util::create_dir_if_needed_for_file(filepath_str);
}

u64 filepath_exists(u32 filepath) {
  auto filepath_str = std::string(Ptr<String>(filepath).c()->data());
  if (fs::exists(filepath_str)) {
    return s7.offset + true_symbol_offset(g_game_version);
  }
  return s7.offset;
}

void prof_event(u32 name, u32 kind) {
  prof().event(Ptr<String>(name).c()->data(), (ProfNode::Kind)kind);
}

void set_frame_rate(s64 rate) {
  Gfx::set_frame_rate(rate);
}

void set_vsync(u32 symptr) {
  Gfx::set_vsync(symptr != s7.offset);
}

void set_window_lock(u32 symptr) {
  Gfx::set_window_lock(symptr == s7.offset);
}

void set_collision(u32 symptr) {
  Gfx::g_global_settings.collision_enable = symptr != s7.offset;
}

void set_collision_wireframe(u32 symptr) {
  Gfx::g_global_settings.collision_wireframe = symptr != s7.offset;
}

void set_collision_mask(GfxGlobalSettings::CollisionRendererMode mode, int mask, u32 symptr) {
  if (symptr != s7.offset) {
    Gfx::CollisionRendererSetMask(mode, mask);
  } else {
    Gfx::CollisionRendererClearMask(mode, mask);
  }
}

u32 get_collision_mask(GfxGlobalSettings::CollisionRendererMode mode, int mask) {
  return Gfx::CollisionRendererGetMask(mode, mask) ? s7.offset + true_symbol_offset(g_game_version)
                                                   : s7.offset;
}

void set_gfx_hack(u64 which, u32 symptr) {
  switch (which) {
    case 0:  // no tex
      Gfx::g_global_settings.hack_no_tex = symptr != s7.offset;
      break;
  }
}

/*!
 * PC PORT FUNCTIONS END
 */

void vif_interrupt_callback(int bucket_id) {
  // added for the PC port for faking VIF interrupts from the graphics system.
  if (vif1_interrupt_handler && MasterExit == RuntimeExitStatus::RUNNING) {
    call_goal(Ptr<Function>(vif1_interrupt_handler), bucket_id, 0, 0, s7.offset, g_ee_main_mem);
  }
}

/*!
 * Added in PC port.
 */
u32 offset_of_s7() {
  return s7.offset;
}

/*!
 * Called from the game thread at initialization.
 * The game thread is the only one to touch the mips2c function table (through the linker and
 * through this function), so no locking is needed.
 */
u64 pc_get_mips2c(u32 name) {
  const char* n = Ptr<String>(name).c()->data();
  return Mips2C::gLinkedFunctionTable.get(n);
}

/*!
 * Called from game thread to submit rendering DMA chain.
 */
void send_gfx_dma_chain(u32 /*bank*/, u32 chain) {
  Gfx::send_chain(g_ee_main_mem, chain);
}

/*!
 * Called from game thread to upload a texture outside of the main DMA chain.
 */
void pc_texture_upload_now(u32 page, u32 mode) {
  Gfx::texture_upload_now(Ptr<u8>(page).c(), mode, s7.offset);
}

void pc_texture_relocate(u32 dst, u32 src, u32 format) {
  Gfx::texture_relocate(dst, src, format);
}

u64 get_tas_frame() {
  return tas_frame;
}

u64 get_skip_spool_movies() {
  return skip_spool_movies;
}
