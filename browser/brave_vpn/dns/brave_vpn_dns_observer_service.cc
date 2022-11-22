/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_vpn/dns/brave_vpn_dns_observer_service.h"

#include <vector>

#include "base/bind.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "brave/browser/ui/views/brave_vpn/vpn_notification_dialog_view.h"
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
#include "components/country_codes/country_codes.h"
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
const char kBraveVpnDnsProvider[] = "Cloudflare";
const char kDohServersValue[] = "doh_servers";
const char kDohModeValue[] = "mode";

void SaveUserDNSConfig(PrefService* prefs, SecureDnsConfig& config) {
  base::Value::Dict user_dns_config;
  user_dns_config.Set(kDohModeValue,
                      SecureDnsConfig::ModeToString(config.mode()));
  user_dns_config.Set(kDohServersValue, config.doh_servers().ToString());
  prefs->SetDict(prefs::kBraveVPNUserConfig, std::move(user_dns_config));
}

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

std::string GetFilteredProvidersForCountry() {
  namespace secure_dns = chrome_browser_net::secure_dns;
  // Use default hardcoded servers for current country.
  auto providers = secure_dns::ProvidersForCountry(
      secure_dns::SelectEnabledProviders(net::DohProviderEntry::GetList()),
      country_codes::GetCurrentCountryID());
  for (const net::DohProviderEntry* entry : net::DohProviderEntry::GetList()) {
    if (entry->provider != kBraveVpnDnsProvider)
      continue;
    net::DnsOverHttpsConfig doh_config({entry->doh_server_config});
    return doh_config.ToString();
  }
  NOTREACHED() << "Should not be reached as we expect " << kBraveVpnDnsProvider
               << " is available in the default list.";
  return std::string("1.1.1.1");
}

std::string GetDoHServers(const std::string* user_servers) {
  return user_servers && !user_servers->empty()
             ? *user_servers
             : GetFilteredProvidersForCountry();
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
      profile_prefs_(profile_prefs),
      dns_config_service_(net::DnsConfigService::CreateSystemService()) {
  DCHECK(profile_prefs_);
  DCHECK(local_state_);
  dns_config_service_->WatchConfig(
      base::BindRepeating(&BraveVpnDnsObserverService::OnSystemDNSConfigChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      ::prefs::kDnsOverHttpsMode,
      base::BindRepeating(&BraveVpnDnsObserverService::OnDNSPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      ::prefs::kDnsOverHttpsTemplates,
      base::BindRepeating(&BraveVpnDnsObserverService::OnDNSPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

BraveVpnDnsObserverService::~BraveVpnDnsObserverService() = default;

void BraveVpnDnsObserverService::OnSystemDNSConfigChanged(
    const net::DnsConfig& config) {
  system_dns_config_ = config;
}

bool BraveVpnDnsObserverService::IsDNSSecure(
    SecureDnsConfig browser_dns_config) const {
  bool secure = (SecureDnsConfig::ModeToString(browser_dns_config.mode()) ==
                 SecureDnsConfig::kModeSecure);
  bool is_valid_automatic_mode =
      (SecureDnsConfig::ModeToString(browser_dns_config.mode()) ==
       SecureDnsConfig::kModeAutomatic) &&
      IsValidDoHTemplates(browser_dns_config.doh_servers().ToString());
  bool is_system_doh_enabled =
      (!system_dns_config_.doh_config.servers().empty() &&
       system_dns_config_.allow_dns_over_https_upgrade);
  return secure || is_valid_automatic_mode || is_system_doh_enabled;
}

bool BraveVpnDnsObserverService::ShouldAllowExternalChanges() const {
  if (allow_changes_for_testing_.has_value())
    return allow_changes_for_testing_.value();

  return IsDNSSecure(SystemNetworkContextManager::GetStubResolverConfigReader()
                         ->GetSecureDnsConfiguration(false)) ||
         (chrome::ShowQuestionMessageBoxSync(
              GetAnchorBrowserWindow(),
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
              l10n_util::GetStringUTF16(IDS_BRAVE_VPN_DNS_CHANGE_ALERT)) ==
          chrome::MESSAGE_BOX_RESULT_YES);
}

bool BraveVpnDnsObserverService::IsDnsModeConfiguredByPolicy() const {
  bool doh_policy_set =
      policy_reader_ &&
      !policy_reader_.Run(policy::key::kDnsOverHttpsMode).empty();
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

void BraveVpnDnsObserverService::ShowMessageWhyWeOverrideDNSSettings() {
  if (skip_notification_dialog_for_testing_)
    return;
  VpnNotificationDialogView::Show(chrome::FindLastActive());
}

void BraveVpnDnsObserverService::OnDNSPrefChanged() {
  if (ignore_prefs_change_)
    return;
  // Reset saved config and keep user's choice.
  if (!ShouldAllowExternalChanges()) {
    ignore_prefs_change_ = true;
    const auto& saved_dns_config =
        profile_prefs_->GetDict(prefs::kBraveVPNUserConfig);
    if (!saved_dns_config.empty()) {
      auto* servers_to_restore = saved_dns_config.FindString(kDohServersValue);
      SetDNSOverHTTPSMode(SecureDnsConfig::kModeSecure,
                          GetDoHServers(servers_to_restore));
    }
    ignore_prefs_change_ = false;
  }
}

void BraveVpnDnsObserverService::SetDNSOverHTTPSMode(
    const std::string& mode,
    const std::string& doh_providers) {
  local_state_->SetString(::prefs::kDnsOverHttpsTemplates, doh_providers);
  local_state_->SetString(::prefs::kDnsOverHttpsMode, mode);
}

void BraveVpnDnsObserverService::UnlockDNS() {
  profile_prefs_->SetBoolean(prefs::kBraveVPNUserConfigLocked, false);
  const auto& saved_dns_config =
      profile_prefs_->GetDict(prefs::kBraveVPNUserConfig);
  if (saved_dns_config.empty())
    return;
  auto* mode_to_restore = saved_dns_config.FindString(kDohModeValue);
  auto* servers_to_restore = saved_dns_config.FindString(kDohServersValue);
  if (mode_to_restore && servers_to_restore) {
    SetDNSOverHTTPSMode(*mode_to_restore, *servers_to_restore);
  }
  profile_prefs_->ClearPref(prefs::kBraveVPNUserConfig);
}

void BraveVpnDnsObserverService::LockDNS(const std::string& servers) {
  SetDNSOverHTTPSMode(SecureDnsConfig::kModeSecure, GetDoHServers(&servers));
  profile_prefs_->SetBoolean(prefs::kBraveVPNUserConfigLocked, true);
  ShowMessageWhyWeOverrideDNSSettings();
}

void BraveVpnDnsObserverService::OnConnectionStateChanged(
    brave_vpn::mojom::ConnectionState state) {
  if (state == brave_vpn::mojom::ConnectionState::CONNECTED) {
    ignore_prefs_change_ = false;
    auto dns_config = SystemNetworkContextManager::GetStubResolverConfigReader()
                          ->GetSecureDnsConfiguration(false);
    SaveUserDNSConfig(profile_prefs_, dns_config);
    if (!IsDNSSecure(SystemNetworkContextManager::GetStubResolverConfigReader()
                         ->GetSecureDnsConfiguration(false))) {
      // If DNS mode configured by policies we notify user that DNS may leak
      // via configured DNS gateway.
      if (IsDnsModeConfiguredByPolicy()) {
        ShowPolicyWarningMessage();
        return;
      }

      LockDNS(dns_config.doh_servers().ToString());
    }
  } else if (state == brave_vpn::mojom::ConnectionState::DISCONNECTED) {
    ignore_prefs_change_ = true;
    UnlockDNS();
  }
}

}  // namespace brave_vpn
