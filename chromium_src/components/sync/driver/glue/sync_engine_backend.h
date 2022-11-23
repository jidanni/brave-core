/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_CHROMIUM_SRC_COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_
#define BRAVE_CHROMIUM_SRC_COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_

// chromium_src/components/sync/engine/sync_engine.h also redefines
// DisableProtocolEventForwarding include explicitly it to avoid compilation
// 'DisableProtocolEventForwarding' macro redefined
#include "components/sync/engine/sync_engine.h"

#define DisableProtocolEventForwarding                  \
  PermanentlyDeleteAccount(base::OnceClosure callback); \
  void DisableProtocolEventForwarding

#include "src/components/sync/driver/glue/sync_engine_backend.h"

#undef DisableProtocolEventForwarding

#endif  // BRAVE_CHROMIUM_SRC_COMPONENTS_SYNC_DRIVER_GLUE_SYNC_ENGINE_BACKEND_H_
