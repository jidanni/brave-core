/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/keyring_service.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_iterators.h"
#include "base/values.h"
#include "brave/components/brave_wallet/browser/brave_wallet_prefs.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/ethereum_keyring.h"
#include "brave/components/brave_wallet/browser/filecoin_keyring.h"
#include "brave/components/brave_wallet/browser/hd_keyring.h"
#include "brave/components/brave_wallet/browser/json_rpc_service.h"
#include "brave/components/brave_wallet/browser/pref_names.h"
#include "brave/components/brave_wallet/browser/solana_keyring.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom-forward.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom-shared.h"
#include "brave/components/brave_wallet/common/brave_wallet_constants.h"
#include "brave/components/brave_wallet/common/eth_address.h"
#include "brave/components/brave_wallet/common/hex_utils.h"
#include "brave/components/brave_wallet/common/solana_utils.h"
#include "brave/components/brave_wallet/common/switches.h"
#include "components/grit/brave_components_strings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "crypto/random.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

/* kBraveWalletKeyrings structure
 *
 * "filecoin":
 *   {
 *     "selected_account": "t1....ac",
 *     "imported_accounts": [
 *       {
 *           "account_address": "t3vmv....ughsa",
 *           "account_name": "Filecoin",
 *           "encrypted_private_key": "9/Xb...X4IL",
 *           "coin_type": 461 // Enum mojom::CoinType
 *       }
 *     ],
 *     "hardware":  {
 *        ...
 *     }
 *     "password_encryptor_nonce": "xxx"
 * },
 * "default":
 *   {
 *      "selected_account": "0xb3652763...cf3744911",
 *      "backup_complete": false,
 *      "encrypted_mnemonic": [mnemonic],
 *      "legacy_brave_wallet": false,
 *      "account_metas": {
 *         "m/44'/60'/0'/0/0": {
 *               "account_name": "account 1",
 *               ...
 *               "coin_type": 60 // Enum mojom::CoinType
 *          },
 *          "m/44'/60'/0'/0/1": {
 *               "account_name": "account 2",
 *               ...
 *          }
 *      },
 *     "hardware":  {
 *       "Ledger12445": {
 *         "account_metas": {
 *           "0xEA04...CC8Acc": {
 *             "account_name": "Ledger",
 *             "derivation_path": "m/44'/60'/1'/0/0",
 *             "hardware_vendor": "ledger",
 *             "coin_type": 60 // Enum mojom::CoinType
 *           },
 *           "0x264Ef...6b8F1": {
 *             "account_name": "Ledger",
 *             "derivation_path": "m/44'/60'/2'/0/0",
 *             "hardware_vendor": "ledger",
 *             "coin_type": 60 // Enum mojom::CoinType
 *            }
 *         },
 *         device_name: "Ledger 123"
 *       },
 *       "Ledger44332":{
 *         ...
 *       }
 *     },
 *      "imported_accounts": [
 *        { "address": "0x71f430f5f2a79274c17986ea1a1106596a39ba05",
 *          "encrypted_private_key": [privatekey],
 *          "account_name": "Imported account 1"
 *        },
 *        ...
 *      ],
 *      ...
 *   },
 *
 *   [keyringid]: {...}
 *   ...
 * }
 */

namespace brave_wallet {
namespace {
const size_t kSaltSize = 32;
const size_t kNonceSize = 12;
const int kPbkdf2IterationsLegacy = 100000;
const int kPbkdf2Iterations = 310000;
const int kPbkdf2KeySize = 256;
const char kRootPath[] = "m/44'/{coin}'";
const char kPasswordEncryptorSalt[] = "password_encryptor_salt";
const char kPasswordEncryptorNonce[] = "password_encryptor_nonce";
const char kEncryptedMnemonic[] = "encrypted_mnemonic";
const char kBackupComplete[] = "backup_complete";
const char kAccountMetas[] = "account_metas";
const char kAccountName[] = "account_name";
const char kHardwareVendor[] = "hardware_vendor";
const char kImportedAccounts[] = "imported_accounts";
const char kAccountAddress[] = "account_address";
const char kEncryptedPrivateKey[] = "encrypted_private_key";
const char kCoinType[] = "coin_type";
const char kLegacyBraveWallet[] = "legacy_brave_wallet";
const char kHardwareAccounts[] = "hardware";
const char kHardwareDerivationPath[] = "derivation_path";
const char kSelectedAccount[] = "selected_account";
const int kDiscoveryAttempts = 20;
const char kKeyringNotFound[] = "";

std::string GetRootPath(const std::string& keyring_id) {
  std::string root(kRootPath);
  auto coin = GetCoinForKeyring(keyring_id);
  auto coin_part = std::to_string(static_cast<int32_t>(coin));
  if (keyring_id == brave_wallet::mojom::kFilecoinTestnetKeyringId) {
    coin_part = "1";
  }
  base::ReplaceSubstringsAfterOffset(&root, 0, "{coin}", coin_part);
  if (coin == mojom::CoinType::ETH || coin == mojom::CoinType::FIL) {
    base::StrAppend(&root, {"/0'/0"});
  }
  return root;
}

static base::span<const uint8_t> ToSpan(base::StringPiece sp) {
  return base::as_bytes(base::make_span(sp));
}

std::string GetAccountName(size_t number) {
  return l10n_util::GetStringFUTF8(IDS_BRAVE_WALLET_NUMBERED_ACCOUNT_NAME,
                                   base::NumberToString16(number));
}

void SerializeHardwareAccounts(const std::string& device_id,
                               const base::Value* account_value,
                               const std::string& keyring_id,
                               std::vector<mojom::AccountInfoPtr>* accounts) {
  for (const auto account : account_value->GetDict()) {
    DCHECK(account.second.is_dict());
    std::string address = account.first;
    const base::Value::Dict& dict = account.second.GetDict();

    std::string hardware_vendor;
    const std::string* hardware_value = dict.FindString(kHardwareVendor);
    if (hardware_value)
      hardware_vendor = *hardware_value;

    std::string name;
    const std::string* name_value = dict.FindString(kAccountName);
    if (name_value)
      name = *name_value;

    std::string derivation_path;
    const std::string* derivation_path_value =
        dict.FindString(kHardwareDerivationPath);
    if (derivation_path_value)
      derivation_path = *derivation_path_value;

    mojom::CoinType coin = mojom::CoinType::ETH;
    auto coin_name_value = dict.FindInt(kCoinType);
    if (coin_name_value) {
      coin = static_cast<mojom::CoinType>(*coin_name_value);
    }

    accounts->push_back(mojom::AccountInfo::New(
        address, name, false,
        mojom::HardwareInfo::New(derivation_path, hardware_vendor, device_id),
        coin,
        IsFilecoinKeyringId(keyring_id)
            ? absl::optional<std::string>(keyring_id)
            : absl::nullopt));
  }
}

// TODO(apaymyshev): Need to use much lesser value for unit tests where this
// value is irrelevenat. Otherwise it takes too much time for tests to pass (44
// seconds for *KeryingService* on my machine).
int GetPbkdf2Iterations() {
  return KeyringService::GetPbkdf2IterationsForTesting().value_or(
      kPbkdf2Iterations);
}

const base::Value::List* GetPrefForKeyringList(const PrefService& prefs,
                                               const std::string& key,
                                               const std::string& id) {
  if (const base::Value* result =
          KeyringService::GetPrefForKeyring(prefs, key, id);
      result && result->is_list()) {
    return result->GetIfList();
  }
  return nullptr;
}

const base::Value::Dict* GetPrefForKeyringDict(const PrefService& prefs,
                                               const std::string& key,
                                               const std::string& id) {
  if (const base::Value* result =
          KeyringService::GetPrefForKeyring(prefs, key, id);
      result && result->is_dict()) {
    return result->GetIfDict();
  }
  return nullptr;
}

}  // namespace

KeyringService::KeyringService(JsonRpcService* json_rpc_service,
                               PrefService* prefs)
    : json_rpc_service_(json_rpc_service), prefs_(prefs) {
  DCHECK(prefs);
  auto_lock_timer_ = std::make_unique<base::OneShotTimer>();

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      kBraveWalletAutoLockMinutes,
      base::BindRepeating(&KeyringService::OnAutoLockPreferenceChanged,
                          base::Unretained(this)));

  MaybeUnlockWithCommandLine();
}

KeyringService::~KeyringService() {
  auto_lock_timer_.reset();
}

// static
absl::optional<int>& KeyringService::GetPbkdf2IterationsForTesting() {
  static absl::optional<int> iterations;
  return iterations;
}

mojo::PendingRemote<mojom::KeyringService> KeyringService::MakeRemote() {
  mojo::PendingRemote<mojom::KeyringService> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

KeyringService::ImportedAccountInfo::ImportedAccountInfo(
    const std::string& input_account_name,
    const std::string& input_account_address,
    const std::string& input_encrypted_private_key,
    mojom::CoinType input_coin)
    : account_name(input_account_name),
      account_address(input_account_address),
      encrypted_private_key(input_encrypted_private_key),
      coin(input_coin) {}

KeyringService::ImportedAccountInfo::~ImportedAccountInfo() = default;

KeyringService::ImportedAccountInfo::ImportedAccountInfo(
    const KeyringService::ImportedAccountInfo& other) {
  account_name = other.account_name;
  account_address = other.account_address;
  encrypted_private_key = other.encrypted_private_key;
  coin = other.coin;
}

void KeyringService::Bind(
    mojo::PendingReceiver<mojom::KeyringService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// static
absl::optional<std::string> KeyringService::GetKeyringIdForCoinNonFIL(
    mojom::CoinType coin) {
  if (coin == mojom::CoinType::FIL) {
    return absl::nullopt;
  } else if (coin == mojom::CoinType::SOL) {
    return mojom::kSolanaKeyringId;
  }
  DCHECK_EQ(coin, mojom::CoinType::ETH);
  return mojom::kDefaultKeyringId;
}

// static
void KeyringService::MigrateObsoleteProfilePrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(kBraveWalletSelectedAccount)) {
    SetPrefForKeyring(
        prefs, kSelectedAccount,
        base::Value(prefs->GetString(kBraveWalletSelectedAccount)),
        mojom::kDefaultKeyringId);
    prefs->ClearPref(kBraveWalletSelectedAccount);
  }

  // Moving hardware part under default keyring.
  DictionaryPrefUpdate update(prefs, kBraveWalletKeyrings);
  auto* obsolete = update->FindDictKey(kHardwareAccounts);
  if (obsolete) {
    SetPrefForKeyring(prefs, kHardwareAccounts, obsolete->Clone(),
                      mojom::kDefaultKeyringId);
    update->RemovePath(kHardwareAccounts);
  }
}

// static
bool KeyringService::HasPrefForKeyring(const PrefService& prefs,
                                       const std::string& key,
                                       const std::string& id) {
  return GetPrefForKeyring(prefs, key, id) != nullptr;
}

// static
std::vector<std::string> KeyringService::GetAvailableKeyringsFromPrefs(
    PrefService* prefs) {
  DCHECK(prefs);
  const auto& keyrings_pref = prefs->GetDict(kBraveWalletKeyrings);
  std::vector<std::string> keyrings;
  for (const auto&& [key, value] : keyrings_pref) {
    keyrings.push_back(key);
  }
  return keyrings;
}

// static
const base::Value* KeyringService::GetPrefForKeyring(const PrefService& prefs,
                                                     const std::string& key,
                                                     const std::string& id) {
  const auto& keyrings_pref = prefs.GetDict(kBraveWalletKeyrings);
  const base::Value::Dict* keyring_dict = keyrings_pref.FindDict(id);
  if (!keyring_dict)
    return nullptr;

  return keyring_dict->Find(key);
}

// static
base::Value::Dict& KeyringService::GetPrefForKeyringUpdate(
    PrefService* prefs,
    const std::string& key,
    const std::string& id) {
  DCHECK(prefs);
  DictionaryPrefUpdate update(prefs, kBraveWalletKeyrings);
  base::Value* keyrings_pref = update.Get();
  DCHECK(keyrings_pref);
  return *keyrings_pref->GetDict().EnsureDict(id)->EnsureDict(key);
}

// static
void KeyringService::SetPrefForKeyring(PrefService* prefs,
                                       const std::string& key,
                                       base::Value value,
                                       const std::string& id) {
  DCHECK(prefs);
  DictionaryPrefUpdate update(prefs, kBraveWalletKeyrings);
  base::Value* keyrings_pref = update.Get();
  DCHECK(keyrings_pref);
  keyrings_pref->GetDict().EnsureDict(id)->Set(key, std::move(value));
}

// static
void KeyringService::SetAccountMetaForKeyring(
    PrefService* prefs,
    const std::string& account_path,
    const absl::optional<std::string> name,
    const absl::optional<std::string> address,
    const std::string& id) {
  base::Value::Dict& account_metas =
      GetPrefForKeyringUpdate(prefs, kAccountMetas, id);

  base::Value::Dict* account_meta = account_metas.EnsureDict(account_path);
  if (name)
    account_meta->Set(kAccountName, *name);
  if (address)
    account_meta->Set(kAccountAddress, *address);
}

// static
std::string KeyringService::GetAccountNameForKeyring(
    const PrefService& prefs,
    const std::string& account_path,
    const std::string& id) {
  const base::Value::Dict* account_metas =
      GetPrefForKeyringDict(prefs, kAccountMetas, id);
  if (!account_metas)
    return std::string();

  const std::string* name = account_metas->FindStringByDottedPath(
      base::JoinString({account_path, kAccountName}, "."));
  if (!name)
    return std::string();

  return *name;
}

// static
std::string KeyringService::GetAccountAddressForKeyring(
    const PrefService& prefs,
    const std::string& account_path,
    const std::string& id) {
  const base::Value::Dict* account_metas =
      GetPrefForKeyringDict(prefs, kAccountMetas, id);
  if (!account_metas)
    return std::string();

  const std::string* address = account_metas->FindStringByDottedPath(
      base::JoinString({account_path, kAccountAddress}, "."));
  if (!address)
    return std::string();

  return *address;
}

// static
std::string KeyringService::GetAccountPathByIndex(
    size_t index,
    const std::string& keyring_id) {
  std::string path =
      base::StrCat({GetRootPath(keyring_id), "/", base::NumberToString(index)});
  auto coin = GetCoinForKeyring(keyring_id);
  if (coin == mojom::CoinType::SOL) {
    base::StrAppend(&path, {"'/0'"});
  }

  return path;
}

// static
void KeyringService::SetImportedAccountForKeyring(
    PrefService* prefs,
    const ImportedAccountInfo& info,
    const std::string& id) {
  DCHECK(prefs);
  base::Value::Dict imported_account;
  imported_account.Set(kAccountName, info.account_name);
  imported_account.Set(kAccountAddress, info.account_address);
  imported_account.Set(kEncryptedPrivateKey, info.encrypted_private_key);
  imported_account.Set(kCoinType, static_cast<int>(info.coin));

  base::Value::List imported_accounts;
  const base::Value::List* value =
      GetPrefForKeyringList(*prefs, kImportedAccounts, id);
  if (value)
    imported_accounts = value->Clone();
  imported_accounts.Append(std::move(imported_account));

  SetPrefForKeyring(prefs, kImportedAccounts,
                    base::Value(std::move(imported_accounts)), id);
}

// static
std::vector<KeyringService::ImportedAccountInfo>
KeyringService::GetImportedAccountsForKeyring(const PrefService& prefs,
                                              const std::string& id) {
  std::vector<ImportedAccountInfo> result;
  const base::Value::List* imported_accounts =
      GetPrefForKeyringList(prefs, kImportedAccounts, id);
  if (!imported_accounts)
    return result;
  for (const auto& item : *imported_accounts) {
    DCHECK(item.is_dict());
    const auto& imported_account = item.GetDict();
    const std::string* account_name = imported_account.FindString(kAccountName);
    const std::string* account_address =
        imported_account.FindString(kAccountAddress);
    const std::string* encrypted_private_key =
        imported_account.FindString(kEncryptedPrivateKey);
    if (!account_name || !account_address || !encrypted_private_key) {
      VLOG(0) << __func__ << "Imported accounts corruppted";
      continue;
    }
    mojom::CoinType coin = GetCoinForKeyring(id);
    auto coin_name_value = imported_account.FindInt(kCoinType);
    if (coin_name_value) {
      // TODO(cdesouza): Review removing this unsafe enum class type conversion.
      coin = static_cast<mojom::CoinType>(*coin_name_value);
    }

    result.emplace_back(*account_name, *account_address, *encrypted_private_key,
                        coin);
  }
  return result;
}
// static
void KeyringService::RemoveImportedAccountForKeyring(PrefService* prefs,
                                                     const std::string& address,
                                                     const std::string& id) {
  DCHECK(prefs);
  base::Value::List imported_accounts;
  const base::Value::List* value =
      GetPrefForKeyringList(*prefs, kImportedAccounts, id);
  if (!value)
    return;
  imported_accounts = value->Clone();
  for (const auto& imported_account : imported_accounts) {
    DCHECK(imported_account.is_dict());
    const std::string* account_address =
        imported_account.GetDict().FindString(kAccountAddress);
    if (account_address && *account_address == address) {
      imported_accounts.EraseValue(imported_account);
      break;
    }
  }

  SetPrefForKeyring(prefs, kImportedAccounts,
                    base::Value(std::move(imported_accounts)), id);
}

HDKeyring* KeyringService::CreateKeyring(const std::string& keyring_id,
                                         const std::string& password) {
  if (keyring_id != mojom::kDefaultKeyringId &&
      !IsFilecoinKeyringId(keyring_id) &&
      keyring_id != mojom::kSolanaKeyringId) {
    VLOG(1) << "Unknown keyring id " << keyring_id;
    return nullptr;
  }
  if (!CreateEncryptorForKeyring(password, keyring_id))
    return nullptr;

  const std::string mnemonic = GenerateMnemonic(16);
  if (!CreateKeyringInternal(keyring_id, mnemonic, false)) {
    return nullptr;
  }

  for (const auto& observer : observers_) {
    observer->KeyringCreated(keyring_id);
  }
  ResetAutoLockTimer();

  return GetHDKeyringById(keyring_id);
}

void KeyringService::RequestUnlock() {
  DCHECK(IsLocked(mojom::kDefaultKeyringId));
  request_unlock_pending_ = true;
}

HDKeyring* KeyringService::ResumeKeyring(const std::string& keyring_id,
                                         const std::string& password) {
  DCHECK(prefs_);
  if (!CreateEncryptorForKeyring(password, keyring_id)) {
    return nullptr;
  }

  const std::string mnemonic = GetMnemonicForKeyringImpl(keyring_id);
  bool is_legacy_brave_wallet = false;
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kLegacyBraveWallet, keyring_id);
  if (value)
    is_legacy_brave_wallet = value->GetBool();
  if (mnemonic.empty() ||
      !CreateKeyringInternal(keyring_id, mnemonic, is_legacy_brave_wallet)) {
    return nullptr;
  }

  auto* keyring = GetHDKeyringById(keyring_id);
  size_t account_no = GetAccountMetasNumberForKeyring(keyring_id);
  if (account_no)
    keyring->AddAccounts(account_no);

  // TODO(bbondy):
  // We can remove this some months after the initial wallet launch
  // We didn't store account address in meta pref originally.
  for (size_t i = 0; i < account_no; ++i) {
    SetAccountMetaForKeyring(prefs_, GetAccountPathByIndex(i, keyring_id),
                             absl::nullopt, keyring->GetAddress(i), keyring_id);
  }

  for (const auto& imported_account_info :
       GetImportedAccountsForKeyring(*prefs_, keyring_id)) {
    auto private_key_decoded =
        base::Base64Decode(imported_account_info.encrypted_private_key);
    if (!private_key_decoded)
      continue;

    auto private_key = encryptors_[keyring_id]->Decrypt(
        base::make_span(*private_key_decoded),
        GetOrCreateNonceForKeyring(keyring_id));
    if (!private_key)
      continue;

    if (IsFilecoinKeyringId(keyring_id)) {
      auto* filecoin_keyring = static_cast<FilecoinKeyring*>(keyring);
      if (filecoin_keyring) {
        filecoin_keyring->RestoreFilecoinAccount(
            *private_key, imported_account_info.account_address);
      }
    } else {
      keyring->ImportAccount(*private_key);
    }
  }

  return keyring;
}

HDKeyring* KeyringService::RestoreKeyring(const std::string& keyring_id,
                                          const std::string& mnemonic,
                                          const std::string& password,
                                          bool is_legacy_brave_wallet) {
  DCHECK(prefs_);
  if (!IsValidMnemonic(mnemonic))
    return nullptr;
  // Try getting existing mnemonic first
  if (CreateEncryptorForKeyring(password, keyring_id)) {
    const std::string current_mnemonic = GetMnemonicForKeyringImpl(keyring_id);
    // Restore with same mnmonic and same password, resume current keyring
    // Also need to make sure is_legacy_brave_wallet are the same, users might
    // choose the option wrongly and then want to start over with same mnemonic
    // but different is_legacy_brave_wallet value
    const base::Value* value =
        GetPrefForKeyring(*prefs_, kLegacyBraveWallet, keyring_id);
    if (!current_mnemonic.empty() && current_mnemonic == mnemonic && value &&
        value->GetBool() == is_legacy_brave_wallet) {
      return ResumeKeyring(keyring_id, password);
    } else if (keyring_id == mojom::kDefaultKeyringId) {
      // We have no way to check if new mnemonic is same as current mnemonic so
      // we need to clear all prefs for fresh start
      Reset(false);
      // Consider no migration needed after wallet is reset.
      prefs_->SetBoolean(kBraveWalletKeyringEncryptionKeysMigrated, true);
    }
  }

  if (!CreateEncryptorForKeyring(password, keyring_id)) {
    return nullptr;
  }

  // non default keyrings can only create encryptors for lazily keyring creation
  if (keyring_id != mojom::kDefaultKeyringId ||
      !CreateKeyringInternal(keyring_id, mnemonic, is_legacy_brave_wallet)) {
    return nullptr;
  }

  for (const auto& observer : observers_) {
    observer->KeyringRestored(keyring_id);
  }
  ResetAutoLockTimer();
  return GetHDKeyringById(keyring_id);
}

mojom::KeyringInfoPtr KeyringService::GetKeyringInfoSync(
    const std::string& keyring_id) {
  DCHECK(prefs_);
  mojom::KeyringInfoPtr keyring_info = mojom::KeyringInfo::New();
  keyring_info->id = keyring_id;
  keyring_info->is_keyring_created = IsKeyringCreated(keyring_id);
  keyring_info->is_locked = IsLocked(keyring_id);
  bool backup_complete = false;
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kBackupComplete, keyring_id);
  if (value)
    backup_complete = value->GetBool();
  keyring_info->is_backed_up = backup_complete;
  keyring_info->account_infos = GetAccountInfosForKeyring(keyring_id);
  return keyring_info;
}

void KeyringService::GetKeyringInfo(const std::string& keyring_id,
                                    GetKeyringInfoCallback callback) {
  std::move(callback).Run(GetKeyringInfoSync(keyring_id));
}

void KeyringService::GetKeyringsInfo(const std::vector<std::string>& keyrings,
                                     GetKeyringsInfoCallback callback) {
  std::vector<mojom::KeyringInfoPtr> result;
  for (const auto& keyring : keyrings) {
    result.push_back(GetKeyringInfoSync(keyring));
  }

  std::move(callback).Run(std::move(result));
}

void KeyringService::GetMnemonicForDefaultKeyring(
    const std::string& password,
    GetMnemonicForDefaultKeyringCallback callback) {
  if (!ValidatePasswordInternal(password)) {
    std::move(callback).Run("");
    return;
  }

  std::move(callback).Run(GetMnemonicForKeyringImpl(mojom::kDefaultKeyringId));
}

void KeyringService::MaybeCreateDefaultSolanaAccount() {
  if (ShouldCreateDefaultSolanaAccount() &&
      LazilyCreateKeyring(mojom::kSolanaKeyringId)) {
    auto address = AddAccountForKeyring(mojom::kSolanaKeyringId,
                                        "Solana " + GetAccountName(1));
    if (address) {
      SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                        mojom::kSolanaKeyringId);
      SetSelectedCoin(prefs_, mojom::CoinType::SOL);
      // This is needed for Android to select default coin, because they listen
      // to network change events.
      json_rpc_service_->SetNetwork(brave_wallet::mojom::kSolanaMainnet,
                                    mojom::CoinType::SOL, false);
    }
  }
}

void KeyringService::CreateWallet(const std::string& password,
                                  CreateWalletCallback callback) {
  prefs_->SetBoolean(kBraveWalletKeyringEncryptionKeysMigrated, true);

  auto* keyring = CreateKeyring(mojom::kDefaultKeyringId, password);
  if (keyring) {
    const auto address =
        AddAccountForKeyring(mojom::kDefaultKeyringId, GetAccountName(1));
    if (address) {
      SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                        mojom::kDefaultKeyringId);

      NotifyAccountsAdded(mojom::CoinType::ETH, {address.value()});
    }
  }

  // keep encryptor pre-created
  // to be able to lazily create keyring later
  if (IsFilecoinEnabled()) {
    if (!CreateEncryptorForKeyring(password, mojom::kFilecoinKeyringId)) {
      VLOG(1) << "Unable to create filecoin encryptor";
    }
    if (!CreateEncryptorForKeyring(password,
                                   mojom::kFilecoinTestnetKeyringId)) {
      VLOG(1) << "Unable to create filecoin testnet encryptor";
    }
  }
  if (IsSolanaEnabled()) {
    if (!CreateEncryptorForKeyring(password, mojom::kSolanaKeyringId)) {
      VLOG(1) << "Unable to create solana encryptor";
    }
    MaybeCreateDefaultSolanaAccount();
  }

  std::move(callback).Run(GetMnemonicForKeyringImpl(mojom::kDefaultKeyringId));
}

void KeyringService::RestoreWallet(const std::string& mnemonic,
                                   const std::string& password,
                                   bool is_legacy_brave_wallet,
                                   RestoreWalletCallback callback) {
  auto* keyring = RestoreKeyring(mojom::kDefaultKeyringId, mnemonic, password,
                                 is_legacy_brave_wallet);
  if (keyring && !keyring->GetAccountsNumber()) {
    const auto address =
        AddAccountForKeyring(mojom::kDefaultKeyringId, GetAccountName(1));
    if (address) {
      SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                        mojom::kDefaultKeyringId);
      NotifyAccountsAdded(mojom::CoinType::ETH, {address.value()});
    }
  }

  if (IsFilecoinEnabled()) {
    // Restore mainnet filecoin acc
    auto* filecoin_keyring = RestoreKeyring(mojom::kFilecoinKeyringId, mnemonic,
                                            password, is_legacy_brave_wallet);
    if (filecoin_keyring && !filecoin_keyring->GetAccountsNumber()) {
      auto address =
          AddAccountForKeyring(mojom::kFilecoinKeyringId, GetAccountName(1));
      if (address) {
        SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                          mojom::kFilecoinKeyringId);
      }
    }

    // Restore testnet filecoin acc
    auto* testnet_filecoin_keyring =
        RestoreKeyring(mojom::kFilecoinTestnetKeyringId, mnemonic, password,
                       is_legacy_brave_wallet);
    if (testnet_filecoin_keyring &&
        !testnet_filecoin_keyring->GetAccountsNumber()) {
      auto address = AddAccountForKeyring(mojom::kFilecoinTestnetKeyringId,
                                          GetAccountName(1));
      if (address) {
        SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                          mojom::kFilecoinTestnetKeyringId);
      }
    }
  }

  if (IsSolanaEnabled()) {
    auto* solana_keyring = RestoreKeyring(mojom::kSolanaKeyringId, mnemonic,
                                          password, is_legacy_brave_wallet);
    if (solana_keyring && !solana_keyring->GetAccountsNumber()) {
      auto address =
          AddAccountForKeyring(mojom::kSolanaKeyringId, GetAccountName(1));
      if (address) {
        SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(*address),
                          mojom::kSolanaKeyringId);
      }
    } else {
      MaybeCreateDefaultSolanaAccount();
    }
  }

  if (keyring) {
    discovery_weak_factory_.InvalidateWeakPtrs();
    // Start account discovery process. Consecutively look for accounts with at
    // least one transaction. Add such ones and all missing previous ones(so no
    // gaps). Stop discovering when there are 20 consecutive accounts with no
    // transactions.
    AddDiscoveryAccountsForKeyring(1, kDiscoveryAttempts);
  }

  std::move(callback).Run(keyring);
}

std::string KeyringService::GetMnemonicForKeyringImpl(
    const std::string& keyring_id) {
  if (IsLocked(keyring_id)) {
    VLOG(1) << __func__ << ": Must Unlock service first";
    return std::string();
  }
  DCHECK(encryptors_[keyring_id]);
  auto encrypted_mnemonic =
      GetPrefInBytesForKeyring(*prefs_, kEncryptedMnemonic, keyring_id);
  if (!encrypted_mnemonic)
    return std::string();

  auto mnemonic = encryptors_[keyring_id]->Decrypt(
      *encrypted_mnemonic, GetOrCreateNonceForKeyring(keyring_id));
  if (!mnemonic)
    return std::string();

  return std::string(mnemonic->begin(), mnemonic->end());
}

void KeyringService::AddFilecoinAccount(const std::string& account_name,
                                        const std::string& network,
                                        AddAccountCallback callback) {
  if (!IsFilecoinEnabled()) {
    std::move(callback).Run(false);
    return;
  }

  std::string keyring_id = GetFilecoinKeyringId(network);

  if (!LazilyCreateKeyring(keyring_id)) {
    VLOG(1) << "Unable to create Filecoin keyring";
    std::move(callback).Run(false);
    return;
  }

  auto* keyring = GetHDKeyringById(keyring_id);

  absl::optional<std::string> address;
  if (keyring) {
    address = AddAccountForKeyring(keyring_id, account_name);
  }

  if (address) {
    SetSelectedAccountForCoinSilently(mojom::CoinType::FIL, address.value());
    SetSelectedCoin(prefs_, mojom::CoinType::FIL);
  }

  NotifyAccountsChanged();

  std::move(callback).Run(keyring);
}

void KeyringService::AddAccount(const std::string& account_name,
                                mojom::CoinType coin,
                                AddAccountCallback callback) {
  auto keyring_id = GetKeyringIdForCoinNonFIL(coin);
  if (!keyring_id) {
    NOTREACHED() << "AddFilecoinAccount must be used";
    std::move(callback).Run(false);
    return;
  }
  if (*keyring_id == mojom::kSolanaKeyringId) {
    if (!IsSolanaEnabled()) {
      std::move(callback).Run(false);
      return;
    }
    if (!LazilyCreateKeyring(mojom::kSolanaKeyringId)) {
      VLOG(1) << "Unable to create Solana keyring";
      std::move(callback).Run(false);
      return;
    }
  }

  absl::optional<std::string> address;
  auto* keyring = GetHDKeyringById(*keyring_id);
  if (keyring) {
    address = AddAccountForKeyring(*keyring_id, account_name);
  }

  if (address) {
    SetSelectedAccountForCoinSilently(coin, address.value());
    SetSelectedCoin(prefs_, coin);
    NotifyAccountsAdded(coin, {address.value()});
  }

  NotifyAccountsChanged();
  std::move(callback).Run(keyring);
}

void KeyringService::GetPrivateKeyForKeyringAccount(
    const std::string& address,
    const std::string& password,
    mojom::CoinType coin,
    GetPrivateKeyForKeyringAccountCallback callback) {
  if (address.empty() || !ValidatePasswordInternal(password)) {
    std::move(callback).Run(false, "");
    return;
  }

  std::string keyring_id = GetKeyringId(coin, address);
  auto* keyring = GetHDKeyringById(keyring_id);
  if (!keyring) {
    std::move(callback).Run(false, "");
    return;
  }

  std::string private_key = keyring->GetEncodedPrivateKey(address);
  std::move(callback).Run(!private_key.empty(), private_key);
}

bool KeyringService::IsKeyringExist(const std::string& keyring_id) const {
  return keyrings_.contains(keyring_id) || IsKeyringCreated(keyring_id);
}

void KeyringService::ImportFilecoinAccount(
    const std::string& account_name,
    const std::string& private_key_hex,
    const std::string& network,
    ImportFilecoinAccountCallback callback) {
  const std::string filecoin_keyring_id = GetFilecoinKeyringId(network);
  if (!LazilyCreateKeyring(filecoin_keyring_id)) {
    std::move(callback).Run(false, "");
    VLOG(1) << "Unable to create Filecoin keyring";
    return;
  }

  if (account_name.empty() || private_key_hex.empty() ||
      !encryptors_[filecoin_keyring_id]) {
    std::move(callback).Run(false, "");
    return;
  }

  std::vector<uint8_t> private_key;
  mojom::FilecoinAddressProtocol protocol;
  if (!FilecoinKeyring::DecodeImportPayload(private_key_hex, &private_key,
                                            &protocol)) {
    std::move(callback).Run(false, "");
    return;
  }

  auto* keyring =
      static_cast<FilecoinKeyring*>(GetHDKeyringById(filecoin_keyring_id));

  if (!keyring) {
    std::move(callback).Run(false, "");
    return;
  }

  const std::string address =
      keyring->ImportFilecoinAccount(private_key, protocol);
  if (address.empty()) {
    std::move(callback).Run(false, "");
    return;
  }

  std::vector<uint8_t> encrypted_key =
      encryptors_[filecoin_keyring_id]->Encrypt(
          private_key, GetOrCreateNonceForKeyring(filecoin_keyring_id));

  ImportedAccountInfo info(account_name, address,
                           base::Base64Encode(encrypted_key),
                           mojom::CoinType::FIL);
  SetImportedAccountForKeyring(prefs_, info, filecoin_keyring_id);

  SetSelectedAccountForCoinSilently(mojom::CoinType::FIL, address);
  SetSelectedCoin(prefs_, mojom::CoinType::FIL);

  NotifyAccountsChanged();

  std::move(callback).Run(true, address);
}

void KeyringService::ImportAccount(const std::string& account_name,
                                   const std::string& private_key,
                                   mojom::CoinType coin,
                                   ImportAccountCallback callback) {
  std::string private_key_trimmed;
  base::TrimString(private_key, " \n\t", &private_key_trimmed);
  auto keyring_id = GetKeyringIdForCoinNonFIL(coin);

  if (!keyring_id) {
    NOTREACHED() << "ImportFilecoinAccount must be used";
    std::move(callback).Run(false, "");
    return;
  }

  if (account_name.empty() || private_key.empty() ||
      !encryptors_[*keyring_id]) {
    std::move(callback).Run(false, "");
    return;
  }

  std::vector<uint8_t> private_key_bytes;
  if (*keyring_id == mojom::kDefaultKeyringId) {
    if (!base::HexStringToBytes(private_key_trimmed, &private_key_bytes)) {
      // try again with 0x prefix considered
      if (!PrefixedHexStringToBytes(private_key_trimmed, &private_key_bytes)) {
        std::move(callback).Run(false, "");
        return;
      }
    }
  } else if (*keyring_id == mojom::kSolanaKeyringId) {
    if (!LazilyCreateKeyring(*keyring_id)) {
      VLOG(1) << "Unable to create Solana keyring";
      std::move(callback).Run(false, "");
      return;
    }
    std::vector<uint8_t> keypair(kSolanaKeypairSize);
    if (!Base58Decode(private_key_trimmed, &keypair, keypair.size())) {
      if (!Uint8ArrayDecode(private_key_trimmed, &keypair,
                            kSolanaKeypairSize)) {
        std::move(callback).Run(false, "");
        return;
      }
    }
    // extract private key from keypair
    private_key_bytes = std::move(keypair);
  }

  if (private_key_bytes.empty()) {
    std::move(callback).Run(false, "");
    return;
  }

  auto address =
      ImportAccountForKeyring(*keyring_id, account_name, private_key_bytes);

  if (!address) {
    std::move(callback).Run(false, "");
    return;
  }

  std::move(callback).Run(true, *address);
}

void KeyringService::ImportAccountFromJson(const std::string& account_name,
                                           const std::string& password,
                                           const std::string& json,
                                           ImportAccountCallback callback) {
  if (account_name.empty() || password.empty() || json.empty() ||
      !encryptors_[mojom::kDefaultKeyringId]) {
    std::move(callback).Run(false, "");
    return;
  }
  std::unique_ptr<HDKey> hd_key = HDKey::GenerateFromV3UTC(password, json);
  if (!hd_key) {
    std::move(callback).Run(false, "");
    return;
  }

  auto address = ImportAccountForKeyring(mojom::kDefaultKeyringId, account_name,
                                         hd_key->private_key());
  if (!address) {
    std::move(callback).Run(false, "");
    return;
  }

  std::move(callback).Run(true, *address);
}

absl::optional<std::string> KeyringService::FindImportedFilecoinKeyringId(
    const std::string& address) const {
  HDKeyring* mainnet_keyring = GetHDKeyringById(mojom::kFilecoinKeyringId);
  HDKeyring* testnet_keyring =
      GetHDKeyringById(mojom::kFilecoinTestnetKeyringId);

  if (mainnet_keyring && mainnet_keyring->HasImportedAddress(address)) {
    return mojom::kFilecoinKeyringId;
  }

  if (testnet_keyring && testnet_keyring->HasImportedAddress(address)) {
    return mojom::kFilecoinTestnetKeyringId;
  }

  return absl::nullopt;
}

absl::optional<std::string> KeyringService::FindBasicFilecoinKeyringId(
    const std::string& address) const {
  HDKeyring* mainnet_keyring = GetHDKeyringById(mojom::kFilecoinKeyringId);
  HDKeyring* testnet_keyring =
      GetHDKeyringById(mojom::kFilecoinTestnetKeyringId);
  if (mainnet_keyring && mainnet_keyring->HasAddress(address)) {
    return mojom::kFilecoinKeyringId;
  }
  if (testnet_keyring && testnet_keyring->HasAddress(address)) {
    return mojom::kFilecoinTestnetKeyringId;
  }
  return absl::nullopt;
}

absl::optional<std::string> KeyringService::FindHardwareFilecoinKeyringId(
    const std::string& address) const {
  for (const auto& hardware_account :
       GetHardwareAccountsSync(mojom::kFilecoinKeyringId)) {
    if (hardware_account && hardware_account.get()->address == address) {
      return mojom::kFilecoinKeyringId;
    }
  }
  for (const auto& hardware_account :
       GetHardwareAccountsSync(mojom::kFilecoinTestnetKeyringId)) {
    if (hardware_account && hardware_account.get()->address == address) {
      return mojom::kFilecoinTestnetKeyringId;
    }
  }
  return absl::nullopt;
}

absl::optional<std::string> KeyringService::FindFilecoinKeyringId(
    const std::string& address) const {
  auto imported = FindImportedFilecoinKeyringId(address);
  if (imported) {
    return imported;
  }

  auto basic = FindBasicFilecoinKeyringId(address);
  if (basic) {
    return basic;
  }

  auto hardware = FindHardwareFilecoinKeyringId(address);
  if (hardware) {
    return hardware;
  }

  return absl::nullopt;
}

std::string KeyringService::GetImportedKeyringId(
    mojom::CoinType coin_type,
    const std::string& address) const {
  return coin_type == mojom::CoinType::FIL
             ? FindImportedFilecoinKeyringId(address).value_or(kKeyringNotFound)
             : GetKeyringIdForCoinNonFIL(coin_type).value_or(kKeyringNotFound);
}

std::string KeyringService::GetHardwareKeyringId(
    mojom::CoinType coin_type,
    const std::string& address) const {
  return coin_type == mojom::CoinType::FIL
             ? FindHardwareFilecoinKeyringId(address).value_or(kKeyringNotFound)
             : GetKeyringIdForCoinNonFIL(coin_type).value_or(kKeyringNotFound);
}

std::string KeyringService::GetKeyringId(mojom::CoinType coin_type,
                                         const std::string& address) const {
  return coin_type == mojom::CoinType::FIL
             ? FindFilecoinKeyringId(address).value_or(kKeyringNotFound)
             : GetKeyringIdForCoinNonFIL(coin_type).value_or(kKeyringNotFound);
}

std::string KeyringService::GetKeyringIdForNetwork(
    mojom::CoinType coin_type,
    const std::string& network) const {
  return coin_type == mojom::CoinType::FIL
             ? (network == mojom::kFilecoinMainnet
                    ? mojom::kFilecoinKeyringId
                    : mojom::kFilecoinTestnetKeyringId)
             : GetKeyringIdForCoinNonFIL(coin_type).value_or(kKeyringNotFound);
}

HDKeyring* KeyringService::GetHDKeyringById(
    const std::string& keyring_id) const {
  if (keyrings_.contains(keyring_id))
    return keyrings_.at(keyring_id).get();
  return nullptr;
}

bool KeyringService::SetSelectedAccountForCoinSilently(
    mojom::CoinType coin,
    const std::string& address) {
  auto keyring_id = GetKeyringId(coin, address);

  if (keyring_id.empty()) {
    return false;
  }
  SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(address), keyring_id);
  if (coin == mojom::CoinType::FIL) {
    json_rpc_service_->SetNetwork(keyring_id == mojom::kFilecoinKeyringId
                                      ? mojom::kFilecoinMainnet
                                      : mojom::kFilecoinTestnet,
                                  coin, true /* silent */);
  }
  return true;
}

void KeyringService::SetSelectedAccountForCoin(mojom::CoinType coin,
                                               const std::string& address) {
  if (SetSelectedAccountForCoinSilently(coin, address)) {
    NotifySelectedAccountChanged(coin);
  }
}

void KeyringService::RemoveSelectedAccountForCoin(
    mojom::CoinType coin,
    const std::string& keyring_id) {
  SetPrefForKeyring(prefs_, kSelectedAccount, base::Value(std::string()),
                    keyring_id);
  NotifySelectedAccountChanged(coin);
}

void KeyringService::RemoveImportedAccount(
    const std::string& address,
    const std::string& password,
    mojom::CoinType coin,
    RemoveImportedAccountCallback callback) {
  if (address.empty() || !ValidatePasswordInternal(password)) {
    std::move(callback).Run(false);
    return;
  }
  const std::string keyring_id = GetImportedKeyringId(coin, address);

  auto* keyring = GetHDKeyringById(keyring_id);

  if (!keyring || !keyring->RemoveImportedAccount(address)) {
    std::move(callback).Run(false);
    return;
  }

  RemoveImportedAccountForKeyring(prefs_, address, keyring_id);
  NotifyAccountsChanged();
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kSelectedAccount, keyring_id);
  if (value && address == value->GetString()) {
    RemoveSelectedAccountForCoin(coin, keyring_id);
  }
  std::move(callback).Run(true);
}

void KeyringService::IsWalletBackedUp(IsWalletBackedUpCallback callback) {
  bool backup_complete = false;
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kBackupComplete, mojom::kDefaultKeyringId);
  if (value)
    backup_complete = value->GetBool();
  std::move(callback).Run(backup_complete);
}

void KeyringService::NotifyWalletBackupComplete() {
  SetPrefForKeyring(prefs_, kBackupComplete, base::Value(true),
                    mojom::kDefaultKeyringId);
  for (const auto& observer : observers_) {
    observer->BackedUp();
  }
}

absl::optional<std::string> KeyringService::AddAccountForKeyring(
    const std::string& keyring_id,
    const std::string& account_name) {
  auto* keyring = GetHDKeyringById(keyring_id);
  if (!keyring)
    return absl::nullopt;
  keyring->AddAccounts(1);
  size_t accounts_num = keyring->GetAccountsNumber();
  CHECK(accounts_num);
  SetAccountMetaForKeyring(
      prefs_, GetAccountPathByIndex(accounts_num - 1, keyring_id), account_name,
      keyring->GetAddress(accounts_num - 1), keyring_id);
  return keyring->GetAccounts().at(accounts_num - 1);
}

void KeyringService::AddDiscoveryAccountsForKeyring(
    size_t discovery_account_index,
    int attempts_left) {
  if (attempts_left <= 0)
    return;
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring)
    return;
  json_rpc_service_->GetEthTransactionCount(
      keyring->GetDiscoveryAddress(discovery_account_index),
      base::BindOnce(&KeyringService::OnGetTransactionCount,
                     discovery_weak_factory_.GetWeakPtr(),
                     discovery_account_index, attempts_left));
}

void KeyringService::OnGetTransactionCount(size_t discovery_account_index,
                                           int attempts_left,
                                           uint256_t result,
                                           mojom::ProviderError error,
                                           const std::string& error_message) {
  if (error != mojom::ProviderError::kSuccess)
    return;

  if (result > 0) {
    auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
    if (!keyring)
      return;
    DCHECK_GT(keyring->GetAccountsNumber(), 0u);
    size_t last_account_index = keyring->GetAccountsNumber() - 1;
    if (discovery_account_index > last_account_index) {
      AddAccountsWithDefaultName(discovery_account_index - last_account_index);
      NotifyAccountsChanged();
    }

    AddDiscoveryAccountsForKeyring(discovery_account_index + 1,
                                   kDiscoveryAttempts);
  } else {
    AddDiscoveryAccountsForKeyring(discovery_account_index + 1,
                                   attempts_left - 1);
  }
}

absl::optional<std::string> KeyringService::ImportAccountForKeyring(
    const std::string& keyring_id,
    const std::string& account_name,
    const std::vector<uint8_t>& private_key) {
  auto* keyring = GetHDKeyringById(keyring_id);
  if (!keyring) {
    return absl::nullopt;
  }

  const std::string address = keyring->ImportAccount(private_key);
  if (address.empty()) {
    return absl::nullopt;
  }
  std::vector<uint8_t> encrypted_private_key = encryptors_[keyring_id]->Encrypt(
      private_key, GetOrCreateNonceForKeyring(keyring_id));
  ImportedAccountInfo info(account_name, address,
                           base::Base64Encode(encrypted_private_key),
                           GetCoinForKeyring(keyring_id));
  SetImportedAccountForKeyring(prefs_, info, keyring_id);

  SetSelectedAccountForCoinSilently(GetCoinForKeyring(keyring_id), address);
  SetSelectedCoin(prefs_, GetCoinForKeyring(keyring_id));

  NotifyAccountsChanged();
  NotifyAccountsAdded(info.coin, {address});
  return address;
}

size_t KeyringService::GetAccountMetasNumberForKeyring(
    const std::string& id) const {
  const base::Value::Dict* account_metas =
      GetPrefForKeyringDict(*prefs_, kAccountMetas, id);
  if (!account_metas)
    return 0;

  return account_metas->size();
}

// This member function should not assume that the wallet is unlocked!
std::vector<mojom::AccountInfoPtr> KeyringService::GetAccountInfosForKeyring(
    const std::string& keyring_id) const {
  std::vector<mojom::AccountInfoPtr> result;

  size_t account_no = GetAccountMetasNumberForKeyring(keyring_id);
  for (size_t i = 0; i < account_no; ++i) {
    mojom::AccountInfoPtr account_info = mojom::AccountInfo::New();
    account_info->address = GetAccountAddressForKeyring(
        *prefs_, GetAccountPathByIndex(i, keyring_id), keyring_id);
    account_info->name = GetAccountNameForKeyring(
        *prefs_, GetAccountPathByIndex(i, keyring_id), keyring_id);
    account_info->is_imported = false;
    account_info->coin = GetCoinForKeyring(keyring_id);
    account_info->keyring_id = IsFilecoinKeyringId(keyring_id)
                                   ? absl::optional<std::string>(keyring_id)
                                   : absl::nullopt;
    result.push_back(std::move(account_info));
  }
  // append imported account info
  for (const auto& imported_account_info :
       GetImportedAccountsForKeyring(*prefs_, keyring_id)) {
    mojom::AccountInfoPtr account_info = mojom::AccountInfo::New();
    account_info->address = imported_account_info.account_address;
    account_info->name = imported_account_info.account_name;
    account_info->is_imported = true;
    account_info->coin = imported_account_info.coin;
    account_info->keyring_id = IsFilecoinKeyringId(keyring_id)
                                   ? absl::optional<std::string>(keyring_id)
                                   : absl::nullopt;
    result.push_back(std::move(account_info));
  }

  // append hardware accounts info
  for (const auto& hardware_account_info :
       GetHardwareAccountsSync(keyring_id)) {
    result.push_back(hardware_account_info.Clone());
  }
  return result;
}

std::vector<mojom::AccountInfoPtr> KeyringService::GetHardwareAccountsSync(
    const std::string& keyring_id) const {
  std::vector<mojom::AccountInfoPtr> accounts;
  const base::Value::Dict* keyring =
      GetPrefForKeyringDict(*prefs_, kHardwareAccounts, keyring_id);
  if (!keyring) {
    return accounts;
  }

  for (auto&& [id, value] : *keyring) {
    DCHECK(value.is_dict());
    const base::Value* account_value = value.GetDict().Find(kAccountMetas);
    if (!account_value)
      continue;
    SerializeHardwareAccounts(id, account_value, keyring_id, &accounts);
  }

  return accounts;
}

void KeyringService::AddHardwareAccounts(
    std::vector<mojom::HardwareWalletAccountPtr> infos) {
  if (infos.empty())
    return;

  bool account_selected = false;
  std::vector<std::string> addresses;
  for (const auto& info : infos) {
    const auto& hardware_vendor = info->hardware_vendor;
    std::string device_id = info->device_id;

    DCHECK_EQ(hardware_vendor, info->hardware_vendor);
    if (hardware_vendor != info->hardware_vendor)
      continue;
    base::Value::Dict hw_account;
    hw_account.Set(kAccountName, info->name);
    hw_account.Set(kHardwareVendor, info->hardware_vendor);
    hw_account.Set(kHardwareDerivationPath, info->derivation_path);
    hw_account.Set(kCoinType, static_cast<int>(info->coin));
    auto keyring_id =
        GetKeyringIdForNetwork(info->coin, info->network.value_or(""));

    base::Value::Dict& hardware_keyrings =
        GetPrefForKeyringUpdate(prefs_, kHardwareAccounts, keyring_id);

    hardware_keyrings.EnsureDict(device_id)
        ->EnsureDict(kAccountMetas)
        ->Set(info->address, std::move(hw_account));
    addresses.push_back(info->address);

    if (!account_selected) {
      SetSelectedAccountForCoinSilently(infos[0]->coin, infos[0]->address);
      SetSelectedCoin(prefs_, infos[0]->coin);
      account_selected = true;
    }
  }
  NotifyAccountsChanged();
  NotifyAccountsAdded(infos[0]->coin, addresses);
}

void KeyringService::RemoveHardwareAccount(
    const std::string& address,
    const std::string& password,
    mojom::CoinType coin,
    RemoveHardwareAccountCallback callback) {
  if (address.empty() || !ValidatePasswordInternal(password)) {
    std::move(callback).Run(false);
    return;
  }

  auto keyring_id = GetHardwareKeyringId(coin, address);
  base::Value::Dict& hardware_keyrings =
      GetPrefForKeyringUpdate(prefs_, kHardwareAccounts, keyring_id);
  for (auto&& [id, device] : hardware_keyrings) {
    DCHECK(device.is_dict());
    base::Value::Dict* account_metas = device.GetDict().FindDict(kAccountMetas);
    if (!account_metas)
      continue;
    const base::Value* address_key = account_metas->Find(address);
    if (!address_key)
      continue;
    account_metas->Remove(address);

    if (account_metas->empty())
      hardware_keyrings.Remove(id);

    NotifyAccountsChanged();
    const base::Value* pref =
        GetPrefForKeyring(*prefs_, kSelectedAccount, keyring_id);
    if (pref && address == pref->GetString()) {
      RemoveSelectedAccountForCoin(coin, keyring_id);
    }
    std::move(callback).Run(true);
    return;
  }

  std::move(callback).Run(false);
}

absl::optional<std::string> KeyringService::SignTransactionByFilecoinKeyring(
    FilTransaction* tx) {
  if (!tx)
    return absl::nullopt;

  std::string keyring_id = GetFilecoinKeyringId(tx->from().network());
  auto* keyring = GetHDKeyringById(keyring_id);
  if (!keyring)
    return absl::nullopt;
  return static_cast<FilecoinKeyring*>(keyring)->SignTransaction(tx);
}

void KeyringService::SignTransactionByDefaultKeyring(const std::string& address,
                                                     EthTransaction* tx,
                                                     uint256_t chain_id) {
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring)
    return;
  static_cast<EthereumKeyring*>(keyring)->SignTransaction(address, tx,
                                                          chain_id);
}

KeyringService::SignatureWithError::SignatureWithError() = default;
KeyringService::SignatureWithError::SignatureWithError(
    SignatureWithError&& other) = default;
KeyringService::SignatureWithError&
KeyringService::SignatureWithError::operator=(SignatureWithError&& other) =
    default;
KeyringService::SignatureWithError::~SignatureWithError() = default;

KeyringService::SignatureWithError KeyringService::SignMessageByDefaultKeyring(
    const std::string& address,
    const std::vector<uint8_t>& message,
    bool is_eip712) {
  SignatureWithError ret;
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring) {
    ret.signature = absl::nullopt;
    ret.error_message =
        l10n_util::GetStringUTF8(IDS_BRAVE_WALLET_SIGN_MESSAGE_UNLOCK_FIRST);
    return ret;
  }

  // MM currently doesn't provide chain_id when signing message
  std::vector<uint8_t> signature =
      static_cast<EthereumKeyring*>(keyring)->SignMessage(address, message, 0,
                                                          is_eip712);
  if (signature.empty()) {
    ret.signature = absl::nullopt;
    ret.error_message =
        l10n_util::GetStringFUTF8(IDS_BRAVE_WALLET_SIGN_MESSAGE_INVALID_ADDRESS,
                                  base::ASCIIToUTF16(address));
    return ret;
  }
  ret.signature = std::move(signature);
  return ret;
}

bool KeyringService::RecoverAddressByDefaultKeyring(
    const std::vector<uint8_t>& message,
    const std::vector<uint8_t>& signature,
    std::string* address) {
  CHECK(address);
  return EthereumKeyring::RecoverAddress(message, signature, address);
}

bool KeyringService::GetPublicKeyFromX25519_XSalsa20_Poly1305ByDefaultKeyring(
    const std::string& address,
    std::string* key) {
  CHECK(key);
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring)
    return false;
  return static_cast<EthereumKeyring*>(keyring)
      ->GetPublicKeyFromX25519_XSalsa20_Poly1305(
          EthAddress::FromHex(address).ToChecksumAddress(), key);
}

absl::optional<std::vector<uint8_t>>
KeyringService::DecryptCipherFromX25519_XSalsa20_Poly1305ByDefaultKeyring(
    const std::string& version,
    const std::vector<uint8_t>& nonce,
    const std::vector<uint8_t>& ephemeral_public_key,
    const std::vector<uint8_t>& ciphertext,
    const std::string& address) {
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring)
    return absl::nullopt;

  return static_cast<EthereumKeyring*>(keyring)
      ->DecryptCipherFromX25519_XSalsa20_Poly1305(
          version, nonce, ephemeral_public_key, ciphertext,
          EthAddress::FromHex(address).ToChecksumAddress());
}

std::vector<uint8_t> KeyringService::SignMessage(
    const std::string& keyring_id,
    const std::string& address,
    const std::vector<uint8_t>& message) {
  auto* keyring = GetHDKeyringById(keyring_id);
  if (!keyring || keyring_id == mojom::kDefaultKeyringId) {
    return std::vector<uint8_t>();
  }

  return keyring->SignMessage(address, message);
}

void KeyringService::AddAccountsWithDefaultName(size_t number) {
  auto* keyring = GetHDKeyringById(mojom::kDefaultKeyringId);
  if (!keyring) {
    DCHECK(false) << "Should only be called when default keyring exists";
    return;
  }

  size_t current_num = keyring->GetAccountsNumber();
  for (size_t i = current_num + 1; i <= current_num + number; ++i) {
    AddAccountForKeyring(mojom::kDefaultKeyringId, GetAccountName(i));
  }
}

bool KeyringService::IsLocked(const std::string& keyring_id) const {
  auto it = encryptors_.find(keyring_id);
  return (it == encryptors_.end()) || (it->second.get() == nullptr);
}

bool KeyringService::HasPendingUnlockRequest() const {
  return request_unlock_pending_;
}

absl::optional<std::string> KeyringService::GetSelectedAccount(
    mojom::CoinType coin) const {
  auto keyring_id = GetKeyringIdForCoinNonFIL(coin);
  if (!keyring_id) {
    NOTREACHED() << "GetFilecoinSelectedAccount must be used";
  }
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kSelectedAccount, *keyring_id);
  if (!value)
    return absl::nullopt;
  std::string address = value->GetString();
  if (address.empty()) {
    return absl::nullopt;
  }
  return address;
}

absl::optional<std::string> KeyringService::GetFilecoinSelectedAccount(
    const std::string& net) const {
  const base::Value* value =
      GetPrefForKeyring(*prefs_, kSelectedAccount, GetFilecoinKeyringId(net));

  if (!value)
    return absl::nullopt;
  std::string address = value->GetString();
  if (address.empty()) {
    return absl::nullopt;
  }
  return address;
}

void KeyringService::Lock() {
  if (IsLocked(mojom::kDefaultKeyringId))
    return;

  keyrings_.clear();
  encryptors_.clear();

  for (const auto& observer : observers_) {
    observer->Locked();
  }
  StopAutoLockTimer();
}

bool KeyringService::IsHardwareAccount(const std::string& address) const {
  auto keyrings = GetAvailableKeyringsFromPrefs(prefs_);
  for (const auto& keyring : keyrings) {
    for (const auto& hardware_account_info : GetHardwareAccountsSync(keyring)) {
      if (base::EqualsCaseInsensitiveASCII(hardware_account_info->address,
                                           address)) {
        return true;
      }
    }
  }
  return false;
}

void KeyringService::Unlock(const std::string& password,
                            KeyringService::UnlockCallback callback) {
  if (!ResumeKeyring(mojom::kDefaultKeyringId, password)) {
    encryptors_.erase(mojom::kDefaultKeyringId);
    std::move(callback).Run(false);
    return;
  }

  if (IsFilecoinEnabled()) {
    if (!ResumeKeyring(mojom::kFilecoinKeyringId, password)) {
      // If Filecoin keyring doesnt exist we keep encryptor pre-created
      // to be able to lazily create keyring later
      if (IsKeyringExist(mojom::kFilecoinKeyringId)) {
        VLOG(1) << __func__ << " Unable to unlock filecoin keyring";
        encryptors_.erase(mojom::kFilecoinKeyringId);
        std::move(callback).Run(false);
        return;
      }
    }

    if (!ResumeKeyring(mojom::kFilecoinTestnetKeyringId, password)) {
      if (IsKeyringExist(mojom::kFilecoinTestnetKeyringId)) {
        VLOG(1) << __func__ << " Unable to unlock filecoin testnet keyring";
        encryptors_.erase(mojom::kFilecoinTestnetKeyringId);
        std::move(callback).Run(false);
        return;
      }
    }
  }

  if (IsSolanaEnabled() && !ResumeKeyring(mojom::kSolanaKeyringId, password)) {
    if (IsKeyringExist(mojom::kSolanaKeyringId)) {
      VLOG(1) << __func__ << " Unable to unlock Solana keyring";
      encryptors_.erase(mojom::kSolanaKeyringId);
      std::move(callback).Run(false);
      return;
    }
  }

  UpdateLastUnlockPref(prefs_);
  request_unlock_pending_ = false;
  for (const auto& observer : observers_) {
    observer->Unlocked();
  }
  ResetAutoLockTimer();

  std::move(callback).Run(true);
}

void KeyringService::OnAutoLockFired() {
  Lock();
}

void KeyringService::IsLocked(IsLockedCallback callback) {
  std::move(callback).Run(IsLocked(mojom::kDefaultKeyringId));
}

void KeyringService::Reset(bool notify_observer) {
  StopAutoLockTimer();
  encryptors_.clear();
  keyrings_.clear();
  discovery_weak_factory_.InvalidateWeakPtrs();
  ClearKeyringServiceProfilePrefs(prefs_);
  if (notify_observer) {
    for (const auto& observer : observers_) {
      observer->KeyringReset();
    }
  }
}

void KeyringService::MaybeMigratePBKDF2Iterations(const std::string& password) {
  if (prefs_->GetBoolean(kBraveWalletKeyringEncryptionKeysMigrated)) {
    return;
  }

  // Pref is supposed to be set only as true.
  DCHECK(!prefs_->HasPrefPath(kBraveWalletKeyringEncryptionKeysMigrated));

  for (auto* keyring_id :
       {mojom::kDefaultKeyringId, mojom::kFilecoinKeyringId,
        mojom::kFilecoinTestnetKeyringId, mojom::kSolanaKeyringId}) {
    auto legacy_encrypted_mnemonic =
        GetPrefInBytesForKeyring(*prefs_, kEncryptedMnemonic, keyring_id);
    auto legacy_nonce =
        GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorNonce, keyring_id);
    auto legacy_salt =
        GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorSalt, keyring_id);

    if (!legacy_encrypted_mnemonic || !legacy_nonce || !legacy_salt) {
      continue;
    }

    auto legacy_encryptor = PasswordEncryptor::DeriveKeyFromPasswordUsingPbkdf2(
        password, *legacy_salt, kPbkdf2IterationsLegacy, kPbkdf2KeySize);
    if (!legacy_encryptor)
      continue;

    auto mnemonic =
        legacy_encryptor->Decrypt(*legacy_encrypted_mnemonic, *legacy_nonce);
    if (!mnemonic)
      continue;

    auto salt = GetOrCreateSaltForKeyring(keyring_id, /*force_create = */ true);

    auto encryptor = PasswordEncryptor::DeriveKeyFromPasswordUsingPbkdf2(
        password, salt, GetPbkdf2Iterations(), kPbkdf2KeySize);
    if (!encryptor)
      continue;

    auto nonce =
        GetOrCreateNonceForKeyring(keyring_id, /*force_create = */ true);

    SetPrefInBytesForKeyring(
        prefs_, kEncryptedMnemonic,
        encryptor->Encrypt(base::make_span(*mnemonic), nonce), keyring_id);

    if (keyring_id == mojom::kDefaultKeyringId) {
      prefs_->SetBoolean(kBraveWalletKeyringEncryptionKeysMigrated, true);
    }

    const base::Value::List* imported_accounts_legacy =
        GetPrefForKeyringList(*prefs_, kImportedAccounts, keyring_id);
    if (!imported_accounts_legacy)
      continue;
    base::Value::List imported_accounts = imported_accounts_legacy->Clone();
    for (auto& imported_account : imported_accounts) {
      if (!imported_account.is_dict())
        continue;

      const std::string* legacy_encrypted_private_key =
          imported_account.GetDict().FindString(kEncryptedPrivateKey);
      if (!legacy_encrypted_private_key)
        continue;

      auto legacy_private_key_decoded =
          base::Base64Decode(*legacy_encrypted_private_key);
      if (!legacy_private_key_decoded)
        continue;

      auto private_key = legacy_encryptor->Decrypt(
          base::make_span(*legacy_private_key_decoded), *legacy_nonce);
      if (!private_key)
        continue;

      imported_account.GetDict().Set(
          kEncryptedPrivateKey,
          base::Base64Encode(encryptor->Encrypt(*private_key, nonce)));
    }
    SetPrefForKeyring(prefs_, kImportedAccounts,
                      base::Value(std::move(imported_accounts)), keyring_id);
  }
}

void KeyringService::StopAutoLockTimer() {
  auto_lock_timer_->Stop();
}

void KeyringService::ResetAutoLockTimer() {
  if (auto_lock_timer_->IsRunning()) {
    auto_lock_timer_->Reset();
  } else {
    size_t auto_lock_minutes =
        (size_t)prefs_->GetInteger(kBraveWalletAutoLockMinutes);
    auto_lock_timer_->Start(FROM_HERE, base::Minutes(auto_lock_minutes), this,
                            &KeyringService::OnAutoLockFired);
  }
}

// static
absl::optional<std::vector<uint8_t>> KeyringService::GetPrefInBytesForKeyring(
    const PrefService& prefs,
    const std::string& key,
    const std::string& id) {
  const base::Value* value = GetPrefForKeyring(prefs, key, id);
  if (!value)
    return absl::nullopt;

  const std::string* encoded = value->GetIfString();
  if (!encoded || encoded->empty())
    return absl::nullopt;

  return base::Base64Decode(*encoded);
}

// static
void KeyringService::SetPrefInBytesForKeyring(PrefService* prefs,
                                              const std::string& key,
                                              base::span<const uint8_t> bytes,
                                              const std::string& id) {
  const std::string encoded = base::Base64Encode(bytes);
  SetPrefForKeyring(prefs, key, base::Value(encoded), id);
}

std::vector<uint8_t> KeyringService::GetOrCreateNonceForKeyring(
    const std::string& id,
    bool force_create) {
  if (!force_create) {
    if (auto nonce =
            GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorNonce, id)) {
      return *nonce;
    }
  }

  std::vector<uint8_t> nonce(kNonceSize);
  crypto::RandBytes(nonce);
  SetPrefInBytesForKeyring(prefs_, kPasswordEncryptorNonce, nonce, id);
  return nonce;
}

std::vector<uint8_t> KeyringService::GetOrCreateSaltForKeyring(
    const std::string& id,
    bool force_create) {
  if (!force_create) {
    if (auto salt =
            GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorSalt, id)) {
      return *salt;
    }
  }

  std::vector<uint8_t> salt(kSaltSize);
  crypto::RandBytes(salt);
  SetPrefInBytesForKeyring(prefs_, kPasswordEncryptorSalt, salt, id);
  return salt;
}

bool KeyringService::CreateEncryptorForKeyring(const std::string& password,
                                               const std::string& id) {
  if (password.empty())
    return false;

  // Added 08.08.2022
  MaybeMigratePBKDF2Iterations(password);

  encryptors_[id] = PasswordEncryptor::DeriveKeyFromPasswordUsingPbkdf2(
      password, GetOrCreateSaltForKeyring(id), GetPbkdf2Iterations(),
      kPbkdf2KeySize);
  return encryptors_[id] != nullptr;
}

bool KeyringService::CreateKeyringInternal(const std::string& keyring_id,
                                           const std::string& mnemonic,
                                           bool is_legacy_brave_wallet) {
  if (!encryptors_[keyring_id])
    return false;

  std::unique_ptr<std::vector<uint8_t>> seed = nullptr;
  if (is_legacy_brave_wallet)
    seed = MnemonicToEntropy(mnemonic);
  else
    seed = MnemonicToSeed(mnemonic, "");
  if (!seed)
    return false;
  if (is_legacy_brave_wallet && seed->size() != 32) {
    VLOG(1) << __func__
            << "mnemonic for legacy brave wallet must be 24 words which will "
               "produce 32 bytes seed";
    return false;
  }

  std::vector<uint8_t> encrypted_mnemonic = encryptors_[keyring_id]->Encrypt(
      ToSpan(mnemonic), GetOrCreateNonceForKeyring(keyring_id));

  SetPrefInBytesForKeyring(prefs_, kEncryptedMnemonic, encrypted_mnemonic,
                           keyring_id);
  SetPrefForKeyring(prefs_, kLegacyBraveWallet,
                    base::Value(is_legacy_brave_wallet), keyring_id);

  if (keyring_id == mojom::kDefaultKeyringId) {
    keyrings_[mojom::kDefaultKeyringId] = std::make_unique<EthereumKeyring>();
  } else if (IsFilecoinKeyringId(keyring_id)) {
    keyrings_[keyring_id] =
        std::make_unique<FilecoinKeyring>(GetFilecoinChainId(keyring_id));
  } else if (keyring_id == mojom::kSolanaKeyringId) {
    keyrings_[mojom::kSolanaKeyringId] = std::make_unique<SolanaKeyring>();
  }
  auto* keyring = GetHDKeyringById(keyring_id);
  DCHECK(keyring) << "No HDKeyring for " << keyring_id;
  if (keyring)
    keyring->ConstructRootHDKey(*seed, GetRootPath(keyring_id));

  UpdateLastUnlockPref(prefs_);

  return true;
}

bool KeyringService::IsKeyringCreated(const std::string& keyring_id) const {
  return HasPrefForKeyring(*prefs_, kEncryptedMnemonic, keyring_id);
}

void KeyringService::AddObserver(
    ::mojo::PendingRemote<mojom::KeyringServiceObserver> observer) {
  observers_.Add(std::move(observer));
}

void KeyringService::NotifyUserInteraction() {
  if (auto_lock_timer_->IsRunning()) {
    auto_lock_timer_->Reset();
  }
}

bool KeyringService::LazilyCreateKeyring(const std::string& keyring_id) {
  if (keyring_id == mojom::kDefaultKeyringId)
    return false;
  if (IsKeyringExist(keyring_id))
    return true;
  // we use same mnemonic from default keyring for non default keyrings
  auto mnemonic = GetMnemonicForKeyringImpl(mojom::kDefaultKeyringId);
  if (!CreateKeyringInternal(keyring_id, mnemonic, false))
    return false;

  for (const auto& observer : observers_) {
    observer->KeyringCreated(keyring_id);
  }

  return true;
}

void KeyringService::GetSelectedAccount(mojom::CoinType coin,
                                        GetSelectedAccountCallback callback) {
  std::move(callback).Run(GetSelectedAccount(coin));
}

void KeyringService::GetFilecoinSelectedAccount(
    const std::string& net,
    GetSelectedAccountCallback callback) {
  std::move(callback).Run(GetFilecoinSelectedAccount(net));
}

void KeyringService::SetSelectedAccount(const std::string& address,
                                        mojom::CoinType coin,
                                        SetSelectedAccountCallback callback) {
  auto keyring_id = GetKeyringId(coin, address);

  std::vector<mojom::AccountInfoPtr> infos =
      GetAccountInfosForKeyring(keyring_id);

  // Check for matching default and imported account
  for (const mojom::AccountInfoPtr& info : infos) {
    if (base::EqualsCaseInsensitiveASCII(info->address, address)) {
      SetSelectedAccountForCoin(coin, address);
      std::move(callback).Run(true);
      return;
    }
  }

  auto hardware_account_info_ptrs = GetHardwareAccountsSync(keyring_id);
  for (const mojom::AccountInfoPtr& info : hardware_account_info_ptrs) {
    if (base::EqualsCaseInsensitiveASCII(info->address, address)) {
      SetSelectedAccountForCoin(coin, address);
      std::move(callback).Run(true);
      return;
    }
  }
  std::move(callback).Run(false);
}

void KeyringService::SetKeyringDerivedAccountName(
    const std::string& keyring_id,
    const std::string& address,
    const std::string& name,
    KeyringService::SetKeyringDerivedAccountNameCallback callback) {
  auto* keyring = GetHDKeyringById(keyring_id);
  if (address.empty() || name.empty() || !keyring) {
    std::move(callback).Run(false);
    return;
  }

  const absl::optional<size_t> index = keyring->GetAccountIndex(address);
  if (!index) {
    std::move(callback).Run(false);
    return;
  }

  SetAccountMetaForKeyring(prefs_,
                           GetAccountPathByIndex(index.value(), keyring_id),
                           name, address, keyring_id);
  NotifyAccountsChanged();
  std::move(callback).Run(true);
}

bool KeyringService::UpdateNameForHardwareAccountSync(
    const std::string& address,
    const std::string& name,
    mojom::CoinType coin) {
  auto keyring_id = GetHardwareKeyringId(coin, address);
  base::Value::Dict& hardware_keyrings =
      GetPrefForKeyringUpdate(prefs_, kHardwareAccounts, keyring_id);
  for (auto&& [id, device] : hardware_keyrings) {
    DCHECK(device.is_dict());
    base::Value::Dict* account_metas = device.GetDict().FindDict(kAccountMetas);
    if (!account_metas)
      continue;
    base::Value::Dict* address_key = account_metas->FindDict(address);
    if (!address_key)
      continue;
    address_key->Set(kAccountName, name);
    NotifyAccountsChanged();
    return true;
  }
  return false;
}

void KeyringService::SetHardwareAccountName(
    const std::string& address,
    const std::string& name,
    mojom::CoinType coin,
    SetHardwareAccountNameCallback callback) {
  if (address.empty() || name.empty()) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(
      UpdateNameForHardwareAccountSync(address, name, coin));
}

void KeyringService::SetKeyringImportedAccountName(
    const std::string& keyring_id,
    const std::string& address,
    const std::string& name,
    SetKeyringImportedAccountNameCallback callback) {
  auto* keyring = GetHDKeyringById(keyring_id);
  if (address.empty() || name.empty() || !keyring) {
    std::move(callback).Run(false);
    return;
  }

  base::Value::List imported_accounts;
  const base::Value::List* value =
      GetPrefForKeyringList(*prefs_, kImportedAccounts, keyring_id);
  if (!value) {
    std::move(callback).Run(false);
    return;
  }

  imported_accounts = value->Clone();

  bool name_updated = false;
  for (auto& entry : imported_accounts) {
    DCHECK(entry.is_dict());
    base::Value::Dict& dict = entry.GetDict();
    const std::string* account_address = dict.FindString(kAccountAddress);
    if (account_address && *account_address == address) {
      dict.Set(kAccountName, name);
      SetPrefForKeyring(prefs_, kImportedAccounts,
                        base::Value(std::move(imported_accounts)), keyring_id);
      NotifyAccountsChanged();
      name_updated = true;
      break;
    }
  }

  std::move(callback).Run(name_updated);
}

void KeyringService::NotifyAccountsChanged() {
  for (const auto& observer : observers_) {
    observer->AccountsChanged();
  }
}

void KeyringService::NotifyAccountsAdded(
    mojom::CoinType coin,
    const std::vector<std::string>& addresses) {
  for (const auto& observer : observers_) {
    std::vector<std::string> addresses_clone;
    for (const auto& address : addresses) {
      addresses_clone.push_back(address);
    }
    observer->AccountsAdded(coin, addresses_clone);
  }
}

void KeyringService::OnAutoLockPreferenceChanged() {
  StopAutoLockTimer();
  ResetAutoLockTimer();
  for (const auto& observer : observers_) {
    observer->AutoLockMinutesChanged();
  }
}

void KeyringService::NotifySelectedAccountChanged(mojom::CoinType coin) {
  for (const auto& observer : observers_) {
    observer->SelectedAccountChanged(coin);
  }
}

void KeyringService::GetAutoLockMinutes(GetAutoLockMinutesCallback callback) {
  std::move(callback).Run(prefs_->GetInteger(kBraveWalletAutoLockMinutes));
}

void KeyringService::SetAutoLockMinutes(int32_t minutes,
                                        SetAutoLockMinutesCallback callback) {
  // Check bounds
  if (minutes < kAutoLockMinutesMin || minutes > kAutoLockMinutesMax) {
    std::move(callback).Run(false);
    return;
  }

  int32_t old_auto_lock_minutes =
      prefs_->GetInteger(kBraveWalletAutoLockMinutes);
  if (minutes != old_auto_lock_minutes) {
    prefs_->SetInteger(kBraveWalletAutoLockMinutes, minutes);
  }
  std::move(callback).Run(true);
}

void KeyringService::IsStrongPassword(const std::string& password,
                                      IsStrongPasswordCallback callback) {
  if (password.length() < 7) {
    std::move(callback).Run(false);
    return;
  }

  // Has at least one letter
  if (!RE2::PartialMatch(password, "[a-zA-Z]")) {
    std::move(callback).Run(false);
    return;
  }

  // Has at least one number
  if (!RE2::PartialMatch(password, "[0-9]")) {
    std::move(callback).Run(false);
    return;
  }

  // Has at least one non-alphanumeric character
  if (!RE2::PartialMatch(password, "[^0-9a-zA-Z]")) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

bool KeyringService::ValidatePasswordInternal(const std::string& password) {
  if (password.empty()) {
    return false;
  }

  const std::string keyring_id = mojom::kDefaultKeyringId;

  auto salt =
      GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorSalt, keyring_id);
  auto encrypted_mnemonic =
      GetPrefInBytesForKeyring(*prefs_, kEncryptedMnemonic, keyring_id);
  auto nonce =
      GetPrefInBytesForKeyring(*prefs_, kPasswordEncryptorNonce, keyring_id);

  if (!salt || !encrypted_mnemonic || !nonce) {
    return false;
  }

  auto iterations =
      prefs_->GetBoolean(kBraveWalletKeyringEncryptionKeysMigrated)
          ? GetPbkdf2Iterations()
          : kPbkdf2IterationsLegacy;

  // TODO(apaymyshev): move this call(and other ones in this file) to
  // background thread.
  auto encryptor = PasswordEncryptor::DeriveKeyFromPasswordUsingPbkdf2(
      password, *salt, iterations, kPbkdf2KeySize);

  if (!encryptor) {
    return false;
  }

  auto mnemonic = encryptor->Decrypt(*encrypted_mnemonic, *nonce);
  return mnemonic && !mnemonic->empty();
}

void KeyringService::ValidatePassword(const std::string& password,
                                      ValidatePasswordCallback callback) {
  std::move(callback).Run(ValidatePasswordInternal(password));
}

void KeyringService::GetChecksumEthAddress(
    const std::string& address,
    GetChecksumEthAddressCallback callback) {
  std::move(callback).Run(EthAddress::FromHex(address).ToChecksumAddress());
}

void KeyringService::HasPendingUnlockRequest(
    HasPendingUnlockRequestCallback callback) {
  std::move(callback).Run(HasPendingUnlockRequest());
}

void KeyringService::MaybeUnlockWithCommandLine() {
#if !defined(OFFICIAL_BUILD)
  std::string dev_wallet_password =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDevWalletPassword);
  if (!dev_wallet_password.empty()) {
    Unlock(dev_wallet_password, base::DoNothing());
  }
#endif  // !defined(OFFICIAL_BUILD)
}

}  // namespace brave_wallet
