/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_BRAVE_ADS_NOTIFICATION_AD_POPUP_H_
#define BRAVE_BROWSER_UI_VIEWS_BRAVE_ADS_NOTIFICATION_AD_POPUP_H_

#include <cstdint>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "brave/browser/ui/brave_ads/notification_ad.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class Profile;

namespace gfx {
class LinearAnimation;
class Insets;
class Point;
class Rect;
class Size;
struct ShadowDetails;
class Vector2d;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace brave_ads {

class NotificationAdView;

// The widget delegate of an notification ad popup. The view is owned by the
// widget
class NotificationAdPopup : public views::WidgetDelegateView,
                            public views::WidgetObserver,
                            public gfx::AnimationDelegate,
                            public display::DisplayObserver {
 public:
  METADATA_HEADER(NotificationAdPopup);

  NotificationAdPopup(Profile* profile,
                      const NotificationAd& notification_ad,
                      gfx::NativeWindow browser_native_window,
                      gfx::NativeView browser_native_view);

  NotificationAdPopup(const NotificationAdPopup&) = delete;
  NotificationAdPopup& operator=(const NotificationAdPopup&) = delete;

  NotificationAdPopup(NotificationAdPopup&& other) noexcept = delete;
  NotificationAdPopup& operator=(NotificationAdPopup&& other) noexcept = delete;

  ~NotificationAdPopup() override;

  // Disables fade in animation for snapshot tests.
  static void SetDisableFadeInAnimationForTesting(bool disable);

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // views::WidgetDelegateView:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnDisplayChanged() override;
  void OnWorkAreaChanged() override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  NotificationAd GetNotificationAd() const;
  void MovePopup(const gfx::Vector2d& distance);
  void ClosePopup();

 private:
  enum class AnimationState {
    // No animation is running
    kIdle,

    // Fading in a notification ad
    kFadeIn,

    // Fading out a notification ad
    kFadeOut
  };

  raw_ptr<Profile> profile_ = nullptr;  // NOT OWNED

  void CreatePopup(gfx::NativeWindow browser_native_window,
                   gfx::NativeView browser_native_view);

  NotificationAd notification_ad_;

  gfx::Point GetDefaultOriginForSize(const gfx::Size& size);
  gfx::Point GetOriginForSize(const gfx::Size& size);
  void SaveOrigin(const gfx::Point& origin) const;

  gfx::Size CalculateViewSize() const;
  gfx::Rect CalculateBounds();

  void RecomputeAlignment();

  const gfx::ShadowDetails& GetShadowDetails() const;
  gfx::Insets GetShadowMargin() const;

  void CreateWidgetView(gfx::NativeWindow browser_native_window,
                        gfx::NativeView browser_native_view);
  void CloseWidgetView();

  NotificationAdView* notification_ad_view_ = nullptr;  // NOT OWNED

  void FadeIn();
  void FadeOut();

  const std::unique_ptr<gfx::LinearAnimation> animation_;
  AnimationState animation_state_ = AnimationState::kIdle;
  void StartAnimation();
  void UpdateAnimation();

  bool IsWidgetValid() const;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

}  // namespace brave_ads

#endif  // BRAVE_BROWSER_UI_VIEWS_BRAVE_ADS_NOTIFICATION_AD_POPUP_H_
