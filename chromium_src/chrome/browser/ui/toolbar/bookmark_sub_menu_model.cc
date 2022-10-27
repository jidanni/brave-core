/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"

#include "brave/browser/ui/toolbar/bookmark_bar_sub_menu_model.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/simple_menu_model.h"

namespace {
constexpr int kChromiumBookmarkBarStringId = IDS_SHOW_BOOKMARK_BAR;
}  // namespace

#undef IDS_SHOW_BOOKMARK_BAR
#define IDS_SHOW_BOOKMARK_BAR kChromiumBookmarkBarStringId); \
  AddBraveBookmarksSubmenu(browser->profile()
#include "src/chrome/browser/ui/toolbar/bookmark_sub_menu_model.cc"
#undef IDS_SHOW_BOOKMARK_BAR
#define IDS_SHOW_BOOKMARK_BAR kChromiumBookmarkBarStringId

void BookmarkSubMenuModel::AddBraveBookmarksSubmenu(Profile* profile) {
  auto index = GetIndexOfCommandId(IDC_SHOW_BOOKMARK_BAR);
  if (!index.has_value())
    return;
  RemoveItemAt(index.value());
  brave_bookmarks_submenu_model_ =
      std::make_unique<BookmarkBarSubMenuModel>(profile);
  AddSubMenuWithStringId(IDC_BRAVE_BOOKMARK_BAR_SUBMENU,
                         kChromiumBookmarkBarStringId,
                         brave_bookmarks_submenu_model_.get());
}
