/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/webui/settings/settings_secure_dns_handler.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "brave/components/brave_vpn/buildflags/buildflags.h"
#include "build/buildflag.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_BRAVE_VPN)
#include "brave/components/brave_vpn/features.h"

#define kDnsOverHttpsTemplates kDnsOverHttpsTemplates, \
      base::BindRepeating( \
          &SecureDnsHandler::SendSecureDnsSettingUpdatesToJavascript, \
          base::Unretained(this)));                                   \
  if (!base::FeatureList::IsEnabled(                                  \
          brave_vpn::features::kBraveVPNDnsProtection)) {             \
    return;                                                           \
  }                                                                   \
  pref_registrar_.Add(prefs::kBraveVpnDnsConfig
#endif
#include "src/chrome/browser/ui/webui/settings/settings_secure_dns_handler.cc"
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_BRAVE_VPN)
#undef kDnsOverHttpsTemplates
#endif
