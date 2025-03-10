package org.chromium.chrome.browser.crypto_wallet.util;

import android.content.Context;
import android.graphics.Typeface;
import android.os.Build;
import android.text.Html;
import android.text.Spanned;
import android.util.TypedValue;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;

import org.chromium.chrome.R;

public class AndroidUtils {
    public static int getToolBarHeight(Context context) {
        TypedValue tv = new TypedValue();
        if (context.getTheme().resolveAttribute(android.R.attr.actionBarSize, tv, true)) {
            return TypedValue.complexToDimensionPixelSize(
                    tv.data, context.getResources().getDisplayMetrics());
        }
        return 0;
    }

    public static void disableViewsByIds(View view, int... ids) {
        if (view != null) {
            for (int id : ids) {
                disableView(view, id);
            }
        }
    }

    public static void disableView(View containerView, @IdRes int id) {
        if (containerView == null) return;
        View view = containerView.findViewById(id);
        if (view != null) {
            view.setEnabled(false);
            view.setClickable(false);
        }
    }

    public static Spanned formatHTML(String html) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return Html.fromHtml(html, Html.FROM_HTML_MODE_LEGACY);
        } else {
            return Html.fromHtml(html);
        }
    }

    // Views
    public static TextView makeHeaderTv(Context context) {
        TextView textView = new TextView(context);
        textView.setTextAppearance(R.style.BraveWalletTextViewTitle);
        textView.setTypeface(null, Typeface.BOLD);
        textView.setId(View.generateViewId());
        return textView;
    }

    public static TextView makeSubHeaderTv(Context context) {
        TextView textView = new TextView(context);
        textView.setTextAppearance(R.style.BraveWalletTextViewSubTitle);
        textView.setId(View.generateViewId());
        return textView;
    }

    public static void gone(View... views) {
        setViewVisibility(false, views);
    }

    public static void show(View... views) {
        setViewVisibility(true, views);
    }

    private static void setViewVisibility(boolean isVisible, View... views) {
        int visibility = isVisible ? View.VISIBLE : View.GONE;
        for (View view : views) {
            view.setVisibility(visibility);
        }
    }
}
