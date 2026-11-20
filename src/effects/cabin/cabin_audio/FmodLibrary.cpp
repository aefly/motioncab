#include "FmodLibrary.hpp"

#include "core/PluginContext.hpp"
#include "core/Strings.hpp"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace motioncab {

namespace {

constexpr const char *kStudioDll = "fmodstudio.dll";
constexpr const char *kStudioUpdate =
    "?update@System@Studio@FMOD@@QEAA?AW4FMOD_RESULT@@XZ";
// Past every type FMOD 2.x has; an unknown one only returns an error.
constexpr int kMaxDspType = 64;

using StudioUpdateFn = int (*)(void *studio);

std::atomic<void *> captured_studio{nullptr};
std::atomic<unsigned> update_calls{0};
StudioUpdateFn original_update = nullptr;
void **update_slot = nullptr;

// Studio::System::update is a member function: on x64, `this` comes first,
// like any first argument.
int HookedUpdate(void *studio) {
  captured_studio.store(studio, std::memory_order_relaxed);
  update_calls.fetch_add(1, std::memory_order_relaxed);
  return original_update(studio);
}

// The game's import address table entry for `function` of `dll`, matched by
// name: another hook may have replaced the address already.
void **FindImportSlot(HMODULE module, const char *dll, const char *function) {
  auto *base = reinterpret_cast<BYTE *>(module);
  const auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
  const auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY &imports =
      nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!imports.VirtualAddress)
    return nullptr;
  for (auto *desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(
           base + imports.VirtualAddress);
       desc->Name; ++desc) {
    if (!strings::EqualsIgnoreCase(reinterpret_cast<char *>(base + desc->Name),
                                   dll) ||
        !desc->OriginalFirstThunk)
      continue;
    auto *names =
        reinterpret_cast<IMAGE_THUNK_DATA *>(base + desc->OriginalFirstThunk);
    auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + desc->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots) {
      if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
        continue;
      const auto *by_name = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(
          base + names->u1.AddressOfData);
      if (std::strcmp(reinterpret_cast<const char *>(by_name->Name),
                      function) == 0)
        return reinterpret_cast<void **>(&slots->u1.Function);
    }
  }
  return nullptr;
}

bool WriteSlot(void **slot, void *value) {
  DWORD old_protect = 0;
  if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old_protect))
    return false;
  *slot = value;
  VirtualProtect(slot, sizeof(void *), old_protect, &old_protect);
  return true;
}

template <typename Fn> bool Resolve(HMODULE module, const char *name, Fn &out) {
  out = reinterpret_cast<Fn>(
      reinterpret_cast<void *>(GetProcAddress(module, name)));
  if (!out)
    Context().LogFmt(SPF_LOG_WARN,
                     "(CabinAudio) %s not found in the game's FMOD", name);
  return out != nullptr;
}

} // namespace

bool FmodLibrary::Load() {
  if (load_tried_)
    return loaded_;
  load_tried_ = true;
  const HMODULE core = GetModuleHandleW(L"fmod.dll");
  const HMODULE studio = GetModuleHandleA(kStudioDll);
  if (!core || !studio) {
    Context().Log(SPF_LOG_WARN, "(CabinAudio) The game's FMOD isn't loaded");
    return false;
  }
  // Every one is tried, so the log names all that are missing.
  bool ok =
      Resolve(studio, "FMOD_Studio_System_IsValid", Studio_System_IsValid);
  ok &= Resolve(studio, "FMOD_Studio_System_GetCoreSystem",
                Studio_System_GetCoreSystem);
  ok &= Resolve(studio, "FMOD_Studio_System_GetBankCount",
                Studio_System_GetBankCount);
  ok &= Resolve(studio, "FMOD_Studio_System_GetBankList",
                Studio_System_GetBankList);
  ok &=
      Resolve(studio, "FMOD_Studio_Bank_GetBusCount", Studio_Bank_GetBusCount);
  ok &= Resolve(studio, "FMOD_Studio_Bank_GetBusList", Studio_Bank_GetBusList);
  ok &= Resolve(studio, "FMOD_Studio_Bus_GetPath", Studio_Bus_GetPath);
  ok &= Resolve(studio, "FMOD_Studio_Bus_IsValid", Studio_Bus_IsValid);
  ok &= Resolve(studio, "FMOD_Studio_Bus_GetChannelGroup",
                Studio_Bus_GetChannelGroup);
  ok &= Resolve(core, "FMOD_ChannelGroup_AddDSP", ChannelGroup_AddDSP);
  ok &= Resolve(core, "FMOD_ChannelGroup_RemoveDSP", ChannelGroup_RemoveDSP);
  ok &=
      Resolve(core, "FMOD_System_GetSoftwareFormat", System_GetSoftwareFormat);
  ok &= Resolve(core, "FMOD_System_CreateDSP", System_CreateDSP);
  ok &= Resolve(core, "FMOD_System_CreateDSPByType", System_CreateDSPByType);
  ok &= Resolve(core, "FMOD_DSP_GetInfo", DSP_GetInfo);
  ok &= Resolve(core, "FMOD_DSP_Release", DSP_Release);
  ok &= Resolve(core, "FMOD_DSP_DisconnectAll", DSP_DisconnectAll);
  ok &= Resolve(core, "FMOD_DSP_SetBypass", DSP_SetBypass);
  ok &= Resolve(core, "FMOD_DSP_SetParameterFloat", DSP_SetParameterFloat);
  ok &= Resolve(core, "FMOD_DSP_SetParameterInt", DSP_SetParameterInt);
  ok &= Resolve(core, "FMOD_DSP_GetNumParameters", DSP_GetNumParameters);
  ok &= Resolve(core, "FMOD_DSP_GetParameterInfo", DSP_GetParameterInfo);
  if (!ok)
    return false;

  if (!update_slot) {
    void **slot =
        FindImportSlot(GetModuleHandleW(nullptr), kStudioDll, kStudioUpdate);
    if (!slot) {
      Context().Log(SPF_LOG_WARN,
                    "(CabinAudio) The game doesn't import Studio's update");
      return false;
    }
    original_update = reinterpret_cast<StudioUpdateFn>(*slot);
    if (!WriteSlot(slot, reinterpret_cast<void *>(&HookedUpdate))) {
      Context().Log(SPF_LOG_WARN, "(CabinAudio) Couldn't hook Studio's update");
      return false;
    }
    update_slot = slot;
  }
  loaded_ = true;
  return true;
}

void FmodLibrary::Unload() {
  if (update_slot) {
    // Someone hooked it after us: putting the original back would drop
    // theirs. Theirs calls ours, so there's no safe way out but to say so.
    if (*update_slot != reinterpret_cast<void *>(&HookedUpdate))
      Context().Log(SPF_LOG_ERROR,
                    "(CabinAudio) Studio's update was hooked over ours");
    else
      WriteSlot(update_slot, reinterpret_cast<void *>(original_update));
    update_slot = nullptr;
  }
  captured_studio.store(nullptr, std::memory_order_relaxed);
  load_tried_ = false;
  loaded_ = false;
}

unsigned FmodLibrary::UpdateCalls() const {
  return update_calls.load(std::memory_order_relaxed);
}

void *FmodLibrary::StudioSystem() const {
  void *studio = captured_studio.load(std::memory_order_relaxed);
  // Studio's handles are checked, so a released system just isn't valid.
  if (!loaded_ || !studio || !Studio_System_IsValid(studio))
    return nullptr;
  return studio;
}

void *FmodLibrary::CoreSystem(void *studio) const {
  void *core = nullptr;
  if (!loaded_ || !studio || Studio_System_GetCoreSystem(studio, &core) != kOk)
    return nullptr;
  return core;
}

template <typename Visit>
void FmodLibrary::ForEachBus(void *studio, Visit visit) const {
  int bank_count = 0;
  if (!loaded_ || !studio ||
      Studio_System_GetBankCount(studio, &bank_count) != kOk || bank_count <= 0)
    return;
  std::vector<void *> banks(bank_count);
  if (Studio_System_GetBankList(studio, banks.data(), bank_count,
                                &bank_count) != kOk)
    return;
  banks.resize(bank_count);
  std::vector<void *> buses;
  char path[256];
  for (void *bank : banks) {
    int bus_count = 0;
    if (Studio_Bank_GetBusCount(bank, &bus_count) != kOk || bus_count <= 0)
      continue;
    buses.resize(bus_count);
    if (Studio_Bank_GetBusList(bank, buses.data(), bus_count, &bus_count) !=
        kOk)
      continue;
    for (int i = 0; i < bus_count; ++i) {
      int retrieved = 0;
      // No path without the strings bank: shown as such.
      if (Studio_Bus_GetPath(buses[i], path, sizeof(path), &retrieved) != kOk)
        std::snprintf(path, sizeof(path), "(no path)");
      if (visit(buses[i], path))
        return;
    }
  }
}

void *FmodLibrary::FindBus(void *studio, const char *name) const {
  void *found = nullptr;
  ForEachBus(studio, [&](void *bus, const char *path) {
    std::string_view last(path);
    if (const size_t slash = last.rfind('/'); slash != std::string_view::npos)
      last.remove_prefix(slash + 1);
    if (!strings::EqualsIgnoreCase(last, name))
      return false;
    found = bus;
    return true;
  });
  return found;
}

void FmodLibrary::LogBuses(void *studio, const char *filter) const {
  PluginContext &ctx = Context();
  int bank_count = 0;
  Studio_System_GetBankCount(studio, &bank_count);
  ctx.LogFmt(SPF_LOG_INFO,
             "(CabinAudio) Buses in %d banks matching \"%s\":", bank_count,
             filter ? filter : "");
  const std::string needle = filter ? strings::ToLower(filter) : "";
  ForEachBus(studio, [&](void *bus, const char *path) {
    if (strings::ToLower(path).find(needle) == std::string::npos)
      return false;
    void *group = nullptr;
    const int result = Studio_Bus_GetChannelGroup(bus, &group);
    ctx.LogFmt(SPF_LOG_INFO, "(CabinAudio)   %s (bus %p, group %p, result %d)",
               path, bus, group, result);
    return false;
  });
}

void *FmodLibrary::CreateDspByName(void *core, const char *name) const {
  if (!loaded_ || !core)
    return nullptr;
  for (int type = 1; type < kMaxDspType; ++type) {
    void *dsp = nullptr;
    if (System_CreateDSPByType(core, type, &dsp) != kOk || !dsp)
      continue;
    char buffer[32] = {};
    unsigned int version = 0;
    int channels = 0, width = 0, height = 0;
    if (DSP_GetInfo(dsp, buffer, &version, &channels, &width, &height) == kOk &&
        strings::EqualsIgnoreCase(buffer, name))
      return dsp;
    DSP_Release(dsp);
  }
  Context().LogFmt(SPF_LOG_WARN,
                   "(CabinAudio) No \"%s\" DSP in the game's FMOD", name);
  return nullptr;
}

int FmodLibrary::FindParameter(void *dsp, const char *name) const {
  // The start of FMOD_DSP_PARAMETER_DESC, the same in every 2.x.
  struct ParameterDesc {
    int type;
    char name[16];
  };
  int count = 0;
  if (!loaded_ || !dsp || DSP_GetNumParameters(dsp, &count) != kOk)
    return -1;
  for (int i = 0; i < count; ++i) {
    void *desc = nullptr;
    if (DSP_GetParameterInfo(dsp, i, &desc) != kOk || !desc)
      continue;
    const auto *parameter = static_cast<const ParameterDesc *>(desc);
    if (strings::EqualsIgnoreCase(
            std::string_view(parameter->name,
                             strnlen(parameter->name, sizeof(parameter->name))),
            name))
      return i;
  }
  std::string names;
  for (int i = 0; i < count; ++i) {
    void *desc = nullptr;
    if (DSP_GetParameterInfo(dsp, i, &desc) != kOk || !desc)
      continue;
    const auto *parameter = static_cast<const ParameterDesc *>(desc);
    names += names.empty() ? "" : ", ";
    names.append(parameter->name,
                 strnlen(parameter->name, sizeof(parameter->name)));
  }
  Context().LogFmt(SPF_LOG_WARN,
                   "(CabinAudio) No \"%s\" parameter on a DSP (it has: %s)",
                   name, names.c_str());
  return -1;
}

} // namespace motioncab
