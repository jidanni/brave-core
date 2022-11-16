/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/components/constants/pref_names.h"
#include "components/bookmarks/common/bookmark_pref_names.h"

#define kShowBookmarkBar kShowBookmarkBar, kAlwaysShowBookmarkBarOnNTP
#include "src/chrome/browser/prefs/pref_service_incognito_allowlist.cc"
#undef kShowBookmarkBar
