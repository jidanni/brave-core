/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_shields/browser/ad_block_service.h"

#include <algorithm>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "brave/components/brave_shields/browser/ad_block_component_filters_provider.h"
#include "brave/components/brave_shields/browser/ad_block_custom_filters_provider.h"
#include "brave/components/brave_shields/browser/ad_block_default_resource_provider.h"
#include "brave/components/brave_shields/browser/ad_block_engine.h"
#include "brave/components/brave_shields/browser/ad_block_filter_list_catalog_provider.h"
#include "brave/components/brave_shields/browser/ad_block_regional_service_manager.h"
#include "brave/components/brave_shields/browser/ad_block_service_helper.h"
#include "brave/components/brave_shields/browser/ad_block_subscription_service_manager.h"
#include "brave/components/brave_shields/common/adblock_domain_resolver.h"
#include "brave/components/brave_shields/common/brave_shield_constants.h"
#include "brave/components/brave_shields/common/features.h"
#include "brave/components/brave_shields/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace {

const char kAdBlockComponentName[] = "Brave Ad Block Updater";
const char kAdBlockComponentId[] = "iodkpdagapdfkphljnddpjlldadblomo";
const char kAdBlockComponentBase64PublicKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsD/B/MGdz0gh7WkcFARn"
    "ZTBX9KAw2fuGeogijoI+fET38IK0L+P/trCT2NshqhRNmrDpLzV2+Dmes6PvkA+O"
    "dQkUV6VbChJG+baTfr3Oo5PdE0WxmP9Xh8XD7p85DQrk0jJilKuElxpK7Yq0JhcT"
    "Sc3XNHeTwBVqCnHwWZZ+XysYQfjuDQ0MgQpS/s7U04OZ63NIPe/iCQm32stvS/pE"
    "ya7KdBZXgRBQ59U6M1n1Ikkp3vfECShbBld6VrrmNrl59yKWlEPepJ9oqUc2Wf2M"
    "q+SDNXROG554RnU4BnDJaNETTkDTZ0Pn+rmLmp1qY5Si0yGsfHkrv3FS3vdxVozO"
    "PQIDAQAB";

std::string g_ad_block_component_id_(kAdBlockComponentId);
std::string g_ad_block_component_base64_public_key_(
    kAdBlockComponentBase64PublicKey);

}  // namespace

namespace brave_shields {

AdBlockService::SourceProviderObserver::SourceProviderObserver(
    base::WeakPtr<AdBlockEngine> adblock_engine,
    AdBlockFiltersProvider* filters_provider,
    AdBlockResourceProvider* resource_provider,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::RepeatingCallback<void(const adblock::FilterListMetadata&)>
        on_metadata_retrieved)
    : adblock_engine_(adblock_engine),
      filters_provider_(filters_provider),
      resource_provider_(resource_provider),
      on_metadata_retrieved_(on_metadata_retrieved),
      task_runner_(task_runner) {
  filters_provider_->AddObserver(this);
  filters_provider_->LoadDAT(this);
}

AdBlockService::SourceProviderObserver::~SourceProviderObserver() {
  filters_provider_->RemoveObserver(this);
  resource_provider_->RemoveObserver(this);
}

void AdBlockService::SourceProviderObserver::OnDATLoaded(
    bool deserialize,
    const DATFileDataBuffer& dat_buf) {
  deserialize_ = deserialize;
  dat_buf_ = std::move(dat_buf);
  // multiple AddObserver calls are ignored
  resource_provider_->AddObserver(this);
  resource_provider_->LoadResources(base::BindOnce(
      &SourceProviderObserver::OnResourcesLoaded, weak_factory_.GetWeakPtr()));
}

void AdBlockService::SourceProviderObserver::OnResourcesLoaded(
    const std::string& resources_json) {
  if (dat_buf_.empty()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&AdBlockEngine::AddResources, adblock_engine_,
                                  resources_json));
  } else {
    auto engine_load_callback = base::BindOnce(
        [](base::WeakPtr<AdBlockEngine> engine, bool deserialize,
           DATFileDataBuffer dat_buf, const std::string& resources_json)
            -> absl::optional<adblock::FilterListMetadata> {
          if (engine) {
            return engine->Load(deserialize, std::move(dat_buf),
                                resources_json);
          } else {
            return absl::nullopt;
          }
        },
        adblock_engine_, deserialize_, std::move(dat_buf_), resources_json);
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(engine_load_callback),
        base::BindOnce(&SourceProviderObserver::OnEngineReplaced,
                       weak_factory_.GetWeakPtr()));
  }
}

void AdBlockService::SourceProviderObserver::OnEngineReplaced(
    const absl::optional<adblock::FilterListMetadata> maybe_metadata) {
  if (maybe_metadata) {
    if (on_metadata_retrieved_) {
      on_metadata_retrieved_.Run(*maybe_metadata);
    }
  }
}

void AdBlockService::ShouldStartRequest(
    const GURL& url,
    blink::mojom::ResourceType resource_type,
    const std::string& tab_host,
    bool aggressive_blocking,
    bool* did_match_rule,
    bool* did_match_exception,
    bool* did_match_important,
    std::string* mock_data_url,
    std::string* rewritten_url) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  GURL request_url;

  if (aggressive_blocking ||
      base::FeatureList::IsEnabled(
          brave_shields::features::kBraveAdblockDefault1pBlocking) ||
      !SameDomainOrHost(
          url, url::Origin::CreateFromNormalizedTuple("https", tab_host, 80),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    request_url =
        rewritten_url && !rewritten_url->empty() ? GURL(*rewritten_url) : url;
    default_service()->ShouldStartRequest(
        request_url, resource_type, tab_host, aggressive_blocking,
        did_match_rule, did_match_exception, did_match_important, mock_data_url,
        rewritten_url);
    if (did_match_important && *did_match_important) {
      return;
    }
  }

  request_url =
      rewritten_url && !rewritten_url->empty() ? GURL(*rewritten_url) : url;
  regional_service_manager()->ShouldStartRequest(
      request_url, resource_type, tab_host, aggressive_blocking, did_match_rule,
      did_match_exception, did_match_important, mock_data_url, rewritten_url);
  if (did_match_important && *did_match_important) {
    return;
  }

  request_url =
      rewritten_url && !rewritten_url->empty() ? GURL(*rewritten_url) : url;
  subscription_service_manager()->ShouldStartRequest(
      request_url, resource_type, tab_host, aggressive_blocking, did_match_rule,
      did_match_exception, did_match_important, mock_data_url, rewritten_url);
  if (did_match_important && *did_match_important) {
    return;
  }

  request_url =
      rewritten_url && !rewritten_url->empty() ? GURL(*rewritten_url) : url;
  custom_filters_service()->ShouldStartRequest(
      request_url, resource_type, tab_host, aggressive_blocking, did_match_rule,
      did_match_exception, did_match_important, mock_data_url, rewritten_url);
}

absl::optional<std::string> AdBlockService::GetCspDirectives(
    const GURL& url,
    blink::mojom::ResourceType resource_type,
    const std::string& tab_host) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  auto csp_directives =
      default_service()->GetCspDirectives(url, resource_type, tab_host);

  const auto regional_csp = regional_service_manager()->GetCspDirectives(
      url, resource_type, tab_host);
  MergeCspDirectiveInto(regional_csp, &csp_directives);

  const auto custom_csp =
      custom_filters_service()->GetCspDirectives(url, resource_type, tab_host);
  MergeCspDirectiveInto(custom_csp, &csp_directives);

  return csp_directives;
}

absl::optional<base::Value> AdBlockService::UrlCosmeticResources(
    const std::string& url) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  absl::optional<base::Value> resources =
      default_service()->UrlCosmeticResources(url);

  if (!resources || !resources->is_dict()) {
    return resources;
  }

  absl::optional<base::Value> regional_resources =
      regional_service_manager()->UrlCosmeticResources(url);

  if (regional_resources && regional_resources->is_dict()) {
    MergeResourcesInto(std::move(regional_resources->GetDict()),
                       resources->GetIfDict(),
                       /*force_hide=*/true);
  }

  absl::optional<base::Value> custom_resources =
      custom_filters_service()->UrlCosmeticResources(url);

  if (custom_resources && custom_resources->is_dict()) {
    MergeResourcesInto(std::move(custom_resources->GetDict()),
                       resources->GetIfDict(),
                       /*force_hide=*/true);
  }

  absl::optional<base::Value> subscription_resources =
      subscription_service_manager()->UrlCosmeticResources(url);

  if (subscription_resources && subscription_resources->is_dict()) {
    MergeResourcesInto(std::move(subscription_resources->GetDict()),
                       resources->GetIfDict(),
                       /*force_hide=*/true);
  }

  return resources;
}

// The return value here is formatted differently from the rest of the adblock
// service instances. We need to distinguish between selectors returned from
// the default engine and those returned by other engines, but still comply
// with the virtual method signature.
// This can be improved once interfaces are decoupled in
// https://github.com/brave/brave-core/pull/10994.
// For now, this returns a dict with two properties:
//  - "hide_selectors" - wraps the result from the default engine
//  - "force_hide_selectors" - wraps appended results from all other engines
base::Value::Dict AdBlockService::HiddenClassIdSelectors(
    const std::vector<std::string>& classes,
    const std::vector<std::string>& ids,
    const std::vector<std::string>& exceptions) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  base::Value::List hide_selectors =
      default_service()->HiddenClassIdSelectors(classes, ids, exceptions);

  base::Value::List regional_selectors =
      regional_service_manager()->HiddenClassIdSelectors(classes, ids,
                                                         exceptions);

  base::Value::List custom_selectors =
      custom_filters_service()->HiddenClassIdSelectors(classes, ids,
                                                       exceptions);
  base::Value::List subscription_selectors =
      subscription_service_manager()->HiddenClassIdSelectors(classes, ids,
                                                             exceptions);

  base::Value::List force_hide_selectors = std::move(regional_selectors);

  for (auto& custom_selector : custom_selectors) {
    force_hide_selectors.Append(std::move(custom_selector));
  }

  for (auto& subscription_selector : subscription_selectors) {
    force_hide_selectors.Append(std::move(subscription_selector));
  }

  base::Value::Dict result;
  result.Set("hide_selectors", std::move(hide_selectors));
  result.Set("force_hide_selectors", std::move(force_hide_selectors));
  return result;
}

AdBlockRegionalServiceManager* AdBlockService::regional_service_manager() {
  if (!regional_service_manager_) {
    regional_service_manager_ =
        brave_shields::AdBlockRegionalServiceManagerFactory(
            local_state_, locale_, component_update_service_, GetTaskRunner());
    regional_service_manager_->Init(resource_provider_.get(),
                                    filter_list_catalog_provider_.get());
  }
  return regional_service_manager_.get();
}

AdBlockEngine* AdBlockService::default_service() {
  if (!default_service_) {
    default_service_ =
        std::unique_ptr<AdBlockEngine, base::OnTaskRunnerDeleter>(
            new AdBlockEngine(), base::OnTaskRunnerDeleter(GetTaskRunner()));
    default_service_observer_ = std::make_unique<SourceProviderObserver>(
        default_service_->AsWeakPtr(), default_filters_provider_.get(),
        resource_provider_.get(), GetTaskRunner());
  }
  return default_service_.get();
}

AdBlockEngine* AdBlockService::custom_filters_service() {
  if (!custom_filters_service_) {
    custom_filters_service_ =
        std::unique_ptr<AdBlockEngine, base::OnTaskRunnerDeleter>(
            new AdBlockEngine(), base::OnTaskRunnerDeleter(GetTaskRunner()));
    custom_filters_service_observer_ = std::make_unique<SourceProviderObserver>(
        custom_filters_service_->AsWeakPtr(), custom_filters_provider_.get(),
        resource_provider_.get(), GetTaskRunner());
  }
  return custom_filters_service_.get();
}

brave_shields::AdBlockCustomFiltersProvider*
AdBlockService::custom_filters_provider() {
  return custom_filters_provider_.get();
}

brave_shields::AdBlockSubscriptionServiceManager*
AdBlockService::subscription_service_manager() {
  if (!subscription_service_manager_->IsInitialized()) {
    subscription_service_manager_->Init(resource_provider_.get());
  }
  return subscription_service_manager_.get();
}

AdBlockService::AdBlockService(
    PrefService* local_state,
    std::string locale,
    component_updater::ComponentUpdateService* cus,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<AdBlockSubscriptionServiceManager>
        subscription_service_manager)
    : local_state_(local_state),
      locale_(locale),
      component_update_service_(cus),
      task_runner_(task_runner),
      custom_filters_service_(nullptr, base::OnTaskRunnerDeleter(task_runner_)),
      default_service_(nullptr, base::OnTaskRunnerDeleter(task_runner_)),
      subscription_service_manager_(std::move(subscription_service_manager)) {
  // Initializes adblock-rust's domain resolution implementation
  adblock::SetDomainResolver(AdBlockServiceDomainResolver);

  default_filters_provider_ =
      std::make_unique<brave_shields::AdBlockComponentFiltersProvider>(
          component_update_service_, g_ad_block_component_id_,
          g_ad_block_component_base64_public_key_, kAdBlockComponentName);
  resource_provider_ =
      std::make_unique<brave_shields::AdBlockDefaultResourceProvider>(
          component_update_service_);
  filter_list_catalog_provider_ =
      std::make_unique<brave_shields::AdBlockFilterListCatalogProvider>(
          component_update_service_);
  custom_filters_provider_ =
      std::make_unique<brave_shields::AdBlockCustomFiltersProvider>(
          local_state_);
}

AdBlockService::~AdBlockService() = default;

bool AdBlockService::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initialize each service:
  default_service();
  custom_filters_service();
  regional_service_manager();
  subscription_service_manager();

  return true;
}

void AdBlockService::EnableTag(const std::string& tag, bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Tags only need to be modified for the default engine.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AdBlockEngine::EnableTag,
                                default_service()->AsWeakPtr(), tag, enabled));
}

base::SequencedTaskRunner* AdBlockService::GetTaskRunner() {
  return task_runner_.get();
}

void RegisterPrefsForAdBlockService(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAdBlockCookieListOptInShown, false);
  registry->RegisterBooleanPref(prefs::kAdBlockCookieListSettingTouched, false);
  registry->RegisterStringPref(prefs::kAdBlockCustomFilters, std::string());
  registry->RegisterDictionaryPref(prefs::kAdBlockRegionalFilters);
  registry->RegisterDictionaryPref(prefs::kAdBlockListSubscriptions);
  registry->RegisterBooleanPref(prefs::kAdBlockCheckedDefaultRegion, false);
}

AdBlockResourceProvider* AdBlockService::resource_provider() {
  return resource_provider_.get();
}

void AdBlockService::UseSourceProvidersForTest(
    AdBlockFiltersProvider* source_provider,
    AdBlockResourceProvider* resource_provider) {
  default_service_observer_ = std::make_unique<SourceProviderObserver>(
      default_service_->AsWeakPtr(), source_provider, resource_provider,
      GetTaskRunner());
}

void AdBlockService::UseCustomSourceProvidersForTest(
    AdBlockFiltersProvider* source_provider,
    AdBlockResourceProvider* resource_provider) {
  custom_filters_service_observer_ = std::make_unique<SourceProviderObserver>(
      custom_filters_service_->AsWeakPtr(), source_provider, resource_provider,
      GetTaskRunner());
}

bool AdBlockService::TagExistsForTest(const std::string& tag) {
  return default_service()->TagExists(tag);
}

// static
void SetDefaultAdBlockComponentIdAndBase64PublicKeyForTest(
    const std::string& component_id,
    const std::string& component_base64_public_key) {
  g_ad_block_component_id_ = component_id;
  g_ad_block_component_base64_public_key_ = component_base64_public_key;
}

}  // namespace brave_shields
