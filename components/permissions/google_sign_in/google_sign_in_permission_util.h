// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef BRAVE_COMPONENTS_PERMISSIONS_GOOGLE_SIGN_IN_GOOGLE_SIGN_IN_PERMISSION_UTIL_H_
#define BRAVE_COMPONENTS_PERMISSIONS_GOOGLE_SIGN_IN_GOOGLE_SIGN_IN_PERMISSION_UTIL_H_

#include <string>
#include <vector>

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace permissions {

bool IsGoogleAuthRelatedRequest(const GURL& request_url,
                                const GURL& request_initiator_url);
blink::mojom::PermissionStatus GetCurrentGoogleSignInPermissionStatus(
    content::PermissionControllerDelegate* permission_controller,
    content::WebContents* contents,
    const GURL& request_initiator_url);

// Check if feature flag is enabled.
bool IsGoogleSignInFeatureEnabled();
// Check if user preference is enabled (default ON). Caller should make sure
// feature flag is enabled.
bool IsGoogleSignInPrefEnabled(PrefService* prefs);
void Set3pCookieException(HostContentSettingsMap* content_settings,
                          const ContentSettingsPattern& embedding_pattern,
                          const ContentSetting& content_setting);
void HandleBraveGoogleSignInPermissionStatus(
    content::BrowserContext* context,
    const GURL& request_initiator_url,
    scoped_refptr<HostContentSettingsMap> content_settings,
    const std::vector<blink::mojom::PermissionStatus>& permission_statuses);
void CreateGoogleSignInPermissionRequest(
    bool* defer,
    content::PermissionControllerDelegate* permission_controller,
    content::RenderFrameHost* rfh,
    const GURL& request_initiator_url,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback);
bool CanCreateWindow(content::RenderFrameHost* opener,
                     const GURL& opener_url,
                     const GURL& target_url);
bool GetPermissionAndMaybeCreatePrompt(
    content::WebContents* contents,
    const GURL& request_initiator_url,
    bool* defer,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        permission_result_callback,
    base::OnceCallback<void()> denied_callback);

}  // namespace permissions

#endif  // BRAVE_COMPONENTS_PERMISSIONS_GOOGLE_SIGN_IN_GOOGLE_SIGN_IN_PERMISSION_UTIL_H_
