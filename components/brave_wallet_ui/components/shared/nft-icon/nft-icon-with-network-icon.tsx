// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as React from 'react'

// types
import { BraveWallet } from '../../../constants/types'

// components
import CreateNetworkIcon from '../create-network-icon'
import { NftIcon, NftIconProps } from './nft-icon'

// styles
import { NetworkIconWrapper } from '../style'
import { NftIconWrapper, IconWrapper } from './nft-icon-styles'

interface Props extends NftIconProps {
  size?: string | number
  tokensNetwork: BraveWallet.NetworkInfo
  disabled?: boolean
}

const defaultSize = 120

export const NftIconWithNetworkIcon = (props: Props) => {
  const { size, tokensNetwork, disabled } = props

  return (
    <NftIconWrapper
      style={{ width: size || defaultSize, height: size || defaultSize }}
    >
      <NftIcon {...props} />
      <IconWrapper disabled={disabled}>
        <NetworkIconWrapper>
          <CreateNetworkIcon size='big' network={tokensNetwork} marginRight={0} />
        </NetworkIconWrapper>
      </IconWrapper>
    </NftIconWrapper>
  )
}
