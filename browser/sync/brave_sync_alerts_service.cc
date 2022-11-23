/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/sync/brave_sync_alerts_service.h"

#include "brave/browser/infobars/brave_sync_account_deleted_infobar_delegate.h"
#include "brave/components/sync/driver/brave_sync_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/infobars/content/content_infobar_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#endif

using syncer::BraveSyncServiceImpl;

BraveSyncAlertsService::BraveSyncAlertsService(Profile* profile)
    : profile_(profile) {
  DCHECK(SyncServiceFactory::IsSyncAllowed(profile));

  if (SyncServiceFactory::IsSyncAllowed(profile)) {
    BraveSyncServiceImpl* service = static_cast<BraveSyncServiceImpl*>(
        SyncServiceFactory::GetForProfile(profile_));

    if (service) {
      DCHECK(!sync_service_observer_.IsObservingSource(service));
      sync_service_observer_.AddObservation(service);
    }
  }
}

BraveSyncAlertsService::~BraveSyncAlertsService() {}

void BraveSyncAlertsService::OnStateChanged(syncer::SyncService* service) {
  brave_sync::Prefs brave_sync_prefs(profile_->GetPrefs());
  if (!brave_sync_prefs.IsSyncAccountDeletedNoticePending()) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  ShowAndroidInfobar();
#else
  ShowDesktopInfobar();
#endif
}

void BraveSyncAlertsService::OnSyncShutdown(syncer::SyncService* sync_service) {
  if (sync_service_observer_.IsObservingSource(sync_service)) {
    sync_service_observer_.RemoveObservation(sync_service);
  }
}

#if BUILDFLAG(IS_ANDROID)

void BraveSyncAlertsService::ShowAndroidInfobar() {
  DCHECK(false);
}

#else

void BraveSyncAlertsService::ShowDesktopInfobar() {
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    content::WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (active_web_contents) {
      infobars::ContentInfoBarManager* infobar_manager =
          infobars::ContentInfoBarManager::FromWebContents(active_web_contents);

      BraveSyncAccountDeletedInfoBarDelegate::Create(infobar_manager, profile_,
                                                     browser);
    }
  }
}

#endif
