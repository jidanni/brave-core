// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import styled from 'styled-components'

import { WalletButton } from '../../shared/style'
import Background from '../../../assets/svg-icons/nft-ipfs/local-node-background.svg'
import Ipfs from '../../../assets/svg-icons/nft-ipfs/ipfs.svg'
import Close from '../../../assets/svg-icons/close.svg'

export const StyledWrapper = styled.div`
  display: flex;
  flex-direction: column;
  width: 100vw;
  height: 100vh;
  background: url(${Background}) bottom 0 center/contain no-repeat, linear-gradient(110.74deg, #242464 -10.97%, #000027 173.98%);
  padding-top: 68px;
  padding-right: 122px;
  `

export const TopRow = styled.div`
  display: flex;
  flex-direction: column;
  align-items: center;
  width: 100%;
  height: 10px;
`

export const MainContent = styled.div`
  display: flex;
  flex-direction: column;
  align-self: center;
  justify-content: center;
  width: 100%;
  padding-left: 160px;
  padding-right: 219px;
`

export const CloseButton = styled(WalletButton)`
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  font-family: 'Poppins';
  font-style: normal;
  font-weight: 600;
  font-size: 13px;
  line-height: 20px;
  background-color: transparent;
  color: ${p => p.theme.palette.white};
  outline: none;
  border: none;
  margin-left: auto
`

export const CloseIcon = styled.div`
  display: flex;
  justify-content: center;
  align-items: center;
  width: 18px;
  height: 18px;
  background-color: ${p => p.theme.palette.white};
  -webkit-mask-image: url(${Close});
  mask-image: url(${Close});
  margin-left: 10px; 
`

export const HeadingWrapper = styled.div`
  display: flex;
  flex-direction: row;
  width: 100%;
  justify-content: flex-start;
  align-items: center;
  margin-top: 36px;
`

export const Heading = styled.h1`
  font-family: 'Poppins';
  font-style: normal;
  font-weight: 600;
  font-size: 18px;
  line-height: 26px;
  color: ${p => p.theme.palette.white};
  margin: 0;
  padding: 0;
`

export const IpfsIcon = styled.div`
display: flex;
  justify-content: center;
  align-items: center;
  width: 56px;
  height: 56px;
  background-color: ${p => p.theme.palette.white};
  -webkit-mask-image: url(${Ipfs});
  mask-image: url(${Ipfs});
  mask-repeat: no-repeat;
  mask-size: 56px;
  margin-right: 16px; 
  `

  export const Section = styled.section`
  display: flex;
  flex-direction: row;
  width: 100%;
  justify-content: space-between;
  gap: 36px;
  margin: 50px 0 0 0;
`

export const SectionText = styled.h2`
  font-family: 'Poppins';
  font-style: normal;
  font-weight: 600;
  font-size: 38px;
  line-height: 60px;
  color: ${p => p.theme.palette.white};
  margin: 0;
  padding: 0;
  flex: 1;
  flex-basis: 522px;
`

export const Description = styled.span`
  font-family: 'Poppins';
  font-style: normal;
  font-weight: 400;
  font-size: 14px;
  line-height: 32px;
  text-align: left;
  color: ${p => p.theme.palette.white};
  margin: 0;
  padding: 0;
  width: 50%;
`
