#pragma once

#include "SPF_Sound_API.h"

#include <random>
#include <string>

namespace motioncab {

// Cabin Walk's footsteps, from our own FMOD Studio bank in data/sounds/.
// MotionCab.bank.guids is FMOD Studio's exported GUIDs.txt, which lets SPF
// find the events by path.
//
// The events must go to one of the game's buses: the game refuses a second
// master bank, and a bank routed to its own master bus loads but stays
// silent. So the FMOD project's master bus has the GUID of the game's
// bus:/cabin/interior ({7fe623a8-a2f0-4ba8-ad2a-9cad68432851}, read from
// the game's master bank), where the cabin's own sounds go: bus:/outside/*
// would muffle them like the world heard from inside.
//
// The sound system only exists while a world is loaded, so the bank is
// loaded again after every world reload.
class CabinWalkSounds {
public:
  void SetDirectory(std::string dir) { dir_ = std::move(dir); }

  // Every frame.
  void Update(SPF_Sound_API *sound);

  // `volume` is 0..1.
  void PlayFootstep(SPF_Sound_API *sound, float volume);

  void Shutdown(SPF_Sound_API *sound);

private:
  void Load(SPF_Sound_API *sound);

  std::string dir_;
  bool ready_ = false; // for the current sound system
  void *bank_ = nullptr;
  std::minstd_rand random_{std::random_device{}()};
};

} // namespace motioncab
