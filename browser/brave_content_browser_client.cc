/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_content_browser_client.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "brave/browser/brave_ads/ads_service_factory.h"
#include "brave/browser/brave_ads/brave_ads_host.h"
#include "brave/browser/brave_browser_main_extra_parts.h"
#include "brave/browser/brave_browser_process.h"
#include "brave/browser/brave_shields/brave_shields_web_contents_observer.h"
#include "brave/browser/brave_wallet/brave_wallet_context_utils.h"
#include "brave/browser/brave_wallet/brave_wallet_provider_delegate_impl.h"
#include "brave/browser/brave_wallet/brave_wallet_service_factory.h"
#include "brave/browser/brave_wallet/json_rpc_service_factory.h"
#include "brave/browser/brave_wallet/keyring_service_factory.h"
#include "brave/browser/brave_wallet/tx_service_factory.h"
#include "brave/browser/debounce/debounce_service_factory.h"
#include "brave/browser/ephemeral_storage/ephemeral_storage_service_factory.h"
#include "brave/browser/ethereum_remote_client/buildflags/buildflags.h"
#include "brave/browser/net/brave_proxying_url_loader_factory.h"
#include "brave/browser/net/brave_proxying_web_socket.h"
#include "brave/browser/profiles/brave_renderer_updater.h"
#include "brave/browser/profiles/brave_renderer_updater_factory.h"
#include "brave/browser/profiles/profile_util.h"
#include "brave/browser/skus/skus_service_factory.h"
#include "brave/components/brave_ads/browser/ads_status_header_throttle.h"
#include "brave/components/brave_ads/common/features.h"
#include "brave/components/brave_federated/features.h"
#include "brave/components/brave_rewards/browser/rewards_protocol_handler.h"
#include "brave/components/brave_search/browser/brave_search_default_host.h"
#include "brave/components/brave_search/browser/brave_search_default_host_private.h"
#include "brave/components/brave_search/browser/brave_search_fallback_host.h"
#include "brave/components/brave_search/common/brave_search_default.mojom.h"
#include "brave/components/brave_search/common/brave_search_fallback.mojom.h"
#include "brave/components/brave_search/common/brave_search_utils.h"
#include "brave/components/brave_shields/browser/ad_block_service.h"
#include "brave/components/brave_shields/browser/brave_farbling_service.h"
#include "brave/components/brave_shields/browser/brave_shields_util.h"
#include "brave/components/brave_shields/browser/domain_block_navigation_throttle.h"
#include "brave/components/brave_shields/common/brave_shield_constants.h"
#include "brave/components/brave_shields/common/features.h"
#include "brave/components/brave_vpn/buildflags/buildflags.h"
#include "brave/components/brave_wallet/browser/brave_wallet_p3a_private.h"
#include "brave/components/brave_wallet/browser/brave_wallet_service.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/ethereum_provider_impl.h"
#include "brave/components/brave_wallet/browser/solana_provider_impl.h"
#include "brave/components/brave_wallet/common/brave_wallet.mojom.h"
#include "brave/components/brave_webtorrent/browser/buildflags/buildflags.h"
#include "brave/components/constants/pref_names.h"
#include "brave/components/constants/webui_url_constants.h"
#include "brave/components/cosmetic_filters/browser/cosmetic_filters_resources.h"
#include "brave/components/cosmetic_filters/common/cosmetic_filters.mojom.h"
#include "brave/components/de_amp/browser/de_amp_throttle.h"
#include "brave/components/debounce/browser/debounce_navigation_throttle.h"
#include "brave/components/decentralized_dns/content/decentralized_dns_navigation_throttle.h"
#include "brave/components/ipfs/buildflags/buildflags.h"
#include "brave/components/playlist/buildflags/buildflags.h"
#include "brave/components/playlist/features.h"
#include "brave/components/sidebar/buildflags/buildflags.h"
#include "brave/components/skus/common/skus_sdk.mojom.h"
#include "brave/components/speedreader/common/buildflags.h"
#include "brave/components/tor/buildflags/buildflags.h"
#include "brave/components/translate/core/common/brave_translate_switches.h"
#include "brave/grit/brave_generated_resources.h"
#include "brave/third_party/blink/renderer/brave_farbling_constants.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/version_info.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/site_for_cookies.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

using blink::web_pref::WebPreferences;
using brave_shields::BraveShieldsWebContentsObserver;
using brave_shields::ControlType;
using brave_shields::GetBraveShieldsEnabled;
using brave_shields::GetFingerprintingControlType;
using content::BrowserThread;
using content::ContentBrowserClient;
using content::RenderFrameHost;
using content::WebContents;

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "extensions/browser/extension_registry.h"
using extensions::ChromeContentBrowserClientExtensionsPart;
#endif

#if BUILDFLAG(ENABLE_BRAVE_WEBTORRENT)
#include "brave/browser/extensions/brave_webtorrent_navigation_throttle.h"
#include "brave/components/brave_webtorrent/browser/content_browser_client_helper.h"
#endif

#if BUILDFLAG(ENABLE_IPFS)
#include "brave/browser/ipfs/content_browser_client_helper.h"
#include "brave/browser/ipfs/ipfs_service_factory.h"
#include "brave/browser/ipfs/ipfs_subframe_navigation_throttle.h"
#include "brave/components/ipfs/ipfs_constants.h"
#include "brave/components/ipfs/ipfs_navigation_throttle.h"
#endif

#if BUILDFLAG(ENABLE_TOR)
#include "brave/browser/tor/onion_location_navigation_throttle_delegate.h"
#include "brave/browser/tor/tor_profile_service_factory.h"
#include "brave/components/tor/onion_location_navigation_throttle.h"
#include "brave/components/tor/tor_navigation_throttle.h"
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
#include "brave/browser/speedreader/speedreader_service_factory.h"
#include "brave/browser/speedreader/speedreader_tab_helper.h"
#include "brave/browser/ui/webui/speedreader/speedreader_panel_ui.h"
#include "brave/components/speedreader/common/speedreader_panel.mojom.h"
#include "brave/components/speedreader/speedreader_throttle.h"
#include "brave/components/speedreader/speedreader_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE)
#include "brave/browser/brave_drm_tab_helper.h"
#endif

#if BUILDFLAG(ENABLE_BRAVE_VPN)
#include "brave/browser/brave_vpn/brave_vpn_service_factory.h"
#include "brave/browser/ui/webui/brave_vpn/vpn_panel_ui.h"
#include "brave/components/brave_vpn/brave_vpn_utils.h"
#include "brave/components/brave_vpn/mojom/brave_vpn.mojom.h"
#endif

#if BUILDFLAG(ETHEREUM_REMOTE_CLIENT_ENABLED)
#include "brave/browser/ethereum_remote_client/ethereum_remote_client_constants.h"
#include "brave/browser/ethereum_remote_client/ethereum_remote_client_service.h"
#include "brave/browser/ethereum_remote_client/ethereum_remote_client_service_factory.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "brave/browser/new_tab/new_tab_shows_navigation_throttle.h"
#include "brave/browser/ui/webui/brave_federated/federated_internals.mojom.h"
#include "brave/browser/ui/webui/brave_federated/federated_internals_ui.h"
#include "brave/browser/ui/webui/brave_rewards/rewards_panel_ui.h"
#include "brave/browser/ui/webui/brave_shields/cookie_list_opt_in_ui.h"
#include "brave/browser/ui/webui/brave_shields/shields_panel_ui.h"
#include "brave/browser/ui/webui/brave_wallet/wallet_page_ui.h"
#include "brave/browser/ui/webui/brave_wallet/wallet_panel_ui.h"
#include "brave/browser/ui/webui/new_tab_page/brave_new_tab_ui.h"
#include "brave/browser/ui/webui/private_new_tab_page/brave_private_new_tab_ui.h"
#include "brave/components/brave_new_tab_ui/brave_new_tab_page.mojom.h"
#include "brave/components/brave_private_new_tab_ui/common/brave_private_new_tab.mojom.h"
#include "brave/components/brave_rewards/common/brave_rewards_panel.mojom.h"
#include "brave/components/brave_rewards/common/features.h"
#include "brave/components/brave_shields/common/brave_shields_panel.mojom.h"
#include "brave/components/brave_shields/common/cookie_list_opt_in.mojom.h"
#include "brave/components/brave_today/common/brave_news.mojom.h"
#include "brave/components/brave_today/common/features.h"
#endif

#if BUILDFLAG(ENABLE_PLAYLIST)
#include "brave/browser/playlist/playlist_service_factory.h"
#include "brave/components/playlist/playlist_service.h"
#endif

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
#include "brave/browser/ui/webui/playlist_ui.h"
#endif  // BUILDFLAG(ENABLE_PLAYLIST_WEBUI)

namespace {

bool HandleURLReverseOverrideRewrite(GURL* url,
                                     content::BrowserContext* browser_context) {
  if (BraveContentBrowserClient::HandleURLOverrideRewrite(url, browser_context))
    return true;

  return false;
}

bool HandleURLRewrite(GURL* url, content::BrowserContext* browser_context) {
  if (BraveContentBrowserClient::HandleURLOverrideRewrite(url, browser_context))
    return true;

  return false;
}

void BindCosmeticFiltersResourcesOnTaskRunner(
    mojo::PendingReceiver<cosmetic_filters::mojom::CosmeticFiltersResources>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<cosmetic_filters::CosmeticFiltersResources>(
          g_brave_browser_process->ad_block_service()),
      std::move(receiver));
}

void BindBraveAdsHost(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_ads::mojom::BraveAdsHost> receiver) {
  auto* context = frame_host->GetBrowserContext();
  auto* profile = Profile::FromBrowserContext(context);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<brave_ads::BraveAdsHost>(profile, web_contents),
      std::move(receiver));
}

void BindCosmeticFiltersResources(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<cosmetic_filters::mojom::CosmeticFiltersResources>
        receiver) {
  g_brave_browser_process->ad_block_service()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&BindCosmeticFiltersResourcesOnTaskRunner,
                                std::move(receiver)));
}

void MaybeBindWalletP3A(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_wallet::mojom::BraveWalletP3A> receiver) {
  auto* context = frame_host->GetBrowserContext();
  if (brave_wallet::IsAllowedForContext(frame_host->GetBrowserContext())) {
    brave_wallet::BraveWalletService* wallet_service =
        brave_wallet::BraveWalletServiceFactory::GetServiceForContext(context);
    DCHECK(wallet_service);
    wallet_service->GetBraveWalletP3A()->Bind(std::move(receiver));
  } else {
    // Dummy API to avoid reporting P3A for OTR contexts
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<brave_wallet::BraveWalletP3APrivate>(),
        std::move(receiver));
  }
}

void MaybeBindEthereumProvider(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_wallet::mojom::EthereumProvider> receiver) {
  auto* json_rpc_service =
      brave_wallet::JsonRpcServiceFactory::GetServiceForContext(
          frame_host->GetBrowserContext());

  if (!json_rpc_service)
    return;

  auto* tx_service = brave_wallet::TxServiceFactory::GetServiceForContext(
      frame_host->GetBrowserContext());
  if (!tx_service)
    return;

  auto* keyring_service =
      brave_wallet::KeyringServiceFactory::GetServiceForContext(
          frame_host->GetBrowserContext());
  if (!keyring_service)
    return;

  auto* brave_wallet_service =
      brave_wallet::BraveWalletServiceFactory::GetServiceForContext(
          frame_host->GetBrowserContext());
  if (!brave_wallet_service)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<brave_wallet::EthereumProviderImpl>(
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(frame_host->GetBrowserContext())),
          json_rpc_service, tx_service, keyring_service, brave_wallet_service,
          std::make_unique<brave_wallet::BraveWalletProviderDelegateImpl>(
              web_contents, frame_host),
          user_prefs::UserPrefs::Get(web_contents->GetBrowserContext())),
      std::move(receiver));
}

void MaybeBindSolanaProvider(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_wallet::mojom::SolanaProvider> receiver) {
  auto* keyring_service =
      brave_wallet::KeyringServiceFactory::GetServiceForContext(
          frame_host->GetBrowserContext());
  if (!keyring_service)
    return;

  auto* brave_wallet_service =
      brave_wallet::BraveWalletServiceFactory::GetServiceForContext(
          frame_host->GetBrowserContext());
  if (!brave_wallet_service)
    return;

  auto* tx_service = brave_wallet::TxServiceFactory::GetServiceForContext(
      frame_host->GetBrowserContext());
  if (!tx_service)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<brave_wallet::SolanaProviderImpl>(
          keyring_service, brave_wallet_service, tx_service,
          std::make_unique<brave_wallet::BraveWalletProviderDelegateImpl>(
              web_contents, frame_host)),
      std::move(receiver));
}

void BindBraveSearchFallbackHost(
    int process_id,
    mojo::PendingReceiver<brave_search::mojom::BraveSearchFallback> receiver) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(process_id);
  if (!render_process_host)
    return;

  content::BrowserContext* context = render_process_host->GetBrowserContext();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<brave_search::BraveSearchFallbackHost>(
          context->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess()),
      std::move(receiver));
}

void BindBraveSearchDefaultHost(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_search::mojom::BraveSearchDefault> receiver) {
  auto* context = frame_host->GetBrowserContext();
  auto* profile = Profile::FromBrowserContext(context);
  if (brave::IsRegularProfile(profile)) {
    auto* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    const std::string host = frame_host->GetLastCommittedURL().host();
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<brave_search::BraveSearchDefaultHost>(
            host, template_url_service, profile->GetPrefs()),
        std::move(receiver));
  } else {
    // Dummy API which always returns false for private contexts.
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<brave_search::BraveSearchDefaultHostPrivate>(),
        std::move(receiver));
  }
}

#if BUILDFLAG(ENABLE_BRAVE_VPN)
void MaybeBindBraveVpnImpl(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<brave_vpn::mojom::ServiceHandler> receiver) {
  auto* context = frame_host->GetBrowserContext();
  brave_vpn::BraveVpnServiceFactory::BindForContext(context,
                                                    std::move(receiver));
}
#endif
void MaybeBindSkusSdkImpl(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<skus::mojom::SkusService> receiver) {
  auto* context = frame_host->GetBrowserContext();
  skus::SkusServiceFactory::BindForContext(context, std::move(receiver));
}

}  // namespace

BraveContentBrowserClient::BraveContentBrowserClient() = default;

BraveContentBrowserClient::~BraveContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
BraveContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  std::unique_ptr<content::BrowserMainParts> main_parts =
      ChromeContentBrowserClient::CreateBrowserMainParts(is_integration_test);
  ChromeBrowserMainParts* chrome_main_parts =
      static_cast<ChromeBrowserMainParts*>(main_parts.get());
  chrome_main_parts->AddParts(std::make_unique<BraveBrowserMainExtraParts>());
  return main_parts;
}

void BraveContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
#if BUILDFLAG(ENABLE_BRAVE_WEBTORRENT)
  handler->AddHandlerPair(&webtorrent::HandleMagnetURLRewrite,
                          content::BrowserURLHandler::null_handler());
  handler->AddHandlerPair(&webtorrent::HandleTorrentURLRewrite,
                          &webtorrent::HandleTorrentURLReverseRewrite);
#endif
#if BUILDFLAG(ENABLE_IPFS)
  handler->AddHandlerPair(&ipfs::HandleIPFSURLRewrite,
                          &ipfs::HandleIPFSURLReverseRewrite);
#endif
  handler->AddHandlerPair(&HandleURLRewrite, &HandleURLReverseOverrideRewrite);
  ChromeContentBrowserClient::BrowserURLHandlerCreated(handler);
}

void BraveContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  Profile* profile = Profile::FromBrowserContext(host->GetBrowserContext());
  BraveRendererUpdaterFactory::GetForProfile(profile)->InitializeRenderer(host);

  ChromeContentBrowserClient::RenderProcessWillLaunch(host);
}

void BraveContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,                // NOLINT
        blink::AssociatedInterfaceRegistry& associated_registry) {  // NOLINT
#if BUILDFLAG(ENABLE_WIDEVINE)
  associated_registry.AddInterface<
      brave_drm::mojom::BraveDRM>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<brave_drm::mojom::BraveDRM> receiver) {
        BraveDrmTabHelper::BindBraveDRM(std::move(receiver), render_frame_host);
      },
      &render_frame_host));
#endif  // BUILDFLAG(ENABLE_WIDEVINE)

  associated_registry.AddInterface<
      brave_shields::mojom::BraveShieldsHost>(base::BindRepeating(
      [](content::RenderFrameHost* render_frame_host,
         mojo::PendingAssociatedReceiver<brave_shields::mojom::BraveShieldsHost>
             receiver) {
        brave_shields::BraveShieldsWebContentsObserver::BindBraveShieldsHost(
            std::move(receiver), render_frame_host);
      },
      &render_frame_host));

#if BUILDFLAG(ENABLE_SPEEDREADER)
  associated_registry.AddInterface<speedreader::mojom::SpeedreaderHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<
                 speedreader::mojom::SpeedreaderHost> receiver) {
            speedreader::SpeedreaderTabHelper::BindSpeedreaderHost(
                std::move(receiver), render_frame_host);
          },
          &render_frame_host));
#endif

  ChromeContentBrowserClient::
      RegisterAssociatedInterfaceBindersForRenderFrameHost(render_frame_host,
                                                           associated_registry);
}
void BraveContentBrowserClient::RegisterWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  ChromeContentBrowserClient::RegisterWebUIInterfaceBrokers(registry);
#if BUILDFLAG(ENABLE_BRAVE_VPN) && !BUILDFLAG(IS_ANDROID)
  if (brave_vpn::IsBraveVPNEnabled()) {
    registry.ForWebUI<VPNPanelUI>()
        .Add<brave_vpn::mojom::PanelHandlerFactory>();
  }
#endif

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
  if (base::FeatureList::IsEnabled(playlist::features::kPlaylist)) {
    registry.ForWebUI<playlist::PlaylistUI>()
        .Add<playlist::mojom::PageHandlerFactory>();
  }
#endif
}

bool BraveContentBrowserClient::AllowWorkerFingerprinting(
    const GURL& url,
    content::BrowserContext* browser_context) {
  return WorkerGetBraveFarblingLevel(url, browser_context) !=
         BraveFarblingLevel::MAXIMUM;
}

uint8_t BraveContentBrowserClient::WorkerGetBraveFarblingLevel(
    const GURL& url,
    content::BrowserContext* browser_context) {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context);
  const bool shields_up =
      brave_shields::GetBraveShieldsEnabled(host_content_settings_map, url);
  if (!shields_up)
    return BraveFarblingLevel::OFF;
  auto fingerprinting_type = brave_shields::GetFingerprintingControlType(
      host_content_settings_map, url);
  if (fingerprinting_type == ControlType::BLOCK)
    return BraveFarblingLevel::MAXIMUM;
  if (fingerprinting_type == ControlType::ALLOW)
    return BraveFarblingLevel::OFF;
  return BraveFarblingLevel::BALANCED;
}

content::ContentBrowserClient::AllowWebBluetoothResult
BraveContentBrowserClient::AllowWebBluetooth(
    content::BrowserContext* browser_context,
    const url::Origin& requesting_origin,
    const url::Origin& embedding_origin) {
  return ContentBrowserClient::AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED;
}

void BraveContentBrowserClient::ExposeInterfacesToRenderer(
    service_manager::BinderRegistry* registry,
    blink::AssociatedInterfaceRegistry* associated_registry,
    content::RenderProcessHost* render_process_host) {
  ChromeContentBrowserClient::ExposeInterfacesToRenderer(
      registry, associated_registry, render_process_host);
  registry->AddInterface(base::BindRepeating(&BindBraveSearchFallbackHost,
                                             render_process_host->GetID()),
                         content::GetUIThreadTaskRunner({}));
}

void BraveContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  ChromeContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
  map->Add<cosmetic_filters::mojom::CosmeticFiltersResources>(
      base::BindRepeating(&BindCosmeticFiltersResources));
  if (brave_search::IsDefaultAPIEnabled()) {
    map->Add<brave_search::mojom::BraveSearchDefault>(
        base::BindRepeating(&BindBraveSearchDefaultHost));
  }

  if (base::FeatureList::IsEnabled(
          brave_ads::features::kSupportBraveSearchResultAdConfirmationEvents)) {
    map->Add<brave_ads::mojom::BraveAdsHost>(
        base::BindRepeating(&BindBraveAdsHost));
  }

  map->Add<brave_wallet::mojom::BraveWalletP3A>(
      base::BindRepeating(&MaybeBindWalletP3A));
  if (brave_wallet::IsAllowedForContext(
          render_frame_host->GetBrowserContext())) {
    if (brave_wallet::IsNativeWalletEnabled()) {
      map->Add<brave_wallet::mojom::EthereumProvider>(
          base::BindRepeating(&MaybeBindEthereumProvider));
      map->Add<brave_wallet::mojom::SolanaProvider>(
          base::BindRepeating(&MaybeBindSolanaProvider));
    }
  }

  map->Add<skus::mojom::SkusService>(
      base::BindRepeating(&MaybeBindSkusSdkImpl));
#if BUILDFLAG(ENABLE_BRAVE_VPN)
  map->Add<brave_vpn::mojom::ServiceHandler>(
      base::BindRepeating(&MaybeBindBraveVpnImpl));
#endif
#if !BUILDFLAG(IS_ANDROID)
  content::RegisterWebUIControllerInterfaceBinder<
      brave_wallet::mojom::PanelHandlerFactory, WalletPanelUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      brave_wallet::mojom::PageHandlerFactory, WalletPageUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      brave_private_new_tab::mojom::PageHandler, BravePrivateNewTabUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      brave_shields::mojom::PanelHandlerFactory, ShieldsPanelUI>(map);
  if (base::FeatureList::IsEnabled(
          brave_shields::features::kBraveAdblockCookieListOptIn)) {
    content::RegisterWebUIControllerInterfaceBinder<
        brave_shields::mojom::CookieListOptInPageHandlerFactory,
        CookieListOptInUI>(map);
  }
  content::RegisterWebUIControllerInterfaceBinder<
      brave_rewards::mojom::PanelHandlerFactory, RewardsPanelUI>(map);
  if (base::FeatureList::IsEnabled(
          brave_federated::features::kFederatedLearning)) {
    content::RegisterWebUIControllerInterfaceBinder<
        federated_internals::mojom::PageHandlerFactory,
        brave_federated::FederatedInternalsUI>(map);
  }
#endif

// Brave News
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(brave_today::features::kBraveNewsFeature)) {
    content::RegisterWebUIControllerInterfaceBinder<
        brave_news::mojom::BraveNewsController, BraveNewTabUI>(map);
  }
#endif

#if BUILDFLAG(ENABLE_PLAYLIST_WEBUI)
  if (base::FeatureList::IsEnabled(playlist::features::kPlaylist)) {
    content::RegisterWebUIControllerInterfaceBinder<
        playlist::mojom::PageHandlerFactory, playlist::PlaylistUI>(map);
  }
#endif

#if BUILDFLAG(ENABLE_SPEEDREADER)
  if (speedreader::IsSpeedreaderPanelV2Enabled()) {
    content::RegisterWebUIControllerInterfaceBinder<
        speedreader::mojom::PanelFactory, SpeedreaderPanelUI>(map);
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  content::RegisterWebUIControllerInterfaceBinder<
      brave_new_tab_page::mojom::PageHandlerFactory, BraveNewTabUI>(map);
#endif
}

bool BraveContentBrowserClient::HandleExternalProtocol(
    const GURL& url,
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    content::NavigationUIData* navigation_data,
    bool is_primary_main_frame,
    bool is_in_fenced_frame_tree,
    network::mojom::WebSandboxFlags sandbox_flags,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const absl::optional<url::Origin>& initiating_origin,
    content::RenderFrameHost* initiator_document,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
#if BUILDFLAG(ENABLE_BRAVE_WEBTORRENT)
  if (webtorrent::IsMagnetProtocol(url)) {
    auto weak_initiator_document =
        initiator_document ? initiator_document->GetWeakDocumentPtr()
                           : content::WeakDocumentPtr();

    webtorrent::HandleMagnetProtocol(url, web_contents_getter, page_transition,
                                     has_user_gesture, is_in_fenced_frame_tree,
                                     initiating_origin,
                                     std::move(weak_initiator_document));
    return true;
  }
#endif

  if (brave_rewards::IsRewardsProtocol(url)) {
    brave_rewards::HandleRewardsProtocol(url, web_contents_getter,
                                         page_transition, has_user_gesture);
    return true;
  }

  return ChromeContentBrowserClient::HandleExternalProtocol(
      url, web_contents_getter, frame_tree_node_id, navigation_data,
      is_primary_main_frame, is_in_fenced_frame_tree, sandbox_flags,
      page_transition, has_user_gesture, initiating_origin, initiator_document,
      out_factory);
}

void BraveContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  ChromeContentBrowserClient::AppendExtraCommandLineSwitches(command_line,
                                                             child_process_id);
  std::string process_type =
      command_line->GetSwitchValueASCII(switches::kProcessType);
  if (process_type == switches::kRendererProcess) {
    uint64_t session_token =
        12345;  // the kinda thing an idiot would have on his luggage

    // Command line parameters from the browser process are propagated to the
    // renderers *after* ContentBrowserClient::AppendExtraCommandLineSwitches()
    // is called from RenderProcessHostImpl::AppendRendererCommandLine(). This
    // means we have to inspect the main browser process' parameters for the
    // |switches::kTestType| as it will be too soon to find it on command_line.
    const base::CommandLine& browser_command_line =
        *base::CommandLine::ForCurrentProcess();
    if (!browser_command_line.HasSwitch(switches::kTestType)) {
      content::RenderProcessHost* process =
          content::RenderProcessHost::FromID(child_process_id);
      Profile* profile =
          process ? Profile::FromBrowserContext(process->GetBrowserContext())
                  : nullptr;
      session_token =
          g_brave_browser_process->brave_farbling_service()->session_token(
              profile && !profile->IsOffTheRecord());
    }
    command_line->AppendSwitchASCII("brave_session_token",
                                    base::NumberToString(session_token));

    // Switches to pass to render processes.
    static const char* const kSwitchNames[] = {
        translate::switches::kBraveTranslateUseGoogleEndpoint,
    };
    command_line->CopySwitchesFrom(browser_command_line, kSwitchNames,
                                   std::size(kSwitchNames));
  }
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
BraveContentBrowserClient::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    content::BrowserContext* browser_context,
    const content::WebContents::Getter& wc_getter,
    content::NavigationUIData* navigation_ui_data,
    int frame_tree_node_id) {
  auto result = ChromeContentBrowserClient::CreateURLLoaderThrottles(
      request, browser_context, wc_getter, navigation_ui_data,
      frame_tree_node_id);
  content::WebContents* contents = wc_getter.Run();

  if (contents) {
    const bool isMainFrame =
        request.resource_type ==
        static_cast<int>(blink::mojom::ResourceType::kMainFrame);
    // Speedreader
#if BUILDFLAG(ENABLE_SPEEDREADER)
    auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context));

    auto* tab_helper =
        speedreader::SpeedreaderTabHelper::FromWebContents(contents);
    if (tab_helper) {
      const auto state = tab_helper->PageDistillState();
      if (speedreader::PageWantsDistill(state) && isMainFrame) {
        // Only check for disabled sites if we are in Speedreader mode
        const bool check_disabled_sites =
            state == speedreader::DistillState::kSpeedreaderModePending;
        auto* speedreader_service =
            speedreader::SpeedreaderServiceFactory::GetForProfile(
                Profile::FromBrowserContext(browser_context));
        std::unique_ptr<speedreader::SpeedReaderThrottle> throttle =
            speedreader::SpeedReaderThrottle::MaybeCreateThrottleFor(
                g_brave_browser_process->speedreader_rewriter_service(),
                speedreader_service, settings_map, tab_helper->GetWeakPtr(),
                request.url, check_disabled_sites,
                base::ThreadTaskRunnerHandle::Get());
        if (throttle)
          result.push_back(std::move(throttle));
      }
    }
#endif  // ENABLE_SPEEDREADER

    if (isMainFrame) {
      // De-AMP
      if (auto de_amp_throttle = de_amp::DeAmpThrottle::MaybeCreateThrottleFor(
              base::ThreadTaskRunnerHandle::Get(), request, wc_getter)) {
        result.push_back(std::move(de_amp_throttle));
      }

      brave_ads::AdsService* ads_service =
          brave_ads::AdsServiceFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context));
      if (auto ads_status_header_throttle =
              brave_ads::AdsStatusHeaderThrottle::MaybeCreateThrottle(
                  ads_service, request)) {
        result.push_back(std::move(ads_status_header_throttle));
      }
    }
  }

  return result;
}

bool BraveContentBrowserClient::WillCreateURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    absl::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    bool* bypass_redirect_checks,
    bool* disable_secure_dns,
    network::mojom::URLLoaderFactoryOverridePtr* factory_override) {
  bool use_proxy = false;
  // TODO(iefremov): Skip proxying for certain requests?
  use_proxy = BraveProxyingURLLoaderFactory::MaybeProxyRequest(
      browser_context, frame,
      type == URLLoaderFactoryType::kNavigation ? -1 : render_process_id,
      factory_receiver);

  use_proxy |= ChromeContentBrowserClient::WillCreateURLLoaderFactory(
      browser_context, frame, render_process_id, type, request_initiator,
      std::move(navigation_id), ukm_source_id, factory_receiver, header_client,
      bypass_redirect_checks, disable_secure_dns, factory_override);

  return use_proxy;
}

bool BraveContentBrowserClient::WillInterceptWebSocket(
    content::RenderFrameHost* frame) {
  return (frame != nullptr);
}

void BraveContentBrowserClient::CreateWebSocket(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const absl::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client) {
  auto* proxy = BraveProxyingWebSocket::ProxyWebSocket(
      frame, std::move(factory), url, site_for_cookies, user_agent,
      std::move(handshake_client));

  if (ChromeContentBrowserClient::WillInterceptWebSocket(frame)) {
    ChromeContentBrowserClient::CreateWebSocket(
        frame, proxy->web_socket_factory(), url, site_for_cookies, user_agent,
        proxy->handshake_client().Unbind());
  } else {
    proxy->Start();
  }
}

void BraveContentBrowserClient::MaybeHideReferrer(
    content::BrowserContext* browser_context,
    const GURL& request_url,
    const GURL& document_url,
    blink::mojom::ReferrerPtr* referrer) {
  DCHECK(referrer && !referrer->is_null());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (document_url.SchemeIs(kChromeExtensionScheme) ||
      request_url.SchemeIs(kChromeExtensionScheme)) {
    return;
  }
#endif
  if (document_url.SchemeIs(content::kChromeUIScheme) ||
      request_url.SchemeIs(content::kChromeUIScheme)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  const bool allow_referrers = brave_shields::AreReferrersAllowed(
      HostContentSettingsMapFactory::GetForProfile(profile), document_url);
  const bool shields_up = brave_shields::GetBraveShieldsEnabled(
      HostContentSettingsMapFactory::GetForProfile(profile), document_url);

  content::Referrer new_referrer;
  if (brave_shields::MaybeChangeReferrer(allow_referrers, shields_up,
                                         (*referrer)->url, request_url,
                                         &new_referrer)) {
    (*referrer)->url = new_referrer.url;
    (*referrer)->policy = new_referrer.policy;
  }
}

GURL BraveContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile)
    return url;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  return ChromeContentBrowserClientExtensionsPart::GetEffectiveURL(profile,
                                                                   url);
#else
  return url;
#endif
}

// [static]
bool BraveContentBrowserClient::HandleURLOverrideRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (url->host() == chrome::kChromeUISyncHost) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(content::kChromeUIScheme);
    replacements.SetHostStr(chrome::kChromeUISettingsHost);
    replacements.SetPathStr(kBraveSyncPath);
    *url = url->ReplaceComponents(replacements);
    return true;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (url->host() == kAdblockHost) {
    GURL::Replacements replacements;
    replacements.SetSchemeStr(content::kChromeUIScheme);
    replacements.SetHostStr(chrome::kChromeUISettingsHost);
    replacements.SetPathStr(kContentFiltersPath);
    *url = url->ReplaceComponents(replacements);
    return false;
  }
#endif

  // no special win10 welcome page
  if (url->host() == chrome::kChromeUIWelcomeHost) {
    *url = GURL(chrome::kChromeUIWelcomeURL);
    return true;
  }

#if BUILDFLAG(ETHEREUM_REMOTE_CLIENT_ENABLED) && BUILDFLAG(ENABLE_EXTENSIONS)
  auto* prefs = user_prefs::UserPrefs::Get(browser_context);
  brave_wallet::mojom::DefaultWallet default_wallet =
      brave_wallet::GetDefaultEthereumWallet(prefs);
  if (!brave_wallet::IsNativeWalletEnabled() ||
      default_wallet == brave_wallet::mojom::DefaultWallet::CryptoWallets) {
    // If the Crypto Wallets extension is loaded, then it replaces the WebUI
    auto* service =
        EthereumRemoteClientServiceFactory::GetForContext(browser_context);
    if (service->IsCryptoWalletsReady() &&
        url->SchemeIs(content::kChromeUIScheme) &&
        url->host() == ethereum_remote_client_host) {
      auto* registry = extensions::ExtensionRegistry::Get(browser_context);
      if (registry->ready_extensions().GetByID(
              ethereum_remote_client_extension_id)) {
        *url = GURL(ethereum_remote_client_base_url);
        return true;
      }
    }
  }
#endif

  return false;
}

std::vector<std::unique_ptr<content::NavigationThrottle>>
BraveContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles =
      ChromeContentBrowserClient::CreateThrottlesForNavigation(handle);

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<content::NavigationThrottle> ntp_shows_navigation_throttle =
      NewTabShowsNavigationThrottle::MaybeCreateThrottleFor(handle);
  if (ntp_shows_navigation_throttle)
    throttles.push_back(std::move(ntp_shows_navigation_throttle));
#endif

#if BUILDFLAG(ENABLE_BRAVE_WEBTORRENT)
  throttles.push_back(
      std::make_unique<extensions::BraveWebTorrentNavigationThrottle>(handle));
#endif

  content::BrowserContext* context =
      handle->GetWebContents()->GetBrowserContext();

#if BUILDFLAG(ENABLE_TOR)
  std::unique_ptr<content::NavigationThrottle> tor_navigation_throttle =
      tor::TorNavigationThrottle::MaybeCreateThrottleFor(handle,
                                                         context->IsTor());
  if (tor_navigation_throttle)
    throttles.push_back(std::move(tor_navigation_throttle));
  std::unique_ptr<tor::OnionLocationNavigationThrottleDelegate>
      onion_location_navigation_throttle_delegate =
          std::make_unique<tor::OnionLocationNavigationThrottleDelegate>();
  std::unique_ptr<content::NavigationThrottle>
      onion_location_navigation_throttle =
          tor::OnionLocationNavigationThrottle::MaybeCreateThrottleFor(
              handle, TorProfileServiceFactory::IsTorDisabled(),
              std::move(onion_location_navigation_throttle_delegate),
              context->IsTor());
  if (onion_location_navigation_throttle)
    throttles.push_back(std::move(onion_location_navigation_throttle));
#endif

#if BUILDFLAG(ENABLE_IPFS)
  throttles.insert(
      throttles.begin(),
      ipfs::IpfsSubframeNavigationThrottle::CreateThrottleFor(handle));
  std::unique_ptr<content::NavigationThrottle> ipfs_navigation_throttle =
      ipfs::IpfsNavigationThrottle::MaybeCreateThrottleFor(
          handle, ipfs::IpfsServiceFactory::GetForContext(context),
          user_prefs::UserPrefs::Get(context),
          g_browser_process->GetApplicationLocale());
  if (ipfs_navigation_throttle)
    throttles.push_back(std::move(ipfs_navigation_throttle));
#endif

  std::unique_ptr<content::NavigationThrottle>
      decentralized_dns_navigation_throttle =
          decentralized_dns::DecentralizedDnsNavigationThrottle::
              MaybeCreateThrottleFor(handle, g_browser_process->local_state(),
                                     g_browser_process->GetApplicationLocale());
  if (decentralized_dns_navigation_throttle)
    throttles.push_back(std::move(decentralized_dns_navigation_throttle));

  if (std::unique_ptr<
          content::NavigationThrottle> domain_block_navigation_throttle =
          brave_shields::DomainBlockNavigationThrottle::MaybeCreateThrottleFor(
              handle, g_brave_browser_process->ad_block_service(),
              g_brave_browser_process->ad_block_service()
                  ->custom_filters_provider(),
              EphemeralStorageServiceFactory::GetForContext(context),
              HostContentSettingsMapFactory::GetForProfile(
                  Profile::FromBrowserContext(context)),
              g_browser_process->GetApplicationLocale()))
    throttles.push_back(std::move(domain_block_navigation_throttle));

  // Debounce
  if (auto debounce_throttle =
          debounce::DebounceNavigationThrottle::MaybeCreateThrottleFor(
              handle,
              debounce::DebounceServiceFactory::GetForBrowserContext(context)))
    throttles.push_back(std::move(debounce_throttle));

  return throttles;
}

bool PreventDarkModeFingerprinting(WebContents* web_contents,
                                   WebPreferences* prefs) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const GURL url = web_contents->GetLastCommittedURL();
  const bool shields_up = brave_shields::GetBraveShieldsEnabled(
      HostContentSettingsMapFactory::GetForProfile(profile), url);
  auto fingerprinting_type = brave_shields::GetFingerprintingControlType(
      HostContentSettingsMapFactory::GetForProfile(profile), url);
  // https://github.com/brave/brave-browser/issues/15265
  // Always use color scheme Light if fingerprinting mode strict
  if (base::FeatureList::IsEnabled(
          brave_shields::features::kBraveDarkModeBlock) &&
      shields_up && fingerprinting_type == ControlType::BLOCK &&
      prefs->preferred_color_scheme !=
          blink::mojom::PreferredColorScheme::kLight) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
    return true;
  }
  return false;
}

bool BraveContentBrowserClient::OverrideWebPreferencesAfterNavigation(
    WebContents* web_contents,
    WebPreferences* prefs) {
  bool changed =
      ChromeContentBrowserClient::OverrideWebPreferencesAfterNavigation(
          web_contents, prefs);
  return PreventDarkModeFingerprinting(web_contents, prefs) || changed;
}

void BraveContentBrowserClient::OverrideWebkitPrefs(WebContents* web_contents,
                                                    WebPreferences* web_prefs) {
  ChromeContentBrowserClient::OverrideWebkitPrefs(web_contents, web_prefs);
  PreventDarkModeFingerprinting(web_contents, web_prefs);
  // This will stop NavigatorPlugins from returning fixed plugins data and will
  // allow us to return our farbled data
  web_prefs->allow_non_empty_navigator_plugins = true;

#if BUILDFLAG(ENABLE_PLAYLIST)
  if (auto* playlist_service =
          playlist::PlaylistServiceFactory::GetForBrowserContext(
              web_contents->GetBrowserContext())) {
    playlist_service->ConfigureWebPrefsForBackgroundWebContents(web_contents,
                                                                web_prefs);
  }
#endif
}

blink::UserAgentMetadata BraveContentBrowserClient::GetUserAgentMetadata() {
  blink::UserAgentMetadata metadata =
      ChromeContentBrowserClient::GetUserAgentMetadata();
  // Expect the brand version lists to have brand version, chromium_version, and
  // greased version.
  DCHECK_EQ(3UL, metadata.brand_version_list.size());
  DCHECK_EQ(3UL, metadata.brand_full_version_list.size());
  // Zero out the last 3 version components in full version list versions.
  for (auto& brand_version : metadata.brand_full_version_list) {
    base::Version version(brand_version.version);
    brand_version.version =
        base::StrCat({base::NumberToString(version.components()[0]), ".0.0.0"});
  }
  // Zero out the last 3 version components in full version.
  base::Version version(metadata.full_version);
  metadata.full_version =
      base::StrCat({base::NumberToString(version.components()[0]), ".0.0.0"});
  return metadata;
}
