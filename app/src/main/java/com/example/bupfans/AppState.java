package com.example.bupfans;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.SystemClock;

import java.util.Date;
import java.util.UUID;

final class AppState {
    static final long DEFAULT_REFRESH_INTERVAL_MS = 60_000L;
    static final long[] REFRESH_INTERVAL_OPTIONS_MS = new long[]{60_000L, 300_000L, 900_000L};
    static final String[] REFRESH_INTERVAL_LABELS = new String[]{"1分钟", "5分钟", "15分钟"};
    private static final long MIN_BACKGROUND_WIDGET_INTERVAL_MS = 60_000L;

    private static final String PREFS = "b_up_fans_state";
    private static final String KEY_INTERVAL = "refresh_interval_ms";
    private static final String KEY_TARGET_NAME = "target_name";
    private static final String KEY_TARGET_MID = "target_mid";
    private static final String KEY_NAME = "latest_name";
    private static final String KEY_MID = "latest_mid";
    private static final String KEY_FOLLOWERS = "latest_followers";
    private static final String KEY_UPDATED_AT = "latest_updated_at";
    private static final String KEY_REFRESH_TOKEN = "refresh_token";
    private static final String KEY_LAST_WIDGET_TAP_REFRESH_AT = "last_widget_tap_refresh_at";
    private static final String EXTRA_REFRESH_TOKEN = "com.example.bupfans.extra.REFRESH_TOKEN";
    private static final String EXTRA_MANUAL_REFRESH = "com.example.bupfans.extra.MANUAL_REFRESH";
    private static final long WIDGET_TAP_REFRESH_THROTTLE_MS = 10_000L;

    private AppState() {
    }

    static long loadRefreshInterval(Context context) {
        long value = prefs(context).getLong(KEY_INTERVAL, DEFAULT_REFRESH_INTERVAL_MS);
        for (long option : REFRESH_INTERVAL_OPTIONS_MS) {
            if (option == value) {
                return value;
            }
        }
        return DEFAULT_REFRESH_INTERVAL_MS;
    }

    static void saveRefreshInterval(Context context, long intervalMs) {
        prefs(context).edit().putLong(KEY_INTERVAL, intervalMs).apply();
        scheduleNextWidgetRefresh(context);
    }

    static BiliFansClient.Target loadTarget(Context context) {
        SharedPreferences preferences = prefs(context);
        long mid = preferences.getLong(KEY_TARGET_MID, BiliFansClient.DEFAULT_MID);
        if (mid <= 0L) {
            mid = BiliFansClient.DEFAULT_MID;
        }
        String name = preferences.getString(KEY_TARGET_NAME, BiliFansClient.DEFAULT_NAME);
        return new BiliFansClient.Target(name, mid);
    }

    static void saveTarget(Context context, BiliFansClient.Target target) {
        SharedPreferences preferences = prefs(context);
        SharedPreferences.Editor editor = preferences.edit()
                .putString(KEY_TARGET_NAME, target.name)
                .putLong(KEY_TARGET_MID, target.mid);
        if (preferences.getLong(KEY_MID, BiliFansClient.DEFAULT_MID) != target.mid) {
            editor.remove(KEY_NAME)
                    .remove(KEY_MID)
                    .remove(KEY_FOLLOWERS)
                    .remove(KEY_UPDATED_AT);
        }
        editor.apply();
        scheduleNextWidgetRefresh(context);
    }

    static void saveLatest(Context context, BiliFansClient.FanResult result) {
        prefs(context).edit()
                .putString(KEY_TARGET_NAME, result.name)
                .putLong(KEY_TARGET_MID, result.mid)
                .putString(KEY_NAME, result.name)
                .putLong(KEY_MID, result.mid)
                .putInt(KEY_FOLLOWERS, result.followers)
                .putLong(KEY_UPDATED_AT, result.updatedAt.getTime())
                .apply();
    }

    static BiliFansClient.FanResult loadLatest(Context context) {
        SharedPreferences preferences = prefs(context);
        if (!preferences.contains(KEY_FOLLOWERS) || !preferences.contains(KEY_UPDATED_AT)) {
            return null;
        }
        BiliFansClient.Target target = loadTarget(context);
        if (preferences.getLong(KEY_MID, BiliFansClient.DEFAULT_MID) != target.mid) {
            return null;
        }
        return new BiliFansClient.FanResult(
                preferences.getString(KEY_NAME, target.name),
                preferences.getLong(KEY_MID, target.mid),
                preferences.getInt(KEY_FOLLOWERS, 0),
                new Date(preferences.getLong(KEY_UPDATED_AT, 0L))
        );
    }

    static String intervalLabel(long intervalMs) {
        for (int i = 0; i < REFRESH_INTERVAL_OPTIONS_MS.length; i++) {
            if (REFRESH_INTERVAL_OPTIONS_MS[i] == intervalMs) {
                return REFRESH_INTERVAL_LABELS[i];
            }
        }
        return "1分钟";
    }

    static void scheduleNextWidgetRefresh(Context context) {
        if (!hasAnyWidget(context)) {
            cancelWidgetRefresh(context);
            return;
        }
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarmManager == null) {
            return;
        }
        long intervalMs = Math.max(loadRefreshInterval(context), MIN_BACKGROUND_WIDGET_INTERVAL_MS);
        long triggerAt = SystemClock.elapsedRealtime() + intervalMs;
        alarmManager.setAndAllowWhileIdle(
                AlarmManager.ELAPSED_REALTIME_WAKEUP,
                triggerAt,
                refreshPendingIntent(context)
        );
    }

    static void cancelWidgetRefresh(Context context) {
        AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        if (alarmManager != null) {
            alarmManager.cancel(refreshPendingIntent(context));
        }
    }

    static boolean hasAnyWidget(Context context) {
        AppWidgetManager manager = AppWidgetManager.getInstance(context);
        return manager.getAppWidgetIds(new ComponentName(context, FansWidgetProvider.class)).length > 0
                || manager.getAppWidgetIds(new ComponentName(context, FansGlanceCardProvider.class)).length > 0;
    }

    static Intent widgetRefreshIntent(Context context) {
        return widgetRefreshIntent(context, false);
    }

    static Intent manualWidgetRefreshIntent(Context context) {
        return widgetRefreshIntent(context, true);
    }

    private static Intent widgetRefreshIntent(Context context, boolean manual) {
        Intent intent = new Intent(context, FansWidgetProvider.class);
        intent.setAction(FansWidgetProvider.ACTION_REFRESH);
        intent.putExtra(EXTRA_REFRESH_TOKEN, refreshToken(context));
        intent.putExtra(EXTRA_MANUAL_REFRESH, manual);
        return intent;
    }

    static boolean isTrustedRefreshIntent(Context context, Intent intent) {
        return refreshToken(context).equals(intent.getStringExtra(EXTRA_REFRESH_TOKEN));
    }

    static boolean isManualRefreshIntent(Intent intent) {
        return intent.getBooleanExtra(EXTRA_MANUAL_REFRESH, false);
    }

    static boolean markWidgetTapRefreshAllowed(Context context) {
        SharedPreferences preferences = prefs(context);
        long now = SystemClock.elapsedRealtime();
        long last = preferences.getLong(KEY_LAST_WIDGET_TAP_REFRESH_AT, 0L);
        if (last > 0L && now >= last && now - last < WIDGET_TAP_REFRESH_THROTTLE_MS) {
            return false;
        }
        preferences.edit().putLong(KEY_LAST_WIDGET_TAP_REFRESH_AT, now).apply();
        return true;
    }

    private static PendingIntent refreshPendingIntent(Context context) {
        return PendingIntent.getBroadcast(
                context,
                42,
                widgetRefreshIntent(context),
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
    }

    @SuppressLint("ApplySharedPref")
    private static String refreshToken(Context context) {
        SharedPreferences preferences = prefs(context);
        String token = preferences.getString(KEY_REFRESH_TOKEN, null);
        if (token != null) {
            return token;
        }
        token = UUID.randomUUID().toString();
        preferences.edit().putString(KEY_REFRESH_TOKEN, token).commit();
        return token;
    }

    private static SharedPreferences prefs(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
