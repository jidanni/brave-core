/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_GEMINI_GEMINI_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_GEMINI_GEMINI_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/timer/timer.h"
#include "bat/ledger/internal/wallet_provider/connect_external_wallet.h"
#include "bat/ledger/internal/wallet_provider/get_external_wallet.h"
#include "bat/ledger/ledger.h"

namespace ledger {
class LedgerImpl;

namespace endpoint {
class GeminiServer;
}

namespace gemini {

struct Transaction {
  std::string address;
  double amount;
  std::string message;
};

class GeminiTransfer;

using FetchBalanceCallback = base::OnceCallback<void(mojom::Result, double)>;

class Gemini {
 public:
  explicit Gemini(LedgerImpl*);

  ~Gemini();

  void Initialize();

  void StartContribution(const std::string& contribution_id,
                         mojom::ServerPublisherInfoPtr info,
                         double amount,
                         ledger::LegacyResultCallback callback);

  void FetchBalance(FetchBalanceCallback callback);

  void TransferFunds(const double amount,
                     const std::string& address,
                     client::TransactionCallback callback);

  void ConnectWallet(const base::flat_map<std::string, std::string>& args,
                     ledger::ConnectExternalWalletCallback);

  void GetWallet(ledger::GetExternalWalletCallback);

  mojom::ExternalWalletPtr GetWallet();

  mojom::ExternalWalletPtr GetWalletIf(const std::set<mojom::WalletStatus>&);

  [[nodiscard]] bool SetWallet(mojom::ExternalWalletPtr);

  [[nodiscard]] bool DisconnectWallet(bool manual = false);

 private:
  void ContributionCompleted(mojom::Result result,
                             const std::string& transaction_id,
                             const std::string& contribution_id,
                             double fee,
                             const std::string& publisher_key,
                             ledger::LegacyResultCallback callback);

  void OnFetchBalance(FetchBalanceCallback, mojom::Result, double available);

  void SaveTransferFee(const std::string& contribution_id, double amount);

  void StartTransferFeeTimer(const std::string& fee_id, const int attempts);

  void OnTransferFeeCompleted(const mojom::Result result,
                              const std::string& transaction_id,
                              const std::string& contribution_id,
                              const int attempts);

  void TransferFee(const std::string& contribution_id,
                   const double amount,
                   const int attempts);

  void OnTransferFeeTimerElapsed(const std::string& id, const int attempts);

  void RemoveTransferFee(const std::string& contribution_id);

  std::unique_ptr<GeminiTransfer> transfer_;
  std::unique_ptr<wallet_provider::ConnectExternalWallet> connect_wallet_;
  std::unique_ptr<wallet_provider::GetExternalWallet> get_wallet_;
  std::unique_ptr<endpoint::GeminiServer> gemini_server_;
  LedgerImpl* ledger_;  // NOT OWNED
  std::map<std::string, base::OneShotTimer> transfer_fee_timers_;
};

}  // namespace gemini
}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_GEMINI_GEMINI_H_
