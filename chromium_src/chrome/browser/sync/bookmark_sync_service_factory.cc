/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/ui/bookmark/bookmark_prefs_service_factory.h"

// Adding BookmarkPrefsServiceFactory as dependency becasue kShowBookmarBar and
// kAlwaysShowBookmarkBarOnNTP manage bookmar bar state togher and need to
// register both prefs same time.
#define DependsOn                                        \
  DependsOn(BookmarkPrefsServiceFactory::GetInstance()); \
  DependsOn
#endif
#include "src/chrome/browser/sync/bookmark_sync_service_factory.cc"
#if !BUILDFLAG(IS_ANDROID)
#undef DependsOn
#endif
