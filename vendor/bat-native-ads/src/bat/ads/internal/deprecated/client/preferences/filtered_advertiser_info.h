/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_DEPRECATED_CLIENT_PREFERENCES_FILTERED_ADVERTISER_INFO_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_DEPRECATED_CLIENT_PREFERENCES_FILTERED_ADVERTISER_INFO_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace ads {

struct FilteredAdvertiserInfo final {
  base::Value::Dict ToValue() const;
  void FromValue(const base::Value::Dict& root);

  std::string id;
};

using FilteredAdvertiserList = std::vector<FilteredAdvertiserInfo>;

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_DEPRECATED_CLIENT_PREFERENCES_FILTERED_ADVERTISER_INFO_H_
