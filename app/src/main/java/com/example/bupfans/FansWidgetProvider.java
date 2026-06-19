package com.example.bupfans;

import android.app.PendingIntent;
import android.appwidget.AppWidgetManager;
import android.appwidget.AppWidgetProvider;
import android.content.BroadcastReceiver.PendingResult;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.TextUtils;
import android.widget.RemoteViews;

import java.text.NumberFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class FansWidgetProvider extends AppWidgetProvider {
    static final String ACTION_REFRESH = "com.example.bupfans.action.REFRESH_WIDGET";
    private static final ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();
    private static final AtomicBoolean IS_REFRESHING = new AtomicBoolean(false);

    @Override
    public void onUpdate(Context context, AppWidgetManager manager, int[] appWidgetIds) {
        showCachedOrLoading(context, manager, appWidgetIds, getClass());
        refresh(context.getApplicationContext());
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (ACTION_REFRESH.equals(action)) {
            if (!AppState.isTrustedRefreshIntent(context, intent)) {
                return;
            }
            AppWidgetManager manager = AppWidgetManager.getInstance(context);
            if (AppState.isManualRefreshIntent(intent) && !AppState.markWidgetTapRefreshAllowed(context)) {
                showStatusAll(context, manager, "刚刚刷新");
                return;
            }
            PendingResult pending = goAsync();
            showLoadingAll(context, manager);
            refresh(context.getApplicationContext(), pending);
        } else if (AppWidgetManager.ACTION_APPWIDGET_UPDATE.equals(action)) {
            PendingResult pending = goAsync();
            AppWidgetManager manager = AppWidgetManager.getInstance(context);
            int[] ids = intent.getIntArrayExtra(AppWidgetManager.EXTRA_APPWIDGET_IDS);
            if (ids == null) {
                ids = manager.getAppWidgetIds(new ComponentName(context, getClass()));
            }
            showCachedOrLoading(context, manager, ids, getClass());
            refresh(context.getApplicationContext(), pending);
        } else if (AppWidgetManager.ACTION_APPWIDGET_ENABLED.equals(action)) {
            onEnabled(context);
        } else if (AppWidgetManager.ACTION_APPWIDGET_DISABLED.equals(action)) {
            onDisabled(context);
        } else if (Intent.ACTION_BOOT_COMPLETED.equals(action) || Intent.ACTION_MY_PACKAGE_REPLACED.equals(action)) {
            AppState.scheduleNextWidgetRefresh(context);
        } else {
            super.onReceive(context, intent);
        }
    }

    private void showCachedOrLoading(Context context, AppWidgetManager manager, int[] appWidgetIds, Class<?> provider) {
        BiliFansClient.FanResult cached = AppState.loadLatest(context);
        if (cached == null) {
            showLoading(context, manager, appWidgetIds, provider);
        } else {
            for (int id : appWidgetIds) {
                manager.updateAppWidget(id, views(context, provider, cached, "上次统计"));
            }
        }
    }

    @Override
    public void onEnabled(Context context) {
        super.onEnabled(context);
        AppState.scheduleNextWidgetRefresh(context);
    }

    @Override
    public void onDisabled(Context context) {
        super.onDisabled(context);
        if (!AppState.hasAnyWidget(context)) {
            AppState.cancelWidgetRefresh(context);
        }
    }

    static void refresh(Context context) {
        refresh(context, null);
    }

    private static void refresh(Context context, final PendingResult pending) {
        final Context appContext = context.getApplicationContext();
        if (!IS_REFRESHING.compareAndSet(false, true)) {
            if (pending != null) {
                pending.finish();
            }
            return;
        }
        EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    BiliFansClient.FanResult result = BiliFansClient.fetchFanResult(AppState.loadTarget(appContext));
                    AppState.saveLatest(appContext, result);
                    updateAllFromResult(appContext, result);
                    AppState.scheduleNextWidgetRefresh(appContext);
                } catch (Exception error) {
                    AppWidgetManager manager = AppWidgetManager.getInstance(appContext);
                    String message = friendlyError(error);
                    for (Class<?> provider : providerClasses()) {
                        int[] ids = manager.getAppWidgetIds(new ComponentName(appContext, provider));
                        for (int id : ids) {
                            manager.updateAppWidget(id, errorViews(appContext, provider, message));
                        }
                    }
                    AppState.scheduleNextWidgetRefresh(appContext);
                } finally {
                    IS_REFRESHING.set(false);
                    if (pending != null) {
                        pending.finish();
                    }
                }
            }
        });
    }

    static void updateAllFromResult(Context context, BiliFansClient.FanResult result) {
        Context appContext = context.getApplicationContext();
        AppWidgetManager manager = AppWidgetManager.getInstance(appContext);
        for (Class<?> provider : providerClasses()) {
            int[] ids = manager.getAppWidgetIds(new ComponentName(appContext, provider));
            for (int id : ids) {
                manager.updateAppWidget(id, views(appContext, provider, result, null));
            }
        }
    }

    static void updateAllFromError(Context context, String message) {
        Context appContext = context.getApplicationContext();
        AppWidgetManager manager = AppWidgetManager.getInstance(appContext);
        String compactMessage = compactErrorMessage(message);
        for (Class<?> provider : providerClasses()) {
            int[] ids = manager.getAppWidgetIds(new ComponentName(appContext, provider));
            for (int id : ids) {
                manager.updateAppWidget(id, errorViews(appContext, provider, compactMessage));
            }
        }
    }

    static void updateAllTargetPlaceholder(Context context) {
        Context appContext = context.getApplicationContext();
        AppWidgetManager manager = AppWidgetManager.getInstance(appContext);
        for (Class<?> provider : providerClasses()) {
            int[] ids = manager.getAppWidgetIds(new ComponentName(appContext, provider));
            for (int id : ids) {
                manager.updateAppWidget(id, loadingViews(appContext, provider));
            }
        }
    }

    private static Class<?>[] providerClasses() {
        return new Class<?>[]{FansWidgetProvider.class, FansGlanceCardProvider.class};
    }

    private static void showLoading(Context context, AppWidgetManager manager, int[] ids, Class<?> provider) {
        for (int id : ids) {
            manager.updateAppWidget(id, loadingViews(context, provider));
        }
    }

    private static void showLoadingAll(Context context, AppWidgetManager manager) {
        for (Class<?> provider : providerClasses()) {
            showLoading(context, manager, manager.getAppWidgetIds(new ComponentName(context, provider)), provider);
        }
    }

    private static void showStatusAll(Context context, AppWidgetManager manager, String status) {
        BiliFansClient.FanResult cached = AppState.loadLatest(context);
        for (Class<?> provider : providerClasses()) {
            int[] ids = manager.getAppWidgetIds(new ComponentName(context, provider));
            for (int id : ids) {
                manager.updateAppWidget(id, cached == null
                        ? loadingViews(context, provider)
                        : views(context, provider, cached, status));
            }
        }
    }

    private static RemoteViews loadingViews(Context context, Class<?> provider) {
        BiliFansClient.FanResult cached = AppState.loadLatest(context);
        if (cached != null) {
            RemoteViews views = views(context, provider, cached, "同步中");
            views.setTextViewText(R.id.widget_updated, formatUpdated(cached.updatedAt));
            return views;
        }
        RemoteViews views = baseViews(context, provider);
        views.setTextViewText(R.id.widget_followers, "--");
        views.setTextViewText(R.id.widget_status, "连接中");
        views.setTextViewText(R.id.widget_updated, "bilibili");
        return views;
    }

    private static RemoteViews errorViews(Context context, Class<?> provider, String message) {
        BiliFansClient.FanResult cached = AppState.loadLatest(context);
        if (cached != null) {
            RemoteViews views = views(context, provider, cached, "保留上次");
            views.setTextViewText(R.id.widget_updated, formatUpdated(cached.updatedAt));
            return views;
        }
        RemoteViews views = baseViews(context, provider);
        views.setTextViewText(R.id.widget_followers, "--");
        views.setTextViewText(R.id.widget_status, "暂时无法连接");
        views.setTextViewText(R.id.widget_updated, message);
        return views;
    }

    private static RemoteViews views(Context context, Class<?> provider, BiliFansClient.FanResult result, String status) {
        RemoteViews views = baseViews(context, provider);
        views.setTextViewText(R.id.widget_followers, formatFollowers(result.followers));
        views.setTextViewText(R.id.widget_status, status == null ? "实时统计" : status);
        views.setTextViewText(R.id.widget_updated, formatUpdated(result.updatedAt));
        return views;
    }

    private static String formatFollowers(int followers) {
        return NumberFormat.getIntegerInstance(Locale.CHINA).format(followers);
    }

    private static String formatTime(Date date) {
        return new SimpleDateFormat("HH:mm", Locale.CHINA).format(date);
    }

    private static String formatUpdated(Date date) {
        return formatTime(date) + " 更新";
    }

    private static String friendlyError(Exception error) {
        String message = error.getMessage();
        if (TextUtils.isEmpty(message)) {
            return "网络异常，稍后再试";
        }
        if (message.startsWith("HTTP")) {
            return "网络异常，稍后再试";
        }
        return compactErrorMessage(message);
    }

    private static String compactErrorMessage(String message) {
        if (TextUtils.isEmpty(message)) {
            return "网络异常，稍后再试";
        }
        return message.length() > 18 ? message.substring(0, 18) : message;
    }

    private static RemoteViews baseViews(Context context, Class<?> provider) {
        int layout = provider == FansGlanceCardProvider.class
                ? R.layout.widget_fans_card
                : R.layout.widget_fans;
        RemoteViews views = new RemoteViews(context.getPackageName(), layout);
        BiliFansClient.Target target = AppState.loadTarget(context);
        views.setTextViewText(R.id.widget_name, provider == FansGlanceCardProvider.class ? "" : target.name);

        Intent appIntent = new Intent(context, MainActivity.class);
        appIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent openApp = PendingIntent.getActivity(
                context,
                1,
                appIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
        views.setOnClickPendingIntent(R.id.widget_root, openApp);

        Intent profileIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://space.bilibili.com/" + target.mid));
        PendingIntent openProfile = PendingIntent.getActivity(
                context,
                3,
                profileIntent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
        views.setOnClickPendingIntent(R.id.widget_followers, openProfile);
        views.setOnClickPendingIntent(R.id.widget_name, openProfile);

        PendingIntent refresh = PendingIntent.getBroadcast(
                context,
                2,
                AppState.manualWidgetRefreshIntent(context),
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );
        views.setOnClickPendingIntent(R.id.widget_refresh, refresh);
        return views;
    }
}
