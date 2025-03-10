// Copyright (c) 2019 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at https://mozilla.org/MPL/2.0/.

import * as preferencesAPI from './preferences'
import * as statsAPI from './stats'
import * as privateTabDataAPI from './privateTabData'
import * as wallpaper from './wallpaper'
import * as newTabAdsDataAPI from './newTabAdsData'
import getNTPBrowserAPI from './background'

export type InitialData = {
  preferences: NewTab.Preferences
  stats: statsAPI.Stats
  privateTabData: privateTabDataAPI.PrivateTabData
  wallpaperData?: NewTab.Wallpaper
  braveBackgrounds: NewTab.BraveBackground[]
  customImageBackgrounds: NewTab.ImageBackground[]
  braveRewardsSupported: boolean
  braveTalkSupported: boolean
  searchPromotionEnabled: boolean
}

export type PreInitialRewardsData = {
  rewardsEnabled: boolean
  isUnsupportedRegion: boolean
  declaredCountry: string
  enabledAds: boolean
  adsSupported: boolean
  needsBrowserUpgradeToServeAds: boolean
}

export type InitialRewardsData = {
  report: NewTab.RewardsBalanceReport
  balance: NewTab.RewardsBalance
  externalWallet?: RewardsExtension.ExternalWallet
  adsAccountStatement: NewTab.AdsAccountStatement
  parameters: NewTab.RewardsParameters
}

const isIncognito: boolean = chrome.extension.inIncognitoContext

// Gets all data required for the first render of the page
export async function getInitialData (): Promise<InitialData> {
  try {
    console.timeStamp('Getting initial data...')
    const [
      preferences,
      stats,
      privateTabData,
      wallpaperData,
      braveRewardsSupported,
      braveTalkSupported,
      searchPromotionEnabled,
      braveBackgrounds,
      customImageBackgrounds
    ] = await Promise.all([
      preferencesAPI.getPreferences(),
      statsAPI.getStats(),
      privateTabDataAPI.getPrivateTabData(),
      !isIncognito ? wallpaper.getWallpaper() : Promise.resolve(undefined),
      new Promise((resolve) => {
        chrome.braveRewards.isSupported((supported: boolean) => {
          resolve(supported)
        })
      }),
      new Promise((resolve) => {
        if (!('braveTalk' in chrome)) {
          resolve(false)
          return
        }

        chrome.braveTalk.isSupported((supported: boolean) => {
          resolve(supported)
        })
      }),
      getNTPBrowserAPI().pageHandler.isSearchPromotionEnabled().then(({ enabled }) => enabled),
      getNTPBrowserAPI().pageHandler.getBraveBackgrounds().then(({ backgrounds }) => {
        return backgrounds.map(background => ({ type: 'brave', wallpaperImageUrl: background.imageUrl.url, author: background.author, link: background.link.url }))
      }),
      getNTPBrowserAPI().pageHandler.getCustomImageBackgrounds().then(({ backgrounds }) => {
        return backgrounds.map(background => ({ type: 'image', wallpaperImageUrl: background.url.url }))
      })
    ])
    console.timeStamp('Got all initial data.')
    return {
      preferences,
      stats,
      privateTabData,
      wallpaperData,
      braveBackgrounds,
      customImageBackgrounds,
      braveRewardsSupported,
      braveTalkSupported,
      searchPromotionEnabled
    } as InitialData
  } catch (e) {
    console.error(e)
    throw Error('Error getting initial data')
  }
}

export async function getRewardsPreInitialData (): Promise<PreInitialRewardsData> {
  const [
    rewardsEnabled,
    isUnsupportedRegion,
    declaredCountry,
    enabledAds,
    adsSupported,
    adsData
  ] = await Promise.all([
    new Promise<boolean>(
      (resolve) => chrome.braveRewards.getRewardsEnabled(resolve)),
    new Promise<boolean>(
      (resolve) => chrome.braveRewards.isUnsupportedRegion(resolve)),
    new Promise<string>(
      (resolve) => chrome.braveRewards.getDeclaredCountry(resolve)),
    new Promise<boolean>(
      (resolve) => chrome.braveRewards.getAdsEnabled(resolve)),
    new Promise<boolean>(
      (resolve) => chrome.braveRewards.getAdsSupported(resolve)),
    newTabAdsDataAPI.getNewTabAdsData()
  ])

  const needsBrowserUpgradeToServeAds = adsData.needsBrowserUpgradeToServeAds

  return {
    rewardsEnabled,
    isUnsupportedRegion,
    declaredCountry,
    enabledAds,
    adsSupported,
    needsBrowserUpgradeToServeAds
  }
}

export async function getRewardsInitialData (): Promise<InitialRewardsData> {
  try {
    const [
      adsAccountStatement,
      report,
      balance,
      parameters,
      externalWallet
    ] = await Promise.all([
      new Promise(resolve => chrome.braveRewards.getAdsAccountStatement((success: boolean, adsAccountStatement: NewTab.AdsAccountStatement) => {
        resolve(success ? adsAccountStatement : undefined)
      })),
      new Promise(resolve => chrome.braveRewards.getBalanceReport(new Date().getMonth() + 1, new Date().getFullYear(), (report: NewTab.RewardsBalanceReport) => {
        resolve(report)
      })),
      new Promise(resolve => chrome.braveRewards.fetchBalance((balance: NewTab.RewardsBalance) => {
        resolve(balance)
      })),
      new Promise(resolve => chrome.braveRewards.getRewardsParameters((parameters: NewTab.RewardsParameters) => {
        resolve(parameters)
      })),
      new Promise(resolve => {
        chrome.braveRewards.getExternalWallet((wallet) => resolve(wallet))
      }),
      new Promise(resolve => {
        chrome.braveRewards.fetchPromotions(resolve)
      })
    ])
    return {
      adsAccountStatement,
      report,
      balance,
      parameters,
      externalWallet
    } as InitialRewardsData
  } catch (err) {
    throw Error(err)
  }
}
