/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_BRAVE_VPN_DNS_BRAVE_VPN_DNS_OBSERVER_SERVICE_H_
#define BRAVE_BROWSER_BRAVE_VPN_DNS_BRAVE_VPN_DNS_OBSERVER_SERVICE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "brave/components/brave_vpn/brave_vpn_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_config_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SecureDnsConfig;
class PrefService;
namespace content {
class BrowserContext;
}  // namespace content

namespace policy {
class CloudPolicyStore;
}  // namespace policy

namespace brave_vpn {

class BraveVpnDnsObserverService : public brave_vpn::BraveVPNServiceObserver,
                                   public KeyedService {
 public:
  using DnsPolicyReaderCallback =
      base::RepeatingCallback<absl::optional<std::string>(const std::string&)>;

  explicit BraveVpnDnsObserverService(PrefService* local_state,
                                      PrefService* profile_prefs,
                                      DnsPolicyReaderCallback callback);
  ~BraveVpnDnsObserverService() override;
  BraveVpnDnsObserverService(const BraveVpnDnsObserverService&) = delete;
  BraveVpnDnsObserverService operator=(const BraveVpnDnsObserverService&) =
      delete;

  // brave_vpn::BraveVPNServiceObserver
  void OnConnectionStateChanged(
      brave_vpn::mojom::ConnectionState state) override;

  bool IsLocked() const;

  void SetPolicyNotificationCallbackForTesting(base::OnceClosure callback) {
    policy_callback_ = std::move(callback);
  }
  void SkipNotificationDialogForTesting(bool value) {
    skip_notification_dialog_for_testing_ = value;
  }
  void SetPrefServiceForTesting(PrefService* service) {
    pref_service_for_testing_ = service;
  }
  bool IsDnsModeConfiguredByPolicy() const;

 private:
  friend class BraveVpnDnsObserverServiceUnitTest;

  void LockDNS(const std::string& servers, bool show_notification);
  void UnlockDNS();
  void ShowPolicyWarningMessage();
  void ShowVpnNotificationDialog();

  base::OnceClosure policy_callback_;
  DnsPolicyReaderCallback policy_reader_;
  bool skip_notification_dialog_for_testing_ = false;
  raw_ptr<PrefService> local_state_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<PrefService> pref_service_for_testing_;
  base::WeakPtrFactory<BraveVpnDnsObserverService> weak_ptr_factory_{this};
};

}  // namespace brave_vpn

#endif  // BRAVE_BROWSER_BRAVE_VPN_DNS_BRAVE_VPN_DNS_OBSERVER_SERVICE_H_
