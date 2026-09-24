#include "FovSettingFallback.hpp"

#include "PluginContext.hpp"
#include "SPF_Hooks_API.h"

#include <cmath>

// TEMPORARY, see FovSettingFallback.hpp.
namespace motioncab::compat::fov_fallback {
namespace {

// Signatures and extraction offsets from SPF Framework (Apache-2.0):
// FreeCameraDataFinder.cpp phase 2 (gameplay manager slot + context offset)
// and FovDataFinder.cpp phases 4-6 as fixed by PR #11 (FOV setting chain).
constexpr const char *kGameSessionStr =
    "[used_vehicles] %Iu used truck offers have expired (%Iu offers valid)";
constexpr const char *kGameplayManagerMovSig = "[MOV r64, [rip+off32]]";
constexpr const char *kContextCmpSig = "48 83 BF ? ? ? ? 00";
// {1.61.1.0s r14 layout | 1.61.1.1s rdi layout} + zoom base read.
constexpr const char *kZoomAnchorSig =
    "{F3 41 0F 10 AE ? ? ? ?|F3 0F 10 BF ? ? ? ? 48 8D 94 24 ? ? ? ? "
    "F3 0F 10 2D ? ? ? ?} F3 44 0F 10 80";
constexpr const char *kSettingPtrSig = "[MOV r64, [r64+off32]] F3";
constexpr const char *kSettingOwnerSig = "[MOV r64, [r64+off8]]";

// Below this the game's speed-dependent FOV term is negligible.
constexpr float kStationarySpeedMps = 0.5f;

struct State {
  bool scanned = false;
  bool ready = false;
  bool logged_active = false;
  uintptr_t gameplay_manager_slot = 0;
  int32_t context_offset = 0;
  int32_t owner_offset = 0;
  int32_t ptr_offset = 0;
  int32_t val_offset = 0;
  bool has_base = false;
  float base_fov = 0.0f;
};
State g_state;

const SPF_Hooks_API *Hooks() {
  const PluginContext &ctx = Context();
  return ctx.core ? ctx.core->hooks : nullptr;
}

bool Scan(const SPF_Hooks_API *h) {
  const PluginContext &ctx = Context();

  const uintptr_t session_fn =
      h->Hook_FindFunctionByString(kGameSessionStr, true, nullptr, 512);
  const uintptr_t mov_addr =
      session_fn
          ? h->Hook_FindPatternFrom(kGameplayManagerMovSig, session_fn, 64)
          : 0;
  const uintptr_t cmp_addr =
      mov_addr ? h->Hook_FindPatternFrom(kContextCmpSig, mov_addr + 3, 64) : 0;
  if (!cmp_addr) {
    ctx.Log(SPF_LOG_WARN, "FOV fallback: gameplay manager not found");
    return false;
  }
  g_state.gameplay_manager_slot = h->Memory_GetRipAddress(mov_addr, 3, 7);
  g_state.context_offset = h->Memory_ReadInt32(cmp_addr + 3);

  const uintptr_t zoom_addr = h->Hook_FindPattern(kZoomAnchorSig);
  const uintptr_t setting_addr =
      zoom_addr ? h->Hook_FindBackward(zoom_addr, 600, kSettingPtrSig) : 0;
  const uintptr_t owner_addr =
      setting_addr ? h->Hook_FindBackward(setting_addr, 64, kSettingOwnerSig)
                   : 0;
  if (!owner_addr) {
    ctx.Log(SPF_LOG_WARN, "FOV fallback: FOV setting anchors not found");
    return false;
  }
  g_state.ptr_offset = h->Memory_ReadInt32(setting_addr + 3);
  g_state.val_offset = h->Memory_ReadInt32(setting_addr + 12);
  g_state.owner_offset = h->Memory_ReadInt8(owner_addr + 3);

  const bool ok = h->Memory_IsValidAddress(g_state.gameplay_manager_slot) &&
                  g_state.context_offset > 0 && g_state.owner_offset > 0 &&
                  g_state.ptr_offset > 0 && g_state.val_offset > 0;
  ctx.LogFmt(ok ? SPF_LOG_INFO : SPF_LOG_WARN,
             "FOV fallback scan %s (ctx 0x%X, owner 0x%X, ptr 0x%X, val 0x%X)",
             ok ? "ok" : "failed", g_state.context_offset,
             g_state.owner_offset, g_state.ptr_offset, g_state.val_offset);
  return ok;
}

bool EnsureScanned(const SPF_Hooks_API *h) {
  if (!g_state.scanned) {
    g_state.scanned = true; // one attempt: game code doesn't change at runtime
    g_state.ready = Scan(h);
  }
  return g_state.ready;
}

// Follows gameplay manager -> context -> owner -> setting object, like SPF's
// GameCameraInterior::GetFovSettingAddr, checking each hop. Resolved every
// call since these objects are rebuilt across game sessions.
uintptr_t SettingAddr(const SPF_Hooks_API *h) {
  auto deref = [h](uintptr_t addr) -> uintptr_t {
    return h->Memory_IsValidAddress(addr)
               ? static_cast<uintptr_t>(h->Memory_ReadInt64(addr))
               : 0;
  };
  const uintptr_t gm = deref(g_state.gameplay_manager_slot);
  const uintptr_t ctx = gm ? deref(gm + g_state.context_offset) : 0;
  const uintptr_t owner = ctx ? deref(ctx + g_state.owner_offset) : 0;
  const uintptr_t obj = owner ? deref(owner + g_state.ptr_offset) : 0;
  const uintptr_t addr = obj ? obj + g_state.val_offset : 0;
  return addr && h->Memory_IsValidAddress(addr) ? addr : 0;
}

} // namespace

bool GetFov(SPF_Camera_API *camera_api, bool allow_rebase, float *out_fov) {
  const SPF_Hooks_API *h = Hooks();
  if (!out_fov || !camera_api || !camera_api->Cam_GetInteriorFovReal || !h)
    return false;

  // A readable live FOV means the interior camera is resolved, so a failing
  // Cam_GetInteriorFov points at SPF's missing offsets, not a camera that
  // isn't ready yet.
  float live = 0.0f;
  if (!camera_api->Cam_GetInteriorFovReal(&live) || !EnsureScanned(h))
    return false;
  const uintptr_t addr = SettingAddr(h);
  if (!addr)
    return false;
  const float setting = h->Memory_ReadFloat(addr);

  const PluginContext &ctx = Context();
  const bool stationary =
      ctx.has_truck_data.load(std::memory_order_acquire) &&
      std::fabs(ctx.latest_truck_data.speed) < kStationarySpeedMps;
  // The first sample may include the speed term if the player enters the
  // cabin while moving; it's corrected at the next stop.
  if (!g_state.has_base || (allow_rebase && stationary)) {
    g_state.base_fov = live - setting;
    g_state.has_base = true;
  }

  if (!g_state.logged_active) {
    g_state.logged_active = true;
    ctx.Log(SPF_LOG_WARN,
            "SPF interior FOV API unavailable, Manual Zoom uses a temporary "
            "fallback (SPF-Framework PR #11)");
  }
  *out_fov = g_state.base_fov + setting;
  return true;
}

bool SetFov(float fov) {
  const SPF_Hooks_API *h = Hooks();
  if (!h || !g_state.ready || !g_state.has_base)
    return false;
  const uintptr_t addr = SettingAddr(h);
  if (!addr)
    return false;
  h->Memory_WriteFloat(addr, fov - g_state.base_fov);
  return true;
}

} // namespace motioncab::compat::fov_fallback
