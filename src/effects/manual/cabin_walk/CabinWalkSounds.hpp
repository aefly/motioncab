#pragma once

#include "SPF_Sound_API.h"

#include <random>
#include <string>

namespace motioncab {

// Plays Cabin Walk's footsteps through the game's FMOD system (SPF's sound
// API), from MotionCab's own bank in data/sounds/, built with FMOD Studio:
// MotionCab.bank with its event:/motioncab/footsteps event, and
// MotionCab.bank.guids (FMOD Studio's exported GUIDs.txt), which names the
// events' GUIDs. The events must output to a bus of the game's (the game
// refuses a second master bank): a bank routed to its own project's master
// bus loads, but stays silent. The FMOD project's master bus has the GUID
// of the game's bus:/cabin/interior ({7fe623a8-a2f0-4ba8-ad2a-9cad68432851},
// read from the game's master bank), where the cabin's own sounds go:
// bus:/outside/* would muffle them like the world heard from inside.
//
// The sound system only exists while a game world is loaded, so the bank is
// loaded once it's ready, and again after a world reload. SPF reads the
// .guids file along with the bank, so the events are found by path.
class CabinWalkSounds {
public:
  // `dir`: the folder holding the bank files.
  void SetDirectory(std::string dir) { dir_ = std::move(dir); }

  // Loads the bank once the sound system is ready. Every frame.
  void Update(SPF_Sound_API *sound);

  // `volume`: 0..1.
  void PlayFootstep(SPF_Sound_API *sound, float volume);

  // Plugin or world unload: unloads the bank if the sound system still
  // runs, else forgets its handle, so Update loads it again.
  void Shutdown(SPF_Sound_API *sound);

private:
  void Load(SPF_Sound_API *sound);

  std::string dir_;
  bool ready_ = false; // loaded for the current sound system
  void *bank_ = nullptr;
  std::minstd_rand random_{std::random_device{}()};
};

} // namespace motioncab
