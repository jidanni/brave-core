/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_BRAVE_VPN_VPN_NOTIFICATION_DIALOG_VIEW_H_
#define BRAVE_BROWSER_UI_VIEWS_BRAVE_VPN_VPN_NOTIFICATION_DIALOG_VIEW_H_

#include "ui/views/window/dialog_delegate.h"

class Browser;
class PrefService;

namespace views {
class Checkbox;
}

namespace brave_vpn {

class VpnNotificationDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(VpnNotificationDialogView);

  static void Show(Browser* browser);

  VpnNotificationDialogView(const VpnNotificationDialogView&) = delete;
  VpnNotificationDialogView& operator=(const VpnNotificationDialogView&) =
      delete;

 private:
  explicit VpnNotificationDialogView(Browser* browser);
  ~VpnNotificationDialogView() override;

  void OnAccept();
  void OnClosing();

  void OnLearnMoreLinkClicked();

  // views::DialogDelegate overrides:
  ui::ModalType GetModalType() const override;
  bool ShouldShowCloseButton() const override;
  bool ShouldShowWindowTitle() const override;

  bool close_window_ = true;
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<PrefService> prefs_ = nullptr;
  raw_ptr<views::Checkbox> dont_ask_again_checkbox_ = nullptr;
};

}  // namespace brave_vpn

#endif  // BRAVE_BROWSER_UI_VIEWS_BRAVE_VPN_VPN_NOTIFICATION_DIALOG_VIEW_H_
