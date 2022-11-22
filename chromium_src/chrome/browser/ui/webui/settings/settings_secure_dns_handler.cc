/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"

#if BUILDFLAG(IS_WIN)
#include "base/values.h"
#include "brave/components/brave_vpn/features.h"
#include "brave/components/brave_vpn/pref_names.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/prefs/pref_service.h"

namespace {

// Check if we have overriden DNS config by BraveVPN and block settings from
// changes.
bool IsBlockedByBraveVPN() {
  if (!base::FeatureList::IsEnabled(
          brave_vpn::features::kBraveVPNDnsProtection))
    return false;
  auto* browser = chrome::FindLastActive();
  if (!browser)
    return false;
  return browser->profile()->GetOriginalProfile()->GetPrefs()->GetBoolean(
      brave_vpn::prefs::kBraveVPNUserConfigLocked);
}

// Dummy function for code injection.
bool dummy(bool dummy = true) {
  return dummy;
}

}  // namespace

#define management_mode management_mode()));                                  \
  if (IsBlockedByBraveVPN()) {                                                \
    dict.Set(                                                                 \
        "managementMode",                                                     \
        static_cast<int>(SecureDnsConfig::ManagementMode::kDisabledManaged)); \
  }                                                                           \
dummy(dummy(dummy // NOLINT
#endif
#include "src/chrome/browser/ui/webui/settings/settings_secure_dns_handler.cc"
#if BUILDFLAG(IS_WIN)
#undef management_mode
#endif
