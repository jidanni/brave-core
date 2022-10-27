/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"

#include "brave/browser/ui/toolbar/brave_bookmark_context_menu_controller.h"
#include "ui/views/controls/menu/menu_model_adapter.h"

#define MenuModelAdapter                                                 \
  MenuModelAdapter dummy(controller_->GetBookmarkSubmenuModel());        \
  if (menu_model->GetCommandIdAt(i) == IDC_BRAVE_BOOKMARK_BAR_SUBMENU) { \
    auto* submenu_model = controller_->GetBookmarkSubmenuModel();        \
    auto command_id = menu_model->GetCommandIdAt(i);                     \
    auto* submenu =                                                      \
        menu_->AppendSubMenu(command_id, menu_model->GetLabelAt(i));     \
    for (size_t j = 0; j < submenu_model->GetItemCount(); ++j) {         \
      views::MenuModelAdapter::AppendMenuItemFromModel(                  \
          submenu_model, j, submenu, submenu_model->GetCommandIdAt(j));  \
    }                                                                    \
    continue;                                                            \
  }                                                                      \
  views::MenuModelAdapter
#define BookmarkContextMenuController BraveBookmarkContextMenuController
#include "src/chrome/browser/ui/views/bookmarks/bookmark_context_menu.cc"
#undef BookmarkContextMenuController
#undef MenuModelAdapter
