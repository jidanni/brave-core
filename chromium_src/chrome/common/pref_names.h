/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CHROME_COMMON_PREF_NAMES_H_
#define BRAVE_CHROMIUM_SRC_CHROME_COMMON_PREF_NAMES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
namespace prefs {
extern const char kBraveVpnDnsConfig[];
}  // namespace prefs
#endif

#include "src/chrome/common/pref_names.h"

#endif  // BRAVE_CHROMIUM_SRC_CHROME_COMMON_PREF_NAMES_H_
