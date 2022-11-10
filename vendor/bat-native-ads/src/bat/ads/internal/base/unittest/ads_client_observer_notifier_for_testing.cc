/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/base/unittest/ads_client_observer_notifier_for_testing.h"

namespace ads {

namespace {}  // namespace

void AdsClientObserverNotifierForTesting::NotifyLocaleDidChange(
    const std::string& locale) {
  AdsClientObserverNotifier::NotifyLocaleDidChange(locale);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyPrefDidChange(
    const std::string& path) {
  AdsClientObserverNotifier::NotifyPrefDidChange(path);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyDidUpdateResourceComponent(
    const std::string& id) {
  AdsClientObserverNotifier::NotifyDidUpdateResourceComponent(id);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyTabTextContentDidChange(
    const int32_t tab_id,
    const std::vector<GURL>& redirect_chain,
    const std::string& text) {
  AdsClientObserverNotifier::NotifyTabTextContentDidChange(
      tab_id, redirect_chain, text);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyTabHtmlContentDidChange(
    const int32_t tab_id,
    const std::vector<GURL>& redirect_chain,
    const std::string& html) {
  AdsClientObserverNotifier::NotifyTabHtmlContentDidChange(
      tab_id, redirect_chain, html);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyTabDidStartPlayingMedia(
    const int32_t tab_id) {
  AdsClientObserverNotifier::NotifyTabDidStartPlayingMedia(tab_id);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyTabDidStopPlayingMedia(
    const int32_t tab_id) {
  AdsClientObserverNotifier::NotifyTabDidStopPlayingMedia(tab_id);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyTabDidChange(
    const int32_t tab_id,
    const std::vector<GURL>& redirect_chain,
    const bool is_visible,
    const bool is_incognito) {
  AdsClientObserverNotifier::NotifyTabDidChange(tab_id, redirect_chain,
                                                is_visible, is_incognito);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyDidCloseTab(
    const int32_t tab_id) {
  AdsClientObserverNotifier::NotifyDidCloseTab(tab_id);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyUserDidBecomeIdle() {
  AdsClientObserverNotifier::NotifyUserDidBecomeIdle();
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyUserDidBecomeActive(
    const base::TimeDelta idle_time,
    const bool screen_was_locked) {
  AdsClientObserverNotifier::NotifyUserDidBecomeActive(idle_time,
                                                       screen_was_locked);
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyBrowserDidEnterForeground() {
  AdsClientObserverNotifier::NotifyBrowserDidEnterForeground();
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyBrowserDidEnterBackground() {
  AdsClientObserverNotifier::NotifyBrowserDidEnterBackground();
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyBrowserDidBecomeActive() {
  AdsClientObserverNotifier::NotifyBrowserDidBecomeActive();
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyBrowserDidResignActive() {
  AdsClientObserverNotifier::NotifyBrowserDidResignActive();
  FlushObservers();
}

void AdsClientObserverNotifierForTesting::NotifyRewardsWalletDidChange(
    const std::string& payment_id,
    const std::string& recovery_seed) {
  AdsClientObserverNotifier::NotifyRewardsWalletDidChange(payment_id,
                                                          recovery_seed);
  FlushObservers();
}

///////////////////////////////////////////////////////////////////////////////

void AdsClientObserverNotifierForTesting::FlushObservers() {
  observers_.FlushForTesting();
}

}  // namespace ads
