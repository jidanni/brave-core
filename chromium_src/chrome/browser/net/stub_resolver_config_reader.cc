/* Copyright 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "brave/components/brave_vpn/buildflags/buildflags.h"
#include "build/buildflag.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "net/dns/public/dns_over_https_config.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(ENABLE_BRAVE_VPN) && BUILDFLAG(IS_WIN)
#include "brave/components/brave_vpn/pref_names.h"

namespace {

bool ShouldOverride(PrefService* local_state,
                    SecureDnsConfig::ManagementMode management_mode,
                    bool is_managed) {
  if (management_mode != SecureDnsConfig::ManagementMode::kNoOverride ||
      is_managed) {
    // there is already a managed policy or parental control in place
    return false;
  }
  auto* pref =
      local_state->FindPreference(brave_vpn::prefs::kBraveVpnDnsConfig);
  if (!pref)
    return false;
  auto* value = pref->GetValue();
  if (!value)
    return false;
  auto* servers_to_override = value->GetIfString();
  return servers_to_override && !servers_to_override->empty();
}

bool MaybeOverrideDnsClientEnabled(
    bool insecure_dns_client_enabled,
    PrefService* local_state,
    SecureDnsConfig::ManagementMode management_mode,
    bool is_managed) {
  if (ShouldOverride(local_state, management_mode, is_managed)) {
    // disable the insecure client for doh
    return false;
  }

  return insecure_dns_client_enabled;
}

net::SecureDnsMode MaybeOverrideDnsMode(
    net::SecureDnsMode secure_dns_mode,
    PrefService* local_state,
    SecureDnsConfig::ManagementMode management_mode,
    bool is_managed) {
  if (!ShouldOverride(local_state, management_mode, is_managed)) {
    return secure_dns_mode;
  }
  return net::SecureDnsMode::kSecure;
}

net::DnsOverHttpsConfig MaybeOverrideDnsConfig(
    net::DnsOverHttpsConfig doh_config,
    PrefService* local_state,
    SecureDnsConfig::ManagementMode management_mode,
    bool is_managed) {
  if (!ShouldOverride(local_state, management_mode, is_managed)) {
    return doh_config;
  }
  return net::DnsOverHttpsConfig::FromStringLax(
      local_state->GetString(brave_vpn::prefs::kBraveVpnDnsConfig));
}

SecureDnsConfig::ManagementMode MaybeOverrideForcedManagementMode(
    PrefService* local_state,
    SecureDnsConfig::ManagementMode management_mode,
    bool is_managed) {
  if (management_mode != SecureDnsConfig::ManagementMode::kNoOverride)
    return management_mode;

  if (!ShouldOverride(local_state, management_mode, is_managed)) {
    return management_mode;
  }
  return SecureDnsConfig::ManagementMode::kDisabledManaged;
}

}  // namespace

#define SecureDnsConfig(SECURE_DNS_MODE, SECURE_DOH_CONFIG,                   \
                        FORCED_MANAGEMENT_MODE)                               \
  SecureDnsConfig(MaybeOverrideDnsMode(SECURE_DNS_MODE, local_state_,         \
                                       FORCED_MANAGEMENT_MODE, is_managed),   \
                  MaybeOverrideDnsConfig(SECURE_DOH_CONFIG, local_state_,     \
                                         FORCED_MANAGEMENT_MODE, is_managed), \
                  MaybeOverrideForcedManagementMode(                          \
                      local_state_, FORCED_MANAGEMENT_MODE, is_managed))

#define ConfigureStubHostResolver(INSECURE_DNS_CLIENT_ENABLED,                 \
                                  SECURE_DNS_MODE, DNS_OVER_HTTPS_CONFIG,      \
                                  ADDITIONAL_DNS_TYPES_ENABLED)                \
  ConfigureStubHostResolver(                                                   \
      MaybeOverrideDnsClientEnabled(INSECURE_DNS_CLIENT_ENABLED, local_state_, \
                                    forced_management_mode, is_managed),       \
      MaybeOverrideDnsMode(SECURE_DNS_MODE, local_state_,                      \
                           forced_management_mode, is_managed),                \
      MaybeOverrideDnsConfig(DNS_OVER_HTTPS_CONFIG, local_state_,              \
                             forced_management_mode, is_managed),              \
      ADDITIONAL_DNS_TYPES_ENABLED)
#endif
#include "src/chrome/browser/net/stub_resolver_config_reader.cc"
#if BUILDFLAG(ENABLE_BRAVE_VPN) && BUILDFLAG(IS_WIN)
#undef ConfigureStubHostResolver
#undef SecureDnsConfig
#endif
