#include "CabinWalkSounds.hpp"

#include "core/PluginContext.hpp"

namespace motioncab {

namespace {

constexpr const char *kBankFile = "MotionCab.bank";
constexpr const char *kGuidsFile = "MotionCab.bank.guids";

constexpr const char *kFootstepEvent = "event:/motioncab/footsteps";

// So no two footsteps sound alike.
constexpr float kStepPitchMin = 0.93f, kStepPitchMax = 1.07f;
constexpr float kStepVolumeMin = 0.8f;

} // namespace

void CabinWalkSounds::Load(SPF_Sound_API *sound) {
  PluginContext &ctx = Context();
  const std::string bank = dir_ + "/" + kBankFile;
  const std::string guids = dir_ + "/" + kGuidsFile;
  bank_ = sound->SND_LoadBankFile(bank.c_str(), guids.c_str());
  if (!bank_)
    ctx.LogFmt(SPF_LOG_WARN, "(CabinWalk) %s not loaded", kBankFile);
}

void CabinWalkSounds::Update(SPF_Sound_API *sound) {
  if (!sound)
    return;
  if (!sound->SND_IsReady()) {
    // The bank's handle went with the world.
    ready_ = false;
    bank_ = nullptr;
    return;
  }
  if (!ready_) {
    ready_ = true;
    if (!dir_.empty())
      Load(sound);
  }
}

void CabinWalkSounds::PlayFootstep(SPF_Sound_API *sound, float volume) {
  if (!sound || !ready_ || !bank_)
    return;
  // Not kept: SPF renumbers the events whenever a bank is loaded or unloaded.
  const int event = sound->SND_FindEventIndexByPath(kFootstepEvent);
  if (event < 0) {
    Context().LogThrottledFmt(SPF_LOG_WARN, "cabin_walk_sound_missing", 10000,
                              "(CabinWalk) %s not found", kFootstepEvent);
    return;
  }
  void *instance = sound->SND_CreateEventInstance(event);
  if (!instance)
    return;
  volume *=
      std::uniform_real_distribution<float>(kStepVolumeMin, 1.0f)(random_);
  const float pitch = std::uniform_real_distribution<float>(
      kStepPitchMin, kStepPitchMax)(random_);
  sound->SND_SetEventVolume(instance, volume);
  sound->SND_SetEventPitch(instance, pitch);
  sound->SND_StartEvent(instance);
  // FMOD frees it once it has finished playing.
  sound->SND_ReleaseEvent(instance);
}

void CabinWalkSounds::Shutdown(SPF_Sound_API *sound) {
  if (sound && ready_ && sound->SND_IsReady()) {
    if (bank_)
      sound->SND_UnloadBank(bank_);
  }
  ready_ = false;
  bank_ = nullptr;
}

} // namespace motioncab
