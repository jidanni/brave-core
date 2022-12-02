/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_vpn/dns/brave_vpn_dns_observer_service.h"

#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "brave/browser/ui/views/brave_vpn/vpn_notification_dialog_view.h"
#include "brave/components/brave_vpn/brave_vpn_utils.h"
#include "brave/components/brave_vpn/pref_names.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "components/grit/brave_components_strings.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "net/dns/dns_config.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "ui/base/l10n/l10n_util.h"

namespace brave_vpn {

namespace {
const char kCloudflareDnsProviderURL[] =
    "https://chrome.cloudflare-dns.com/dns-query";

void SkipDNSDialog(PrefService* prefs, bool checked) {
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kBraveVPNShowDNSPolicyWarningDialog, !checked);
}

std::string GetDoHServers(const std::string* user_servers) {
  return user_servers && !user_servers->empty() ? *user_servers
                                                : kCloudflareDnsProviderURL;
}

gfx::NativeWindow GetAnchorBrowserWindow() {
  auto* browser = chrome::FindLastActive();
  return browser ? browser->window()->GetNativeWindow()
                 : gfx::kNullNativeWindow;
}

}  // namespace

BraveVpnDnsObserverService::BraveVpnDnsObserverService(
    PrefService* local_state,
    PrefService* profile_prefs)
    : local_state_(local_state), profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  DCHECK(local_state_);
}

BraveVpnDnsObserverService::~BraveVpnDnsObserverService() = default;

void BraveVpnDnsObserverService::ShowPolicyWarningMessage() {
  if (!profile_prefs_->GetBoolean(prefs::kBraveVPNShowDNSPolicyWarningDialog)) {
    return;
  }

  if (policy_callback_) {
    std::move(policy_callback_).Run();
    return;
  }

  chrome::ShowWarningMessageBoxWithCheckbox(
      GetAnchorBrowserWindow(), l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(IDS_BRAVE_VPN_DNS_POLICY_ALERT),
      l10n_util::GetStringUTF16(IDS_BRAVE_VPN_DNS_POLICY_CHECKBOX),
      base::BindOnce(&SkipDNSDialog, profile_prefs_));
}

void BraveVpnDnsObserverService::ShowVpnNotificationDialog() {
  if (skip_notification_dialog_for_testing_)
    return;
  VpnNotificationDialogView::Show(chrome::FindLastActive());
}

void BraveVpnDnsObserverService::UnlockDNS() {
  local_state_->ClearPref(brave_vpn::prefs::kBraveVpnDnsConfig);
  // Read DNS config to initiate update of actual state.
  SystemNetworkContextManager::GetStubResolverConfigReader()
      ->UpdateNetworkService(false);
}

bool BraveVpnDnsObserverService::IsLocked() const {
  return !local_state_->GetDict(prefs::kBraveVpnDnsConfig).empty();
}

void BraveVpnDnsObserverService::LockDNS(const std::string& servers,
                                         bool show_notification) {
  local_state_->SetString(brave_vpn::prefs::kBraveVpnDnsConfig,
                          GetDoHServers(&servers));
  // Read DNS config to initiate update of actual state.
  SystemNetworkContextManager::GetStubResolverConfigReader()
      ->UpdateNetworkService(false);
  if (show_notification)
    ShowVpnNotificationDialog();
}

void BraveVpnDnsObserverService::OnConnectionStateChanged(
    brave_vpn::mojom::ConnectionState state) {
  if (state == brave_vpn::mojom::ConnectionState::CONNECTED) {
    auto dns_config = SystemNetworkContextManager::GetStubResolverConfigReader()
                          ->GetSecureDnsConfiguration(false);
    // DNS config management_mode can be in kNoOverride state when policies set.
    // Checking both management_mode and managed pref.
    auto* doh_pref = local_state_->FindPreference(::prefs::kDnsOverHttpsMode);
    bool is_managed_pref = doh_pref && doh_pref->IsManaged();
    bool is_managed_mode = dns_config.management_mode() !=
                           SecureDnsConfig::ManagementMode::kNoOverride;
    bool is_managed = (is_managed_mode || is_managed_pref);
    if (dns_config.mode() != net::SecureDnsMode::kSecure && is_managed) {
      // If DNS mode configured by policies we notify user that DNS may leak
      // via configured DNS gateway.
      if (is_managed) {
        ShowPolicyWarningMessage();
        return;
      }
    }
    LockDNS(dns_config.doh_servers().ToString(), !is_managed);
  } else if (state == brave_vpn::mojom::ConnectionState::DISCONNECTED) {
    UnlockDNS();
  }
}

}  // namespace brave_vpn
