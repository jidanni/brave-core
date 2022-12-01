/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_vpn/dns/brave_vpn_dns_observer_service.h"

#include <unordered_map>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "brave/browser/brave_profile_prefs.h"
#include "brave/components/brave_vpn/brave_vpn_utils.h"
#include "brave/components/brave_vpn/pref_names.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/country_codes/country_codes.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/public/doh_provider_entry.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace brave_vpn {
namespace {
const char kCloudflareDnsProviderURL[] =
    "https://chrome.cloudflare-dns.com/dns-query";
}  // namespace

class BraveVpnDnsObserverServiceUnitTest : public testing::Test {
 public:
  BraveVpnDnsObserverServiceUnitTest() {}

  void SetUp() override {
    RegisterLocalState(local_state_.registry());
    RegisterUserProfilePrefs(profile_pref_service_.registry());
    brave_vpn::RegisterProfilePrefs(profile_pref_service_.registry());

    profile_pref_service_.registry()->RegisterBooleanPref(
        prefs::kBraveVPNShowDNSPolicyWarningDialog, true);
    dns_service_.reset(new BraveVpnDnsObserverService(
        local_state(), pref_service(),
        base::BindRepeating(
            &BraveVpnDnsObserverServiceUnitTest::ReadDNSPolicyValue,
            base::Unretained(this))));
    stub_resolver_config_reader_ =
        std::make_unique<StubResolverConfigReader>(&local_state_);
    SystemNetworkContextManager::set_stub_resolver_config_reader_for_testing(
        stub_resolver_config_reader_.get());
    SetPolicyValue(policy::key::kDnsOverHttpsMode, "");
    dns_service_->SetPrefServiceForTesting(pref_service());
    dns_service_->SkipNotificationDialogForTesting(true);
  }

  absl::optional<std::string> ReadDNSPolicyValue(const std::string& name) {
    EXPECT_TRUE(policy_map_.count(name));
    return policy_map_.at(name);
  }

  void TearDown() override {
    // BraveVpnDnsObserverService destructor must be called before the task
    // runner is destroyed.
    dns_service_.reset();
  }
  PrefService* local_state() { return &local_state_; }
  PrefService* pref_service() { return &profile_pref_service_; }

  void FireBraveVPNStateChange(mojom::ConnectionState state) {
    dns_service_->OnConnectionStateChanged(state);
  }

  bool WasPolicyNotificationShownForState(mojom::ConnectionState state) {
    bool callback_called = false;
    dns_service_->SetPolicyNotificationCallbackForTesting(
        base::BindLambdaForTesting([&]() { callback_called = true; }));
    FireBraveVPNStateChange(state);
    return callback_called;
  }

  void SetDNSMode(const std::string& mode, const std::string& doh_providers) {
    local_state()->SetString(::prefs::kDnsOverHttpsTemplates, doh_providers);
    local_state()->SetString(::prefs::kDnsOverHttpsMode, mode);
    SystemNetworkContextManager::GetStubResolverConfigReader()
        ->UpdateNetworkService(false);
  }

  void ExpectDNSMode(const std::string& mode,
                     const std::string& doh_providers) {
    auto dns_config = SystemNetworkContextManager::GetStubResolverConfigReader()
                          ->GetSecureDnsConfiguration(false);
    auto* current_mode = SecureDnsConfig::ModeToString(dns_config.mode());
    auto current_servers = dns_config.doh_servers().ToString();
    EXPECT_EQ(current_mode, mode);
    EXPECT_EQ(current_servers, doh_providers);
  }
  void SetPolicyValue(const std::string& name, const std::string& value) {
    policy_map_[name] = value;
  }

 private:
  std::unordered_map<std::string, std::string> policy_map_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BraveVpnDnsObserverService> dns_service_;
  sync_preferences::TestingPrefServiceSyncable profile_pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<StubResolverConfigReader> stub_resolver_config_reader_;
};

TEST_F(BraveVpnDnsObserverServiceUnitTest, AutoEnable) {
  // Browser DoH mode off -> override browser config and enable vpn
  local_state()->ClearPref(prefs::kBraveVpnDnsConfig);
  SetDNSMode(SecureDnsConfig::kModeOff, "");
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());

  // Browser DoH mode automatic -> override browser config and enable vpn
  SetDNSMode(SecureDnsConfig::kModeAutomatic, "");
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());

  // Browser DoH mode secure -> do not override browser config and enable vpn
  SetDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());

  std::string custom_servers =
      "https://server1.com\nhttps://server2.com/{?dns}";

  // Browser DoH mode automatic with custom servers
  // -> do not override browser config and enable vpn
  SetDNSMode(SecureDnsConfig::kModeAutomatic, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, custom_servers);
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());

  // Browser DoH mode automatic with broken custom servers
  // -> override browser config and enable vpn
  SetDNSMode(SecureDnsConfig::kModeAutomatic, std::string());
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, std::string());
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, std::string());
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());

  // Browser DoH mode secure with custom servers
  // -> do not override browser config and enable vpn
  SetDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::CONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTING);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  FireBraveVPNStateChange(mojom::ConnectionState::DISCONNECTED);
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_TRUE(local_state()->GetString(prefs::kBraveVpnDnsConfig).empty());
}

TEST_F(BraveVpnDnsObserverServiceUnitTest, DoHConfiguredByPolicy) {
  SetPolicyValue(policy::key::kDnsOverHttpsMode, SecureDnsConfig::kModeOff);

  SetDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_TRUE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_FALSE(WasPolicyNotificationShownForState(
      mojom::ConnectionState::DISCONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::DISCONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeOff, "");

  SetPolicyValue(policy::key::kDnsOverHttpsMode,
                 SecureDnsConfig::kModeAutomatic);

  SetDNSMode(SecureDnsConfig::kModeAutomatic, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");
  EXPECT_TRUE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");
  EXPECT_FALSE(WasPolicyNotificationShownForState(
      mojom::ConnectionState::DISCONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::DISCONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeAutomatic, "");

  SetPolicyValue(policy::key::kDnsOverHttpsMode, SecureDnsConfig::kModeSecure);
  SetDNSMode(SecureDnsConfig::kModeSecure, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(WasPolicyNotificationShownForState(
      mojom::ConnectionState::DISCONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, kCloudflareDnsProviderURL);
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::DISCONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, "");

  std::string custom_servers =
      "https://server1.com\nhttps://server2.com/{?dns}";
  SetDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(WasPolicyNotificationShownForState(
      mojom::ConnectionState::DISCONNECTING));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::DISCONNECTED));
  ExpectDNSMode(SecureDnsConfig::kModeSecure, custom_servers);

  // Do not show dialog option enabled
  SetPolicyValue(policy::key::kDnsOverHttpsMode, SecureDnsConfig::kModeOff);
  pref_service()->SetBoolean(prefs::kBraveVPNShowDNSPolicyWarningDialog, false);
  SetDNSMode(SecureDnsConfig::kModeOff, "");
  EXPECT_FALSE(
      WasPolicyNotificationShownForState(mojom::ConnectionState::CONNECTED));
}

}  // namespace brave_vpn
