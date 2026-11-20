#pragma once

namespace motioncab {

// The game's own FMOD, called directly: SPF's Sound API can set a bus's
// volume but can't pan it. fmod.dll and fmodstudio.dll ship apart and export
// FMOD's C API, so the functions are looked up once in the loaded DLLs.
//
// Studio's channel groups are named "Group Bus" and the like, not after
// their bus, so a bus is found by its path, which needs the Studio
// system. Neither FMOD nor SPF hands it out: the game's import of
// Studio::System::update is redirected through us, and `this` is it.
class FmodLibrary {
public:
  static constexpr int kOk = 0; // FMOD_OK

  bool Load();
  // Before the DLL goes away.
  void Unload();

  // The game's Studio system, once it has called update; null before.
  void *StudioSystem() const;
  unsigned UpdateCalls() const;
  void *CoreSystem(void *studio) const;
  // The bus whose path's last part is `name` (case-insensitive).
  void *FindBus(void *studio, const char *name) const;
  void LogBuses(void *studio, const char *filter = nullptr) const;
  // FMOD's DSP type enum isn't stable across versions, names are.
  void *CreateDspByName(void *core, const char *name) const;
  // -1 if it has none.
  int FindParameter(void *dsp, const char *name) const;

  using StudioSystemIsValidFn = int (*)(void *studio);
  using StudioSystemGetCoreSystemFn = int (*)(void *studio, void **core);
  using StudioSystemGetBankCountFn = int (*)(void *studio, int *count);
  using StudioSystemGetBankListFn = int (*)(void *studio, void **banks,
                                            int capacity, int *count);
  using StudioBankGetBusCountFn = int (*)(void *bank, int *count);
  using StudioBankGetBusListFn = int (*)(void *bank, void **buses, int capacity,
                                         int *count);
  using StudioBusGetPathFn = int (*)(void *bus, char *path, int size,
                                     int *retrieved);
  using StudioBusIsValidFn = int (*)(void *bus);
  using StudioBusGetChannelGroupFn = int (*)(void *bus, void **group);
  using ChannelGroupAddDspFn = int (*)(void *group, int index, void *dsp);
  using ChannelGroupRemoveDspFn = int (*)(void *group, void *dsp);
  using SystemGetSoftwareFormatFn = int (*)(void *core, int *sample_rate,
                                            int *speaker_mode,
                                            int *raw_speakers);
  using SystemCreateDspFn = int (*)(void *core, const void *description,
                                    void **dsp);
  using SystemCreateDspByTypeFn = int (*)(void *core, int type, void **dsp);
  using DspGetInfoFn = int (*)(void *dsp, char *name, unsigned int *version,
                               int *channels, int *config_width,
                               int *config_height);
  using DspReleaseFn = int (*)(void *dsp);
  using DspDisconnectAllFn = int (*)(void *dsp, int inputs, int outputs);
  using DspSetBypassFn = int (*)(void *dsp, int bypass);
  using DspSetParameterFloatFn = int (*)(void *dsp, int index, float value);
  using DspSetParameterIntFn = int (*)(void *dsp, int index, int value);
  using DspGetNumParametersFn = int (*)(void *dsp, int *count);
  // FMOD_DSP_PARAMETER_DESC**: its name is a char[16] after the type.
  using DspGetParameterInfoFn = int (*)(void *dsp, int index, void **desc);

  StudioSystemIsValidFn Studio_System_IsValid = nullptr;
  StudioSystemGetCoreSystemFn Studio_System_GetCoreSystem = nullptr;
  StudioSystemGetBankCountFn Studio_System_GetBankCount = nullptr;
  StudioSystemGetBankListFn Studio_System_GetBankList = nullptr;
  StudioBankGetBusCountFn Studio_Bank_GetBusCount = nullptr;
  StudioBankGetBusListFn Studio_Bank_GetBusList = nullptr;
  StudioBusGetPathFn Studio_Bus_GetPath = nullptr;
  StudioBusIsValidFn Studio_Bus_IsValid = nullptr;
  StudioBusGetChannelGroupFn Studio_Bus_GetChannelGroup = nullptr;
  ChannelGroupAddDspFn ChannelGroup_AddDSP = nullptr;
  ChannelGroupRemoveDspFn ChannelGroup_RemoveDSP = nullptr;
  SystemGetSoftwareFormatFn System_GetSoftwareFormat = nullptr;
  SystemCreateDspFn System_CreateDSP = nullptr;
  SystemCreateDspByTypeFn System_CreateDSPByType = nullptr;
  DspGetInfoFn DSP_GetInfo = nullptr;
  DspReleaseFn DSP_Release = nullptr;
  DspDisconnectAllFn DSP_DisconnectAll = nullptr;
  DspSetBypassFn DSP_SetBypass = nullptr;
  DspSetParameterFloatFn DSP_SetParameterFloat = nullptr;
  DspSetParameterIntFn DSP_SetParameterInt = nullptr;
  DspGetNumParametersFn DSP_GetNumParameters = nullptr;
  DspGetParameterInfoFn DSP_GetParameterInfo = nullptr;

private:
  // Until `visit(bus, path)` returns true.
  template <typename Visit> void ForEachBus(void *studio, Visit visit) const;

  bool load_tried_ = false;
  bool loaded_ = false;
};

} // namespace motioncab
