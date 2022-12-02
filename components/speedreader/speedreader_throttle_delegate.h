/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_SPEEDREADER_SPEEDREADER_RESULT_DELEGATE_H_
#define BRAVE_COMPONENTS_SPEEDREADER_SPEEDREADER_RESULT_DELEGATE_H_

namespace speedreader {

// SpeedreaderThrottleDelegate is an interface for the speedreader component to
// notify a tab_helper.
class SpeedreaderThrottleDelegate {
 public:
  virtual ~SpeedreaderThrottleDelegate() = default;

  virtual bool IsPageDistillationAllowed() = 0;
  virtual void OnDistillComplete() = 0;
};

}  // namespace speedreader

#endif  // BRAVE_COMPONENTS_SPEEDREADER_SPEEDREADER_RESULT_DELEGATE_H_
