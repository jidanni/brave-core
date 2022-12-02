// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import { createEntityAdapter } from '@reduxjs/toolkit'

import { BraveWallet, WalletAccountTypeName } from '../../../constants/types'
import { RootStoreState } from '../../../page/store'
import { walletApi } from '../api.slice'

export type AccountInfoEntity = BraveWallet.AccountInfo & {
  accountType: WalletAccountTypeName
  deviceId: Exclude<BraveWallet.AccountInfo['hardware'], undefined>['deviceId']
}

export const accountInfoEntityAdaptor = createEntityAdapter<AccountInfoEntity>({
  selectId: (accountInfo) => accountInfo.address.toLowerCase()
})
export const accountInfoEntityAdaptorInitialState = accountInfoEntityAdaptor.getInitialState()
export type AccountInfoEntityState = typeof accountInfoEntityAdaptorInitialState

// Selectors from Root State
export const {
  selectAll: selectAllAccountInfos,
  selectById: selectAccountInfoById,
  selectEntities: selectAccountInfoEntities,
  selectIds: selectAccountInfoIds,
  selectTotal: selectTotalAccountInfos
} = accountInfoEntityAdaptor.getSelectors((state: RootStoreState) => {
  return (
    walletApi.endpoints.getAccountInfosRegistry.select()(state)?.data ??
    accountInfoEntityAdaptor.getInitialState()
  )
})
