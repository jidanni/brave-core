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

namespace secure_dns = chrome_browser_net::secure_dns;

namespace brave_vpn {

namespace {
const char kCloudflareDnsProviderURL[] =
    "https://chrome.cloudflare-dns.com/dns-query";

void SkipDNSDialog(PrefService* prefs, bool checked) {
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kBraveVPNShowDNSPolicyWarningDialog, !checked);
}

bool IsValidDoHTemplates(const std::string& templates) {
  auto urls_whithout_templates = templates;
  base::ReplaceSubstringsAfterOffset(&urls_whithout_templates, 0, "{?dns}", "");
  std::vector<std::string> urls =
      base::SplitString(urls_whithout_templates, "\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const auto& it : urls) {
    if (GURL(it).is_valid())
      return true;
  }
  return false;
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
    PrefService* profile_prefs,
    DnsPolicyReaderCallback callback)
    : policy_reader_(std::move(callback)),
      local_state_(local_state),
      profile_prefs_(profile_prefs) {
  DCHECK(profile_prefs_);
  DCHECK(local_state_);
}

BraveVpnDnsObserverService::~BraveVpnDnsObserverService() = default;

bool BraveVpnDnsObserverService::IsDNSSecure(const std::string& mode,
                                             const std::string& servers) const {
  bool secure = (mode == SecureDnsConfig::kModeSecure);
  bool is_valid_automatic_mode =
      (mode == SecureDnsConfig::kModeAutomatic) && IsValidDoHTemplates(servers);
  return secure || is_valid_automatic_mode;
}

bool BraveVpnDnsObserverService::IsDnsModeConfiguredByPolicy() const {
  auto policy_value = (policy_reader_)
                          ? policy_reader_.Run(policy::key::kDnsOverHttpsMode)
                          : absl::nullopt;
  bool doh_policy_set =
      policy_value.has_value() && !policy_value.value().empty();
  return doh_policy_set ||
         SystemNetworkContextManager::GetStubResolverConfigReader()
             ->ShouldDisableDohForManaged();
}

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
  local_state_->ClearPref(prefs::kBraveVpnDnsConfig);
  // Read DNS config to initiate update of actual state.
  SystemNetworkContextManager::GetStubResolverConfigReader()
      ->UpdateNetworkService(false);
}

bool BraveVpnDnsObserverService::IsLocked() const {
  return !local_state_->GetDict(prefs::kBraveVpnDnsConfig).empty();
}

void BraveVpnDnsObserverService::LockDNS(const std::string& mode,
                                         const std::string& servers,
                                         bool show_notification) {
  brave_vpn::SetVpnDNSConfig(local_state_, mode, GetDoHServers(&servers));
  // Read DNS config to initiate update of actual state.
  SystemNetworkContextManager::GetStubResolverConfigReader()
      ->GetSecureDnsConfiguration(false);
  if (show_notification)
    ShowVpnNotificationDialog();
}

void BraveVpnDnsObserverService::OnConnectionStateChanged(
    brave_vpn::mojom::ConnectionState state) {
  if (state == brave_vpn::mojom::ConnectionState::CONNECTED) {
    auto dns_config = SystemNetworkContextManager::GetStubResolverConfigReader()
                          ->GetSecureDnsConfiguration(false);
    auto* current_mode = SecureDnsConfig::ModeToString(dns_config.mode());
    auto current_servers = dns_config.doh_servers().ToString();
    bool has_active_policy = IsDnsModeConfiguredByPolicy();
    bool is_dns_secure = IsDNSSecure(current_mode, current_servers);
    if (!is_dns_secure) {
      // If DNS mode configured by policies we notify user that DNS may leak
      // via configured DNS gateway.
      if (has_active_policy) {
        ShowPolicyWarningMessage();
        return;
      }
    }
    // We lock settings all time when vpn connected but override DNS only
    // when it is unsecure and not set by policy.
    auto mode_to_lock = (has_active_policy || is_dns_secure)
                            ? std::string(current_mode)
                            : SecureDnsConfig::kModeSecure;
    LockDNS(mode_to_lock, current_servers, !has_active_policy);
  } else if (state == brave_vpn::mojom::ConnectionState::DISCONNECTED) {
    UnlockDNS();
  }
}

}  // namespace brave_vpn
