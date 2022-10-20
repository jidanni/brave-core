// Copyright (c) 2022 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js'
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js'
import {PrefsMixin, PrefsMixinInterface} from '../prefs/prefs_mixin.js'
import '../settings_shared.css.js'
import '../settings_vars.css.js'
import {getTemplate} from './bookmark_bar.html.js'

const SettingsBraveAppearanceBookmarkBarElementBase = PrefsMixin(I18nMixin(PolymerElement)) as {
  new (): PolymerElement & I18nMixinInterface & PrefsMixinInterface
}

enum BookmarkBarState {
  ALWAYS = 0,
  NONE = 1,
  NTP = 2,
}


/**
 * 'settings-brave-appearance-bookmark-bar' is the settings page area containing
 * brave's bookmark bar visibility settings in appearance settings.
 */
export class SettingsBraveAppearanceBookmarkBarElement extends SettingsBraveAppearanceBookmarkBarElementBase {
  static get is() {
    return 'settings-brave-appearance-bookmark-bar'
  }

  static get template() {
    return getTemplate()
  }

  static get properties() {
    return {
      bookmarkBarShowOptions_: {
        readOnly: false,
        type: String,
      },

      /** @private {chrome.settingsPrivate.PrefType} */
      bookmarkBarStatePref_: {
        key: '',
        type: Object,
        value() {
          return {
            key: '',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: BookmarkBarState.ALWAYS
          }
        }
      }
    }
  }
  bookmarkBarStatePref_: chrome.settingsPrivate.PrefObject
  private bookmarkBarShowOptions_ = [
    {value: BookmarkBarState.ALWAYS, name: this.i18n('appearanceSettingsBookmarBarAlways')},
    {value: BookmarkBarState.NONE, name: this.i18n('appearanceSettingsBookmarBarNever')},
    {value: BookmarkBarState.NTP, name: this.i18n('appearanceSettingsBookmarBarNTP')}
  ]
  private bookmarkBarShowEnabledLabel_: string

  static get observers() {
    return [
      'onShowOptionChanged_(prefs)'
    ]
  }

  private getBookmarkBarState(): BookmarkBarState {
    if (this.get('prefs.bookmark_bar.show_on_all_tabs.value'))
      return BookmarkBarState.ALWAYS

    if (this.get('prefs.brave.always_show_bookmark_bar_on_ntp.value'))
      return BookmarkBarState.NTP
    return BookmarkBarState.NONE
  }
  private setBookmarkBarState(state: BookmarkBarState) {
    if (state === BookmarkBarState.ALWAYS) {
      this.set('prefs.bookmark_bar.show_on_all_tabs.value', true)
    } else if (state === BookmarkBarState.NTP) {
      this.set('prefs.bookmark_bar.show_on_all_tabs.value', false)
      this.set('prefs.brave.always_show_bookmark_bar_on_ntp.value', true)
    } else {
      this.set('prefs.bookmark_bar.show_on_all_tabs.value', false)
      this.set('prefs.brave.always_show_bookmark_bar_on_ntp.value', false)
    }
  }
  private onShowOptionChanged_() {
    this.setBookmarkBarState(this.bookmarkBarStatePref_.value)

    const state = this.getBookmarkBarState()
    if (state === BookmarkBarState.ALWAYS) {
      this.bookmarkBarShowEnabledLabel_ = this.i18n('appearanceSettingsBookmarBarAlwaysDesc')
    } else if (state === BookmarkBarState.NTP) {
      this.bookmarkBarShowEnabledLabel_ = this.i18n('appearanceSettingsBookmarBarNTPDesc')
    } else {
      this.bookmarkBarShowEnabledLabel_ = this.i18n('appearanceSettingsBookmarBarNeverDesc')
    }
    const pref = {
      key: '',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: state
    };
    this.bookmarkBarStatePref_ = pref;
  }

}

customElements.define(SettingsBraveAppearanceBookmarkBarElement.is, SettingsBraveAppearanceBookmarkBarElement)
