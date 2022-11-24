/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/toolbar/bookmark_bar_sub_menu_model.h"

#include "brave/browser/ui/bookmark/bookmark_helper.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/models/simple_menu_model.h"

BookmarkBarSubMenuModel::BookmarkBarSubMenuModel(Profile* profile)
    : SimpleMenuModel(this), profile_(profile) {
  Build();
}

BookmarkBarSubMenuModel::~BookmarkBarSubMenuModel() = default;

void BookmarkBarSubMenuModel::Build() {
  AddCheckItemWithStringId(IDC_BRAVE_BOOKMARK_BAR_ALWAYS,
                           IDS_BOOKMAR_BAR_MENU_SHOW_ALWAYS);

  AddCheckItemWithStringId(IDC_BRAVE_BOOKMARK_BAR_NEVER,
                           IDS_BOOKMAR_BAR_MENU_SHOW_NEVER);

  AddCheckItemWithStringId(IDC_BRAVE_BOOKMARK_BAR_NTP,
                           IDS_BOOKMAR_BAR_MENU_SHOW_NTP);
}

void BookmarkBarSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_BRAVE_BOOKMARK_BAR_ALWAYS:
      brave::SetBookmarkState(brave::BookmarkBarState::ALWAYS,
                              profile_->GetPrefs());
      return;
    case IDC_BRAVE_BOOKMARK_BAR_NEVER:
      brave::SetBookmarkState(brave::BookmarkBarState::NEVER,
                              profile_->GetPrefs());
      return;
    case IDC_BRAVE_BOOKMARK_BAR_NTP:
      brave::SetBookmarkState(brave::BookmarkBarState::NTP,
                              profile_->GetPrefs());
      return;
  }
}

bool BookmarkBarSubMenuModel::IsCommandIdChecked(int command_id) const {
  switch (brave::GetBookmarkBarState(profile_->GetPrefs())) {
    case brave::BookmarkBarState::ALWAYS:
      return command_id == IDC_BRAVE_BOOKMARK_BAR_ALWAYS;
    case brave::BookmarkBarState::NTP:
      return command_id == IDC_BRAVE_BOOKMARK_BAR_NTP;
    case brave::BookmarkBarState::NEVER:
      return command_id == IDC_BRAVE_BOOKMARK_BAR_NEVER;
  }
  return false;
}

bool BookmarkBarSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return (command_id == IDC_BRAVE_BOOKMARK_BAR_ALWAYS ||
          command_id == IDC_BRAVE_BOOKMARK_BAR_NEVER ||
          command_id == IDC_BRAVE_BOOKMARK_BAR_NTP);
}
