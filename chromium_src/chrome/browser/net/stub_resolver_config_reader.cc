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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_BRAVE_VPN)
#include "brave/components/brave_vpn/brave_vpn_utils.h"
#include "brave/components/brave_vpn/features.h"
#include "brave/components/brave_vpn/pref_names.h"

namespace {

void dummy(bool insecure_dns_client_enabled,
           ::net::SecureDnsMode secure_dns_mode,
           const ::net::DnsOverHttpsConfig& dns_over_https_config,
           bool additional_dns_types_enabled) {}

net::SecureDnsMode MaybeOverrideDnsMode(net::SecureDnsMode secure_dns_mode,
                                        PrefService* local_state) {
  if (!base::FeatureList::IsEnabled(
          brave_vpn::features::kBraveVPNDnsProtection)) {
    return secure_dns_mode;
  }

  auto value = brave_vpn::GetVPNDnsConfigMode(local_state);
  if (!value.has_value())
    return secure_dns_mode;
  auto mode = SecureDnsConfig::ParseMode(value.value());
  return mode.has_value() ? mode.value() : secure_dns_mode;
}

net::DnsOverHttpsConfig MaybeOverrideDohConfig(
    net::DnsOverHttpsConfig doh_config,
    PrefService* local_state) {
  if (!base::FeatureList::IsEnabled(
          brave_vpn::features::kBraveVPNDnsProtection)) {
    return doh_config;
  }

  auto servers = brave_vpn::GetVPNDnsConfigServers(local_state);
  if (!servers.has_value())
    return doh_config;
  return net::DnsOverHttpsConfig::FromStringLax(servers.value());
}

SecureDnsConfig::ManagementMode MaybeSetForcedManagementMode(
    SecureDnsConfig::ManagementMode management_mode,
    PrefService* local_state) {
  if (!base::FeatureList::IsEnabled(
          brave_vpn::features::kBraveVPNDnsProtection)) {
    return management_mode;
  }

  auto mode = brave_vpn::GetVPNDnsConfigMode(local_state);
  return mode.has_value() ? SecureDnsConfig::ManagementMode::kDisabledManaged
                          : management_mode;
}

}  // namespace

#define ConfigureStubHostResolver                                          \
  ConfigureStubHostResolver(                                               \
      GetInsecureStubResolverEnabled(),                                    \
      MaybeOverrideDnsMode(secure_dns_mode, local_state_),                 \
      MaybeOverrideDohConfig(doh_config, local_state_),                    \
      additional_dns_query_types_enabled);                                 \
  }                                                                        \
  return SecureDnsConfig(                                                  \
      MaybeOverrideDnsMode(secure_dns_mode, local_state_),                 \
      MaybeOverrideDohConfig(doh_config, local_state_),                    \
      MaybeSetForcedManagementMode(forced_management_mode, local_state_)); \
  if (0) {                                                                 \
  dummy
#endif
#include "src/chrome/browser/net/stub_resolver_config_reader.cc"
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_BRAVE_VPN)
#undef ConfigureStubHostResolver
#endif
