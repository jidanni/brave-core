/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
#define BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_

#include "brave/browser/ui/toolbar/bookmark_bar_sub_menu_model.h"

#define Build                                                              \
  AddBraveBookmarksSubmenu(Profile* profile);                              \
  std::unique_ptr<BookmarkBarSubMenuModel> brave_bookmarks_submenu_model_; \
  void Build
#include "src/chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#undef Build

#endif  // BRAVE_CHROMIUM_SRC_CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
