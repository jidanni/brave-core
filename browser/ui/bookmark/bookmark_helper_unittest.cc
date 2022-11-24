/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/bookmark/bookmark_helper.h"

#include "brave/browser/ui/bookmark/bookmark_prefs_service_factory.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BookmarkStateUnittest, SetState) {
  sync_preferences::TestingPrefServiceSyncable service;
  BookmarkPrefsServiceFactory::GetInstance()->RegisterProfilePrefs(
      service.registry());
  bookmarks::RegisterProfilePrefs(service.registry());
  // By default only NTP is enabled.
  EXPECT_EQ(brave::GetBookmarkBarState(&service), brave::BookmarkBarState::NTP);

  // Show bookmark bar always.
  brave::SetBookmarkState(brave::BookmarkBarState::ALWAYS, &service);
  EXPECT_EQ(brave::GetBookmarkBarState(&service),
            brave::BookmarkBarState::ALWAYS);

  // Never show bookmark bar.
  brave::SetBookmarkState(brave::BookmarkBarState::NEVER, &service);
  EXPECT_EQ(brave::GetBookmarkBarState(&service),
            brave::BookmarkBarState::NEVER);
}
