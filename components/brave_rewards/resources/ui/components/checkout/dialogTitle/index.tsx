/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { StyledHeader } from './style'

interface DialogTitleProps {
  children: React.ReactNode
}

export function DialogTitle (props: DialogTitleProps) {
  return <StyledHeader>{props.children}</StyledHeader>
}
