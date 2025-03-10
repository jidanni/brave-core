/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/services/bat_ledger/bat_ledger_impl.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "brave/components/services/bat_ledger/bat_ledger_client_mojo_bridge.h"

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace bat_ledger {

BatLedgerImpl::BatLedgerImpl(
    mojo::PendingAssociatedRemote<mojom::BatLedgerClient> client_info)
  : bat_ledger_client_mojo_bridge_(
      new BatLedgerClientMojoBridge(std::move(client_info))),
    ledger_(
      ledger::Ledger::CreateInstance(bat_ledger_client_mojo_bridge_.get())) {
}

BatLedgerImpl::~BatLedgerImpl() = default;

void BatLedgerImpl::OnInitialize(CallbackHolder<InitializeCallback>* holder,
                                 ledger::mojom::Result result) {
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }
  delete holder;
}
void BatLedgerImpl::Initialize(
    const bool execute_create_script,
    InitializeCallback callback) {
  auto* holder = new CallbackHolder<InitializeCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->Initialize(
      execute_create_script,
      std::bind(BatLedgerImpl::OnInitialize, holder, _1));
}

void BatLedgerImpl::CreateRewardsWallet(const std::string& country,
                                        CreateRewardsWalletCallback callback) {
  ledger_->CreateRewardsWallet(country, std::move(callback));
}

void BatLedgerImpl::GetRewardsParameters(
    GetRewardsParametersCallback callback) {
  ledger_->GetRewardsParameters(std::move(callback));
}

void BatLedgerImpl::GetAutoContributeProperties(
    GetAutoContributePropertiesCallback callback) {
  ledger::mojom::AutoContributePropertiesPtr props =
      ledger_->GetAutoContributeProperties();
  if (!props) {
    props = ledger::mojom::AutoContributeProperties::New();
  }
  std::move(callback).Run(std::move(props));
}

void BatLedgerImpl::GetPublisherMinVisitTime(
    GetPublisherMinVisitTimeCallback callback) {
  std::move(callback).Run(ledger_->GetPublisherMinVisitTime());
}

void BatLedgerImpl::GetPublisherMinVisits(
    GetPublisherMinVisitsCallback callback) {
  std::move(callback).Run(ledger_->GetPublisherMinVisits());
}

void BatLedgerImpl::GetPublisherAllowNonVerified(
    GetPublisherAllowNonVerifiedCallback callback) {
  std::move(callback).Run(ledger_->GetPublisherAllowNonVerified());
}

void BatLedgerImpl::GetPublisherAllowVideos(
    GetPublisherAllowVideosCallback callback) {
  std::move(callback).Run(ledger_->GetPublisherAllowVideos());
}

void BatLedgerImpl::GetAutoContributeEnabled(
    GetAutoContributeEnabledCallback callback) {
  std::move(callback).Run(ledger_->GetAutoContributeEnabled());
}

void BatLedgerImpl::GetReconcileStamp(GetReconcileStampCallback callback) {
  std::move(callback).Run(ledger_->GetReconcileStamp());
}

void BatLedgerImpl::OnLoad(ledger::mojom::VisitDataPtr visit_data,
                           uint64_t current_time) {
  ledger_->OnLoad(std::move(visit_data), current_time);
}

void BatLedgerImpl::OnUnload(uint32_t tab_id, uint64_t current_time) {
  ledger_->OnUnload(tab_id, current_time);
}

void BatLedgerImpl::OnShow(uint32_t tab_id, uint64_t current_time) {
  ledger_->OnShow(tab_id, current_time);
}

void BatLedgerImpl::OnHide(uint32_t tab_id, uint64_t current_time) {
  ledger_->OnHide(tab_id, current_time);
}

void BatLedgerImpl::OnForeground(uint32_t tab_id, uint64_t current_time) {
  ledger_->OnForeground(tab_id, current_time);
}

void BatLedgerImpl::OnBackground(uint32_t tab_id, uint64_t current_time) {
  ledger_->OnBackground(tab_id, current_time);
}

void BatLedgerImpl::OnPostData(const std::string& url,
                               const std::string& first_party_url,
                               const std::string& referrer,
                               const std::string& post_data,
                               ledger::mojom::VisitDataPtr visit_data) {
  ledger_->OnPostData(
      url, first_party_url, referrer, post_data, std::move(visit_data));
}

void BatLedgerImpl::OnXHRLoad(
    uint32_t tab_id,
    const std::string& url,
    const base::flat_map<std::string, std::string>& parts,
    const std::string& first_party_url,
    const std::string& referrer,
    ledger::mojom::VisitDataPtr visit_data) {
  ledger_->OnXHRLoad(tab_id, url, parts, first_party_url, referrer,
                     std::move(visit_data));
}

void BatLedgerImpl::SetPublisherExclude(const std::string& publisher_key,
                                        ledger::mojom::PublisherExclude exclude,
                                        SetPublisherExcludeCallback callback) {
  ledger_->SetPublisherExclude(publisher_key, exclude, std::move(callback));
}

void BatLedgerImpl::RestorePublishers(RestorePublishersCallback callback) {
  ledger_->RestorePublishers(std::move(callback));
}

void BatLedgerImpl::FetchPromotions(
    FetchPromotionsCallback callback) {
  ledger_->FetchPromotions(std::move(callback));
}

void BatLedgerImpl::ClaimPromotion(
    const std::string& promotion_id,
    const std::string& payload,
    ClaimPromotionCallback callback) {
  ledger_->ClaimPromotion(promotion_id, payload, std::move(callback));
}

void BatLedgerImpl::AttestPromotion(
    const std::string& promotion_id,
    const std::string& solution,
    AttestPromotionCallback callback) {
  ledger_->AttestPromotion(promotion_id, solution, std::move(callback));
}

void BatLedgerImpl::SetPublisherMinVisitTime(int duration_in_seconds) {
  ledger_->SetPublisherMinVisitTime(duration_in_seconds);
}

void BatLedgerImpl::SetPublisherMinVisits(int visits) {
  ledger_->SetPublisherMinVisits(visits);
}

void BatLedgerImpl::SetPublisherAllowNonVerified(bool allow) {
  ledger_->SetPublisherAllowNonVerified(allow);
}

void BatLedgerImpl::SetPublisherAllowVideos(bool allow) {
  ledger_->SetPublisherAllowVideos(allow);
}

void BatLedgerImpl::SetAutoContributionAmount(double amount) {
  ledger_->SetAutoContributionAmount(amount);
}

void BatLedgerImpl::SetAutoContributeEnabled(bool enabled) {
  ledger_->SetAutoContributeEnabled(enabled);
}

// static
void BatLedgerImpl::OnGetBalanceReport(
    CallbackHolder<GetBalanceReportCallback>* holder,
    const ledger::mojom::Result result,
    ledger::mojom::BalanceReportInfoPtr report_info) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(result, std::move(report_info));
  delete holder;
}
void BatLedgerImpl::GetBalanceReport(const ledger::mojom::ActivityMonth month,
                                     const int32_t year,
                                     GetBalanceReportCallback callback) {
  auto* holder = new CallbackHolder<GetBalanceReportCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->GetBalanceReport(
      month,
      year,
      std::bind(BatLedgerImpl::OnGetBalanceReport, holder, _1, _2));
}

void BatLedgerImpl::GetPublisherActivityFromUrl(
    uint64_t window_id,
    ledger::mojom::VisitDataPtr visit_data,
    const std::string& publisher_blob) {
  ledger_->GetPublisherActivityFromUrl(
      window_id, std::move(visit_data), publisher_blob);
}

// static
void BatLedgerImpl::OnGetPublisherBanner(
    CallbackHolder<GetPublisherBannerCallback>* holder,
    ledger::mojom::PublisherBannerPtr banner) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(banner));
  delete holder;
}

void BatLedgerImpl::GetPublisherBanner(const std::string& publisher_id,
    GetPublisherBannerCallback callback) {
  // delete in OnGetPublisherBanner
  auto* holder = new CallbackHolder<GetPublisherBannerCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->GetPublisherBanner(publisher_id,
      std::bind(BatLedgerImpl::OnGetPublisherBanner, holder, _1));
}

void BatLedgerImpl::GetAutoContributionAmount(
    GetAutoContributionAmountCallback callback) {
  std::move(callback).Run(ledger_->GetAutoContributionAmount());
}

void BatLedgerImpl::OnOneTimeTip(CallbackHolder<OneTimeTipCallback>* holder,
                                 const ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(result);
  delete holder;
}

void BatLedgerImpl::OneTimeTip(
    const std::string& publisher_key,
    const double amount,
    OneTimeTipCallback callback) {
  // deleted in OnOneTimeTip
  auto* holder = new CallbackHolder<OneTimeTipCallback>(
    AsWeakPtr(), std::move(callback));
  ledger_->OneTimeTip(
      publisher_key,
      amount,
      std::bind(BatLedgerImpl::OnOneTimeTip, holder, _1));
}

// static
void BatLedgerImpl::OnRemoveRecurringTip(
    CallbackHolder<RemoveRecurringTipCallback>* holder,
    const ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }
  delete holder;
}

void BatLedgerImpl::RemoveRecurringTip(
  const std::string& publisher_key,
  RemoveRecurringTipCallback callback) {
  auto* holder = new CallbackHolder<RemoveRecurringTipCallback>(
            AsWeakPtr(), std::move(callback));
  ledger_->RemoveRecurringTip(
    publisher_key,
    std::bind(BatLedgerImpl::OnRemoveRecurringTip, holder, _1));
}

void BatLedgerImpl::GetCreationStamp(GetCreationStampCallback callback) {
  std::move(callback).Run(ledger_->GetCreationStamp());
}

void BatLedgerImpl::OnHasSufficientBalanceToReconcile(
    CallbackHolder<HasSufficientBalanceToReconcileCallback>* holder,
    bool sufficient) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(sufficient);
  }
  delete holder;
}

void BatLedgerImpl::HasSufficientBalanceToReconcile(
    HasSufficientBalanceToReconcileCallback callback) {
  auto* holder = new CallbackHolder<HasSufficientBalanceToReconcileCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->HasSufficientBalanceToReconcile(
      std::bind(BatLedgerImpl::OnHasSufficientBalanceToReconcile, holder, _1));
}

void BatLedgerImpl::OnGetRewardsInternalsInfo(
    CallbackHolder<GetRewardsInternalsInfoCallback>* holder,
    ledger::mojom::RewardsInternalsInfoPtr info) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(info));
  delete holder;
}

void BatLedgerImpl::GetRewardsInternalsInfo(
    GetRewardsInternalsInfoCallback callback) {
  auto* holder = new CallbackHolder<GetRewardsInternalsInfoCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetRewardsInternalsInfo(
      std::bind(BatLedgerImpl::OnGetRewardsInternalsInfo, holder, _1));
}

// static
void BatLedgerImpl::OnSaveRecurringTip(
    CallbackHolder<SaveRecurringTipCallback>* holder,
    const ledger::mojom::Result result) {
  if (holder->is_valid())
    std::move(holder->get()).Run(result);

  delete holder;
}

void BatLedgerImpl::SaveRecurringTip(ledger::mojom::RecurringTipPtr info,
                                     SaveRecurringTipCallback callback) {
  // deleted in OnSaveRecurringTip
  auto* holder = new CallbackHolder<SaveRecurringTipCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->SaveRecurringTip(std::move(info), std::bind(
      BatLedgerImpl::OnSaveRecurringTip, holder, _1));
}

// static
void BatLedgerImpl::OnGetRecurringTips(
    CallbackHolder<GetRecurringTipsCallback>* holder,
    std::vector<ledger::mojom::PublisherInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(list));

  delete holder;
}

void BatLedgerImpl::GetRecurringTips(GetRecurringTipsCallback callback) {
  auto* holder = new CallbackHolder<GetRecurringTipsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetRecurringTips(std::bind(
      BatLedgerImpl::OnGetRecurringTips, holder, _1));
}

// static
void BatLedgerImpl::OnGetOneTimeTips(
    CallbackHolder<GetRecurringTipsCallback>* holder,
    std::vector<ledger::mojom::PublisherInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(list));

  delete holder;
}

void BatLedgerImpl::GetOneTimeTips(GetOneTimeTipsCallback callback) {
  auto* holder = new CallbackHolder<GetOneTimeTipsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetOneTimeTips(std::bind(
      BatLedgerImpl::OnGetOneTimeTips, holder, _1));
}

// static
void BatLedgerImpl::OnGetActivityInfoList(
    CallbackHolder<GetActivityInfoListCallback>* holder,
    std::vector<ledger::mojom::PublisherInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(list));

  delete holder;
}

void BatLedgerImpl::GetActivityInfoList(
    uint32_t start,
    uint32_t limit,
    ledger::mojom::ActivityInfoFilterPtr filter,
    GetActivityInfoListCallback callback) {
  auto* holder = new CallbackHolder<GetActivityInfoListCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetActivityInfoList(
      start,
      limit,
      std::move(filter),
      std::bind(BatLedgerImpl::OnGetActivityInfoList, holder, _1));
}

// static
void BatLedgerImpl::OnGetExcludedList(
    CallbackHolder<GetExcludedListCallback>* holder,
    std::vector<ledger::mojom::PublisherInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(list));

  delete holder;
}

void BatLedgerImpl::GetExcludedList(
    GetExcludedListCallback callback) {
  auto* holder = new CallbackHolder<GetExcludedListCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetExcludedList(
      std::bind(BatLedgerImpl::OnGetExcludedList, holder, _1));
}

void BatLedgerImpl::UpdateMediaDuration(
    const uint64_t window_id,
    const std::string& publisher_key,
    const uint64_t duration,
    const bool first_visit) {
  ledger_->UpdateMediaDuration(window_id, publisher_key, duration, first_visit);
}

// static
void BatLedgerImpl::OnIsPublisherRegistered(
    CallbackHolder<IsPublisherRegisteredCallback>* holder,
    bool is_registered) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(is_registered);
  delete holder;
}

void BatLedgerImpl::IsPublisherRegistered(
    const std::string& publisher_id,
    IsPublisherRegisteredCallback callback) {
  auto* holder = new CallbackHolder<IsPublisherRegisteredCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->IsPublisherRegistered(
      publisher_id,
      std::bind(BatLedgerImpl::OnIsPublisherRegistered, holder, _1));
}

// static
void BatLedgerImpl::OnPublisherInfo(
    CallbackHolder<GetPublisherInfoCallback>* holder,
    const ledger::mojom::Result result,
    ledger::mojom::PublisherInfoPtr info) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(result, std::move(info));
  delete holder;
}

void BatLedgerImpl::GetPublisherInfo(
    const std::string& publisher_key,
    GetPublisherInfoCallback callback) {
  // deleted in OnPublisherInfo
  auto* holder = new CallbackHolder<GetPublisherInfoCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->GetPublisherInfo(
      publisher_key,
      std::bind(BatLedgerImpl::OnPublisherInfo, holder, _1, _2));
}

// static
void BatLedgerImpl::OnPublisherPanelInfo(
    CallbackHolder<GetPublisherPanelInfoCallback>* holder,
    const ledger::mojom::Result result,
    ledger::mojom::PublisherInfoPtr info) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(result, std::move(info));
  delete holder;
}

void BatLedgerImpl::GetPublisherPanelInfo(
    const std::string& publisher_key,
    GetPublisherPanelInfoCallback callback) {
  // deleted in OnPublisherPanelInfo
  auto* holder = new CallbackHolder<GetPublisherPanelInfoCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->GetPublisherPanelInfo(
      publisher_key,
      std::bind(BatLedgerImpl::OnPublisherPanelInfo, holder, _1, _2));
}

// static
void BatLedgerImpl::OnSavePublisherInfo(
    CallbackHolder<SavePublisherInfoCallback>* holder,
    const ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }

  delete holder;
}

void BatLedgerImpl::SavePublisherInfo(
    const uint64_t window_id,
    ledger::mojom::PublisherInfoPtr publisher_info,
    SavePublisherInfoCallback callback) {
  auto* holder = new CallbackHolder<SavePublisherInfoCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->SavePublisherInfo(
      window_id,
      std::move(publisher_info),
      std::bind(BatLedgerImpl::OnSavePublisherInfo,
          holder,
          _1));
}

void BatLedgerImpl::OnRefreshPublisher(
    CallbackHolder<RefreshPublisherCallback>* holder,
    ledger::mojom::PublisherStatus status) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(status);

  delete holder;
}

void BatLedgerImpl::RefreshPublisher(
    const std::string& publisher_key,
    RefreshPublisherCallback callback) {
  auto* holder = new CallbackHolder<RefreshPublisherCallback>(
      AsWeakPtr(), std::move(callback));
  ledger_->RefreshPublisher(
      publisher_key,
      std::bind(BatLedgerImpl::OnRefreshPublisher, holder, _1));
}

void BatLedgerImpl::StartMonthlyContribution() {
  ledger_->StartMonthlyContribution();
}

void BatLedgerImpl::SetInlineTippingPlatformEnabled(
    const ledger::mojom::InlineTipsPlatforms platform,
    bool enabled) {
  ledger_->SetInlineTippingPlatformEnabled(platform, enabled);
}

void BatLedgerImpl::GetInlineTippingPlatformEnabled(
    const ledger::mojom::InlineTipsPlatforms platform,
    GetInlineTippingPlatformEnabledCallback callback) {
  std::move(callback).Run(ledger_->GetInlineTippingPlatformEnabled(platform));
}

void BatLedgerImpl::GetShareURL(
    const base::flat_map<std::string, std::string>& args,
    GetShareURLCallback callback) {
  std::move(callback).Run(ledger_->GetShareURL(args));
}

// static
void BatLedgerImpl::OnGetPendingContributions(
    CallbackHolder<GetPendingContributionsCallback>* holder,
    std::vector<ledger::mojom::PendingContributionInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(list));
  }
  delete holder;
}

void BatLedgerImpl::GetPendingContributions(
    GetPendingContributionsCallback callback) {
  auto* holder = new CallbackHolder<GetPendingContributionsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetPendingContributions(std::bind(
      BatLedgerImpl::OnGetPendingContributions, holder, _1));
}

// static
void BatLedgerImpl::OnRemovePendingContribution(
    CallbackHolder<RemovePendingContributionCallback>* holder,
    ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }
  delete holder;
}

void BatLedgerImpl::RemovePendingContribution(
    const uint64_t id,
    RemovePendingContributionCallback callback) {
  auto* holder = new CallbackHolder<RemovePendingContributionCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->RemovePendingContribution(
      id,
      std::bind(BatLedgerImpl::OnRemovePendingContribution,
                holder,
                _1));
}

// static
void BatLedgerImpl::OnRemoveAllPendingContributions(
    CallbackHolder<RemovePendingContributionCallback>* holder,
    ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }
  delete holder;
}

void BatLedgerImpl::RemoveAllPendingContributions(
    RemovePendingContributionCallback callback) {
  auto* holder = new CallbackHolder<RemovePendingContributionCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->RemoveAllPendingContributions(
      std::bind(BatLedgerImpl::OnRemoveAllPendingContributions,
                holder,
                _1));
}

// static
void BatLedgerImpl::OnGetPendingContributionsTotal(
    CallbackHolder<GetPendingContributionsTotalCallback>* holder,
    double amount) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(amount);
  }
  delete holder;
}

void BatLedgerImpl::GetPendingContributionsTotal(
    GetPendingContributionsTotalCallback callback) {
  auto* holder = new CallbackHolder<GetPendingContributionsTotalCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetPendingContributionsTotal(
      std::bind(BatLedgerImpl::OnGetPendingContributionsTotal,
                holder,
                _1));
}

void BatLedgerImpl::FetchBalance(FetchBalanceCallback callback) {
  ledger_->FetchBalance(std::move(callback));
}

void BatLedgerImpl::GetExternalWallet(const std::string& wallet_type,
                                      GetExternalWalletCallback callback) {
  ledger_->GetExternalWallet(wallet_type, std::move(callback));
}

void BatLedgerImpl::ConnectExternalWallet(
    const std::string& wallet_type,
    const base::flat_map<std::string, std::string>& args,
    ConnectExternalWalletCallback callback) {
  ledger_->ConnectExternalWallet(wallet_type, args, std::move(callback));
}

// static
void BatLedgerImpl::OnDisconnectWallet(
    CallbackHolder<DisconnectWalletCallback>* holder,
    ledger::mojom::Result result) {
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }
  delete holder;
}

void BatLedgerImpl::DisconnectWallet(
    const std::string& wallet_type,
    DisconnectWalletCallback callback) {
  auto* holder = new CallbackHolder<DisconnectWalletCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->DisconnectWallet(
      wallet_type,
      std::bind(BatLedgerImpl::OnDisconnectWallet,
                holder,
                _1));
}

// static
void BatLedgerImpl::OnGetTransactionReport(
    CallbackHolder<GetTransactionReportCallback>* holder,
    std::vector<ledger::mojom::TransactionReportInfoPtr> list) {
  if (!holder) {
    return;
  }

  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(list));
  }
  delete holder;
}

void BatLedgerImpl::GetTransactionReport(
    const ledger::mojom::ActivityMonth month,
    const int year,
    GetTransactionReportCallback callback) {
  auto* holder = new CallbackHolder<GetTransactionReportCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetTransactionReport(
      month,
      year,
      std::bind(BatLedgerImpl::OnGetTransactionReport,
                holder,
                _1));
}

// static
void BatLedgerImpl::OnGetContributionReport(
    CallbackHolder<GetContributionReportCallback>* holder,
    std::vector<ledger::mojom::ContributionReportInfoPtr> list) {
  if (!holder) {
    return;
  }

  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(list));
  }
  delete holder;
}

void BatLedgerImpl::GetContributionReport(
    const ledger::mojom::ActivityMonth month,
    const int year,
    GetContributionReportCallback callback) {
  auto* holder = new CallbackHolder<GetContributionReportCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetContributionReport(
      month,
      year,
      std::bind(BatLedgerImpl::OnGetContributionReport,
                holder,
                _1));
}

// static
void BatLedgerImpl::OnGetAllContributions(
    CallbackHolder<GetAllContributionsCallback>* holder,
    std::vector<ledger::mojom::ContributionInfoPtr> list) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(std::move(list));

  delete holder;
}

void BatLedgerImpl::GetAllContributions(GetAllContributionsCallback callback) {
  auto* holder = new CallbackHolder<GetAllContributionsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetAllContributions(std::bind(
      BatLedgerImpl::OnGetAllContributions,
      holder,
      _1));
}

// static
void BatLedgerImpl::OnSavePublisherInfoForTip(
    CallbackHolder<SavePublisherInfoForTipCallback>* holder,
    const ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }

  delete holder;
}

void BatLedgerImpl::SavePublisherInfoForTip(
    ledger::mojom::PublisherInfoPtr info,
    SavePublisherInfoForTipCallback callback) {
  auto* holder = new CallbackHolder<SavePublisherInfoForTipCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->SavePublisherInfoForTip(
      std::move(info),
      std::bind(BatLedgerImpl::OnSavePublisherInfoForTip,
          holder,
          _1));
}

// static
void BatLedgerImpl::OnGetMonthlyReport(
    CallbackHolder<GetMonthlyReportCallback>* holder,
    const ledger::mojom::Result result,
    ledger::mojom::MonthlyReportInfoPtr info) {
  DCHECK(holder);
  if (holder->is_valid())
    std::move(holder->get()).Run(result, std::move(info));

  delete holder;
}

void BatLedgerImpl::GetMonthlyReport(const ledger::mojom::ActivityMonth month,
                                     const int year,
                                     GetMonthlyReportCallback callback) {
  auto* holder = new CallbackHolder<GetMonthlyReportCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetMonthlyReport(
      month,
      year,
      std::bind(BatLedgerImpl::OnGetMonthlyReport,
          holder,
          _1,
          _2));
}

// static
void BatLedgerImpl::OnGetAllMonthlyReportIds(
    CallbackHolder<GetAllMonthlyReportIdsCallback>* holder,
    const std::vector<std::string>& ids) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(ids);
  }

  delete holder;
}

void BatLedgerImpl::GetAllMonthlyReportIds(
    GetAllMonthlyReportIdsCallback callback) {
  auto* holder = new CallbackHolder<GetAllMonthlyReportIdsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetAllMonthlyReportIds(
      std::bind(BatLedgerImpl::OnGetAllMonthlyReportIds,
          holder,
          _1));
}

// static
void BatLedgerImpl::OnGetAllPromotions(
    CallbackHolder<GetAllPromotionsCallback>* holder,
    base::flat_map<std::string, ledger::mojom::PromotionPtr> items) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(items));
  }

  delete holder;
}

void BatLedgerImpl::GetAllPromotions(GetAllPromotionsCallback callback) {
  auto* holder = new CallbackHolder<GetAllPromotionsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetAllPromotions(
      std::bind(BatLedgerImpl::OnGetAllPromotions,
          holder,
          _1));
}

// static
void BatLedgerImpl::OnShutdown(CallbackHolder<ShutdownCallback>* holder,
                               const ledger::mojom::Result result) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(result);
  }

  delete holder;
}

void BatLedgerImpl::Shutdown(ShutdownCallback callback) {
  auto* holder = new CallbackHolder<ShutdownCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->Shutdown(
      std::bind(BatLedgerImpl::OnShutdown,
          holder,
          _1));
}

// static
void BatLedgerImpl::OnGetEventLogs(
    CallbackHolder<GetEventLogsCallback>* holder,
    std::vector<ledger::mojom::EventLogPtr> logs) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(logs));
  }

  delete holder;
}

void BatLedgerImpl::GetEventLogs(GetEventLogsCallback callback) {
  auto* holder = new CallbackHolder<GetEventLogsCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetEventLogs(
      std::bind(BatLedgerImpl::OnGetEventLogs,
          holder,
          _1));
}

// static
void BatLedgerImpl::OnGetRewardsWallet(
    CallbackHolder<GetRewardsWalletCallback>* holder,
    ledger::mojom::RewardsWalletPtr wallet) {
  DCHECK(holder);
  if (holder->is_valid()) {
    std::move(holder->get()).Run(std::move(wallet));
  }

  delete holder;
}

void BatLedgerImpl::GetRewardsWallet(GetRewardsWalletCallback callback) {
  auto* holder = new CallbackHolder<GetRewardsWalletCallback>(
      AsWeakPtr(), std::move(callback));

  ledger_->GetRewardsWallet(
      std::bind(BatLedgerImpl::OnGetRewardsWallet, holder, _1));
}

}  // namespace bat_ledger
