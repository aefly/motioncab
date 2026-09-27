#include "SettingsTab.hpp"

#include "SPF_Icons.h"
#include "core/Keybinds.hpp"
#include "core/Localization.hpp"
#include "core/PluginContext.hpp"
#include "core/ProfileManager.hpp"
#include "core/SettingsSchema.hpp"
#include "ui/Widgets.hpp"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace motioncab::ui {

namespace {

// "###" IDs of the confirmation popups: their visible title is translated,
// and OpenPopup/BeginPopupModal must still agree on the same ID.
constexpr const char *kOverwritePopupId = "###confirm_overwrite_profile";
constexpr const char *kDeletePopupId = "###confirm_delete_profile";
constexpr const char *kResetPopupId = "###confirm_reset_all";

// `field_w`: width of the name field and the profile dropdown, which the
// buttons next to them follow.
void DrawProfilesSection(SPF_UI_API *ui, float field_w) {
  PluginContext &ctx = Context();

  DrawSectionTitle(ui, loc::Tr("ui.profiles.section"));

  static char profile_name_buf[64] = "";
  static std::string pending_select;
  static std::string pending_overwrite;

  auto do_save = [&](const std::string &saved_name) {
    if (profiles::Save(ctx, saved_name)) {
      profile_name_buf[0] = '\0';
      pending_select = saved_name;
      ShowToast(ui, SPF_NOTIFICATION_SUCCESS,
                loc::Tr("ui.profiles.saved_toast", {{"name", saved_name}}));
      return true;
    }
    ShowToast(ui, SPF_NOTIFICATION_ERROR, loc::Tr("ui.profiles.save_failed"));
    return false;
  };

  ui->UI_SetNextItemWidth(field_w);
  ui->UI_InputTextWithHint("##profile_name", loc::Tr("ui.profiles.name_hint"),
                           profile_name_buf, sizeof(profile_name_buf),
                           SPF_INPUT_TEXT_FLAG_NONE);
  ui->UI_SameLine(0.0f, 8.0f);
  const std::string save_label =
      WithIcon(ICON_FA_FLOPPY_DISK, loc::Tr("ui.profiles.save_as")) +
      "###save_as";
  if (MutedButton(ui, save_label.c_str())) {
    const std::string sanitized = profiles::Sanitize(profile_name_buf);
    if (sanitized.empty()) {
      ShowToast(ui, SPF_NOTIFICATION_WARNING,
                loc::Tr("ui.profiles.invalid_name"));
    } else {
      const std::vector<std::string> existing = profiles::List(ctx);
      // Case-insensitive: on Windows "default" is the same file as
      // "Default". Reuse the stored spelling so the active profile name
      // keeps matching the dropdown entry.
      const std::string *match = profiles::FindIgnoreCase(existing, sanitized);
      if (match) {
        pending_overwrite = *match;
        ui->UI_OpenPopup(kOverwritePopupId, SPF_POPUP_FLAG_NONE);
      } else {
        do_save(sanitized);
      }
    }
  }
  ui->UI_SetItemTooltip(loc::Tr("ui.profiles.save_as_tip"));

  const std::string overwrite_title =
      loc::Tr("ui.profiles.overwrite_title") + std::string(kOverwritePopupId);
  const std::string overwrite_message =
      loc::Tr("ui.profiles.overwrite_message", {{"name", pending_overwrite}});
  if (ConfirmPopup(ui, overwrite_title, overwrite_message.c_str(),
                   loc::Tr("ui.profiles.overwrite_confirm")))
    do_save(pending_overwrite);

  ui->UI_Spacing();
  const std::vector<std::string> profile_names = profiles::List(ctx);
  if (profile_names.empty()) {
    ui->UI_TextDisabled(loc::Tr("ui.profiles.none"));
  } else {
    // -1 means "no profile matches the current live settings", e.g. a
    // manual edit was made since the last Load()/Save(). Shown as its own
    // placeholder rather than defaulting to index 0, which would falsely
    // claim whatever profile sorts first (often "Default") is active.
    static int selected_profile = -2; // -2: not yet synced this session
    static bool selection_initialized = false;
    if (!selection_initialized) {
      selection_initialized = true;
      const std::string last_used = profiles::LastUsedName(ctx);
      const auto it =
          std::find(profile_names.begin(), profile_names.end(), last_used);
      selected_profile = (!last_used.empty() && it != profile_names.end())
                             ? static_cast<int>(it - profile_names.begin())
                             : -1;
    }
    if (!pending_select.empty()) {
      // Only updates which entry the dropdown shows as selected, no
      // Load() here. Saving already copied the current live settings into
      // this profile's file, so reloading it right back would just be a
      // redundant round-trip to the same values.
      const auto it =
          std::find(profile_names.begin(), profile_names.end(), pending_select);
      selected_profile = it != profile_names.end()
                             ? static_cast<int>(it - profile_names.begin())
                             : -1;
      pending_select.clear();
    }
    if (selected_profile >= static_cast<int>(profile_names.size()))
      selected_profile = static_cast<int>(profile_names.size()) - 1;

    const char *preview = selected_profile >= 0
                              ? profile_names[selected_profile].c_str()
                              : loc::Tr("ui.unsaved_changes");
    ui->UI_SetNextItemWidth(field_w);
    ui->UI_PushStyleVarFloat(SPF_STYLE_VAR_POPUP_BORDERSIZE, 1.0f);
    const bool combo_open =
        ui->UI_BeginCombo("##profile_select", preview, SPF_COMBO_FLAG_NONE);
    if (combo_open) {
      for (size_t i = 0; i < profile_names.size(); ++i) {
        const bool is_selected = static_cast<int>(i) == selected_profile;
        if (ui->UI_Selectable(profile_names[i].c_str(), is_selected,
                              SPF_SELECTABLE_FLAG_NONE, 0.0f, 0.0f)) {
          if (profiles::Load(ctx, profile_names[i])) {
            selected_profile = static_cast<int>(i);
          } else {
            ShowToast(ui, SPF_NOTIFICATION_ERROR,
                      loc::Tr("ui.profiles.load_failed",
                              {{"name", profile_names[i]}}));
          }
        }
      }
      ui->UI_EndCombo();
    }
    ui->UI_PopStyleVar(1);
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.select_tip"));

    if (selected_profile < 0) {
      ui->UI_TextDisabled(loc::Tr("ui.profiles.no_match"));
      return;
    }

    const std::string &selected_name = profile_names[selected_profile];

    ui->UI_SameLine(0.0f, 8.0f);
    static std::chrono::steady_clock::time_point save_confirm_until{};
    const bool save_confirmed =
        std::chrono::steady_clock::now() < save_confirm_until;
    const std::string update_label =
        (save_confirmed
             ? WithIcon(ICON_FA_CHECK, loc::Tr("ui.profiles.saved"))
             : WithIcon(ICON_FA_FLOPPY_DISK, loc::Tr("ui.profiles.save"))) +
        "###save";
    if (MutedButton(ui, update_label.c_str()) && do_save(selected_name))
      save_confirm_until =
          std::chrono::steady_clock::now() + std::chrono::seconds(2);
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.save_tip"));

    ui->UI_SameLine(0.0f, 4.0f);
    const std::string delete_label =
        WithIcon(ICON_FA_TRASH, loc::Tr("ui.profiles.delete")) + "###delete";
    static std::string pending_delete;
    if (MutedButton(ui, delete_label.c_str())) {
      pending_delete = selected_name;
      ui->UI_OpenPopup(kDeletePopupId, SPF_POPUP_FLAG_NONE);
    }
    ui->UI_SetItemTooltip(loc::Tr("ui.profiles.delete_tip"));

    const std::string delete_title =
        loc::Tr("ui.profiles.delete_title") + std::string(kDeletePopupId);
    const std::string delete_message =
        loc::Tr("ui.profiles.delete_message", {{"name", pending_delete}});
    if (ConfirmPopup(ui, delete_title, delete_message.c_str(),
                     loc::Tr("ui.profiles.delete_confirm"))) {
      if (!profiles::Delete(ctx, pending_delete)) {
        ShowToast(
            ui, SPF_NOTIFICATION_ERROR,
            loc::Tr("ui.profiles.delete_failed", {{"name", pending_delete}}));
      } else {
        ShowToast(
            ui, SPF_NOTIFICATION_SUCCESS,
            loc::Tr("ui.profiles.deleted_toast", {{"name", pending_delete}}));
        // Deleting the active profile shouldn't leave its values loaded,
        // so fall back to "Default", recreating it first if it's what
        // just got deleted.
        profiles::EnsureDefaultExists(ctx);
        if (profiles::Load(ctx, profiles::kDefaultProfileName)) {
          const std::vector<std::string> refreshed = profiles::List(ctx);
          const auto it = std::find(refreshed.begin(), refreshed.end(),
                                    profiles::kDefaultProfileName);
          selected_profile = it != refreshed.end()
                                 ? static_cast<int>(it - refreshed.begin())
                                 : -1;
        } else {
          ShowToast(ui, SPF_NOTIFICATION_ERROR,
                    loc::Tr("ui.profiles.fallback_failed",
                            {{"name", profiles::kDefaultProfileName}}));
        }
      }
    }
  }
}

// Global reset, last in the tab and away from the effect tabs so it isn't
// clicked by accident.
void DrawResetSection(SPF_UI_API *ui, SPF_Config_API *cfg,
                      SPF_Config_Handle *h) {
  PluginContext &ctx = Context();

  DrawSectionTitle(ui, loc::Tr("ui.reset.section"));
  const std::string reset_label =
      WithIcon(ICON_FA_ARROW_ROTATE_LEFT, loc::Tr("ui.reset.button")) +
      "###reset_all";
  if (MutedButton(ui, reset_label.c_str()))
    ui->UI_OpenPopup(kResetPopupId, SPF_POPUP_FLAG_NONE);

  const std::string title =
      loc::Tr("ui.reset.title") + std::string(kResetPopupId);
  if (ConfirmPopup(ui, title, loc::Tr("ui.reset.message"),
                   loc::Tr("ui.reset.confirm"))) {
    const std::string active_profile = profiles::LastUsedName(ctx);
    settings::WriteDefaults(cfg, h);
    if (!active_profile.empty())
      profiles::Save(ctx, active_profile);
    ShowToast(ui, SPF_NOTIFICATION_SUCCESS, loc::Tr("ui.reset.done_toast"));
  }
}

} // namespace

void DrawSettingsTab(SPF_UI_API *ui, SPF_Config_API *cfg,
                     SPF_Config_Handle *h) {
  DrawSectionTitle(ui, loc::Tr("ui.keybind.section"));
  // The keybind button and the profiles section's buttons all start at
  // the same x, just past the profile name field (180 px, or wider when
  // the translated keybind label needs the room).
  constexpr float kGap = 8.0f;
  const char *toggle_label = loc::Tr("ui.keybind.toggle_window");
  float label_w = 0.0f, label_h = 0.0f;
  ui->UI_CalcTextSize(toggle_label, &label_w, &label_h);
  const float field_w = std::max(180.0f, label_w);
  DrawKeybindRowAt(ui, keybinds::kToggleWindow.id, toggle_label,
                   field_w + kGap);

  ui->UI_Spacing();
  DrawProfilesSection(ui, field_w);

  ui->UI_Spacing();
  DrawResetSection(ui, cfg, h);
}

} // namespace motioncab::ui
