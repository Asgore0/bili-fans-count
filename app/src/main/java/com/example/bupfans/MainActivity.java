package com.example.bupfans;

import android.app.Activity;
import android.appwidget.AppWidgetManager;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Bundle;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.provider.Settings;
import android.text.InputType;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.text.NumberFormat;
import java.text.SimpleDateFormat;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final NumberFormat numberFormat = NumberFormat.getIntegerInstance(Locale.CHINA);
    private final SimpleDateFormat timeFormat = new SimpleDateFormat("HH:mm:ss", Locale.CHINA);

    private TextView followerText;
    private TextView statusText;
    private TextView nameText;
    private TextView midText;
    private TextView updatedText;
    private TextView intervalText;
    private TextView[] intervalOptionViews;
    private EditText uidInput;
    private Button refreshButton;
    private Button applyTargetButton;
    private Button batteryButton;
    private Button addWidgetButton;
    private Button copyButton;
    private Button profileButton;
    private ProgressBar progressBar;
    private long refreshIntervalMs = AppState.DEFAULT_REFRESH_INTERVAL_MS;
    private BiliFansClient.Target currentTarget;
    private BiliFansClient.FanResult latestResult;
    private boolean isVisible;
    private boolean isLoading;
    private boolean isDestroyed;

    private final Runnable refreshLoop = new Runnable() {
        @Override
        public void run() {
            if (isVisible) {
                refreshFans(false);
                scheduleNextRefresh();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        refreshIntervalMs = AppState.loadRefreshInterval(this);
        currentTarget = AppState.loadTarget(this);
        buildUi();
        showCachedResult();
    }

    @Override
    protected void onStart() {
        super.onStart();
        isVisible = true;
        refreshFans(true);
        scheduleNextRefresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (batteryButton != null) {
            batteryButton.setText(batteryButtonText());
        }
    }

    @Override
    protected void onStop() {
        isVisible = false;
        mainHandler.removeCallbacks(refreshLoop);
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        isDestroyed = true;
        executor.shutdownNow();
        super.onDestroy();
    }

    private void buildUi() {
        boolean dark = isDarkMode();
        int primary = dark ? Color.rgb(75, 217, 255) : Color.rgb(0, 161, 214);
        int primaryDark = dark ? Color.rgb(127, 229, 255) : Color.rgb(0, 122, 166);
        int titleColor = dark ? Color.rgb(244, 250, 255) : Color.rgb(16, 24, 40);
        int bodyColor = dark ? Color.rgb(190, 207, 221) : Color.rgb(102, 112, 133);
        int labelColor = dark ? Color.rgb(151, 174, 191) : Color.rgb(71, 84, 103);
        int sourceColor = dark ? Color.rgb(122, 146, 164) : Color.rgb(132, 143, 160);
        int successColor = dark ? Color.rgb(102, 231, 176) : Color.rgb(12, 168, 116);

        ScrollView scrollView = new ScrollView(this);
        scrollView.setFillViewport(true);
        scrollView.setBackground(gradient(
                GradientDrawable.Orientation.TL_BR,
                dark
                        ? new int[]{
                                Color.rgb(13, 19, 28),
                                Color.rgb(16, 25, 35),
                                Color.rgb(17, 22, 31)
                        }
                        : new int[]{
                                Color.rgb(245, 247, 250),
                                Color.rgb(238, 246, 250),
                                Color.rgb(247, 250, 252)
                        }
        ));

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(dp(20), dp(24), dp(20), dp(24));
        scrollView.addView(root, new ScrollView.LayoutParams(
                ScrollView.LayoutParams.MATCH_PARENT,
                ScrollView.LayoutParams.WRAP_CONTENT
        ));

        LinearLayout hero = new LinearLayout(this);
        hero.setOrientation(LinearLayout.VERTICAL);
        hero.setGravity(Gravity.CENTER_HORIZONTAL);
        hero.setPadding(dp(18), dp(18), dp(18), dp(22));
        hero.setBackground(glassCard(dp(28), dark));
        hero.setElevation(dp(7));
        LinearLayout.LayoutParams heroParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        heroParams.topMargin = dp(14);
        root.addView(hero, heroParams);

        LinearLayout identityRow = new LinearLayout(this);
        identityRow.setOrientation(LinearLayout.HORIZONTAL);
        identityRow.setGravity(Gravity.CENTER_VERTICAL);
        hero.addView(identityRow, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));

        ImageView avatar = new ImageView(this);
        avatar.setImageResource(R.drawable.avatar_xiemen);
        avatar.setScaleType(ImageView.ScaleType.CENTER_CROP);
        avatar.setElevation(dp(3));
        identityRow.addView(avatar, new LinearLayout.LayoutParams(dp(48), dp(48)));

        LinearLayout identityText = new LinearLayout(this);
        identityText.setOrientation(LinearLayout.VERTICAL);
        LinearLayout.LayoutParams identityTextParams = new LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
        );
        identityTextParams.setMarginStart(dp(12));
        identityRow.addView(identityText, identityTextParams);

        nameText = text(currentTarget.name, 18, titleColor, Typeface.BOLD);
        identityText.addView(nameText);

        TextView subtitle = text("Bilibili · 实时关注者", 12, bodyColor, Typeface.NORMAL);
        LinearLayout.LayoutParams subtitleParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        subtitleParams.topMargin = dp(3);
        identityText.addView(subtitle, subtitleParams);

        View statusDot = new View(this);
        statusDot.setBackground(roundRect(successColor, dp(999), Color.TRANSPARENT));
        identityRow.addView(statusDot, new LinearLayout.LayoutParams(dp(7), dp(7)));

        followerText = text("--", 66, primary, Typeface.BOLD);
        followerText.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams followerParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        followerParams.topMargin = dp(34);
        hero.addView(followerText, followerParams);

        TextView label = text("关注者", 13, labelColor, Typeface.BOLD);
        label.setGravity(Gravity.CENTER);
        hero.addView(label);

        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        panel.setPadding(dp(18), dp(16), dp(18), dp(16));
        panel.setBackground(glassCard(dp(20), dark));
        panel.setElevation(dp(5));
        LinearLayout.LayoutParams panelParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        panelParams.topMargin = dp(18);
        root.addView(panel, panelParams);

        LinearLayout statusRow = new LinearLayout(this);
        statusRow.setOrientation(LinearLayout.HORIZONTAL);
        statusRow.setGravity(Gravity.CENTER_VERTICAL);
        panel.addView(statusRow);

        statusText = text("准备刷新", 14, primaryDark, Typeface.BOLD);
        statusText.setPadding(dp(12), dp(7), dp(12), dp(7));
        statusText.setBackground(roundRect(
                dark ? Color.argb(42, 75, 217, 255) : Color.argb(34, 0, 161, 214),
                dp(999),
                Color.TRANSPARENT
        ));
        statusRow.addView(statusText);

        intervalText = text(intervalSummary(), 14, bodyColor, Typeface.NORMAL);
        intervalText.setGravity(Gravity.END);
        LinearLayout.LayoutParams intervalParams = new LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
        );
        statusRow.addView(intervalText, intervalParams);

        midText = text(getString(R.string.main_uid_format, currentTarget.mid), 14, bodyColor, Typeface.NORMAL);
        LinearLayout.LayoutParams detailParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        detailParams.topMargin = dp(18);
        panel.addView(midText, detailParams);

        updatedText = text(getString(R.string.main_updated_format, "--"), 14, bodyColor, Typeface.NORMAL);
        LinearLayout.LayoutParams updateParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        updateParams.topMargin = dp(6);
        panel.addView(updatedText, updateParams);

        LinearLayout targetRow = new LinearLayout(this);
        targetRow.setOrientation(LinearLayout.HORIZONTAL);
        targetRow.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams targetRowParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        targetRowParams.topMargin = dp(14);
        panel.addView(targetRow, targetRowParams);

        uidInput = new EditText(this);
        uidInput.setText(String.valueOf(currentTarget.mid));
        uidInput.setSingleLine(true);
        uidInput.setSelectAllOnFocus(true);
        uidInput.setInputType(InputType.TYPE_CLASS_NUMBER);
        uidInput.setTextSize(14);
        uidInput.setTextColor(titleColor);
        uidInput.setHintTextColor(bodyColor);
        uidInput.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        uidInput.setPadding(dp(12), 0, dp(12), 0);
        uidInput.setBackground(roundRect(
                dark ? Color.argb(34, 255, 255, 255) : Color.argb(116, 255, 255, 255),
                dp(16),
                dark ? Color.argb(42, 186, 238, 255) : Color.argb(70, 255, 255, 255)
        ));
        targetRow.addView(uidInput, new LinearLayout.LayoutParams(
                0,
                dp(44),
                1f
        ));

        applyTargetButton = new Button(this);
        applyTargetButton.setText(R.string.main_apply_uid);
        applyTargetButton.setTextSize(13);
        applyTargetButton.setTextColor(dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166));
        applyTargetButton.setAllCaps(false);
        applyTargetButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        applyTargetButton.setBackground(roundRect(
                dark ? Color.argb(38, 255, 255, 255) : Color.argb(120, 255, 255, 255),
                dp(16),
                dark ? Color.argb(50, 186, 238, 255) : Color.argb(52, 0, 122, 166)
        ));
        applyTargetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                applyTargetUid();
            }
        });
        LinearLayout.LayoutParams applyParams = new LinearLayout.LayoutParams(
                dp(92),
                dp(44)
        );
        applyParams.setMarginStart(dp(10));
        targetRow.addView(applyTargetButton, applyParams);

        LinearLayout intervalPicker = new LinearLayout(this);
        intervalPicker.setOrientation(LinearLayout.HORIZONTAL);
        intervalPicker.setGravity(Gravity.CENTER);
        intervalPicker.setPadding(dp(4), dp(4), dp(4), dp(4));
        intervalPicker.setBackground(roundRect(
                dark ? Color.argb(34, 255, 255, 255) : Color.argb(60, 0, 161, 214),
                dp(999),
                dark ? Color.argb(44, 186, 238, 255) : Color.argb(46, 0, 161, 214)
        ));
        LinearLayout.LayoutParams pickerParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        pickerParams.topMargin = dp(16);
        panel.addView(intervalPicker, pickerParams);

        intervalOptionViews = new TextView[AppState.REFRESH_INTERVAL_LABELS.length];
        for (int i = 0; i < AppState.REFRESH_INTERVAL_LABELS.length; i++) {
            final int index = i;
            TextView option = text(AppState.REFRESH_INTERVAL_LABELS[i], 13, bodyColor, Typeface.BOLD);
            option.setGravity(Gravity.CENTER);
            option.setPadding(dp(10), dp(8), dp(10), dp(8));
            option.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    selectRefreshInterval(index);
                }
            });
            intervalOptionViews[i] = option;
            intervalPicker.addView(option, new LinearLayout.LayoutParams(
                    0,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    1f
            ));
        }
        updateIntervalOptions();

        LinearLayout toolPanel = new LinearLayout(this);
        toolPanel.setOrientation(LinearLayout.VERTICAL);
        toolPanel.setPadding(dp(14), dp(14), dp(14), dp(14));
        toolPanel.setBackground(glassCard(dp(22), dark));
        toolPanel.setElevation(dp(5));
        LinearLayout.LayoutParams toolParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        toolParams.topMargin = dp(18);
        root.addView(toolPanel, toolParams);

        LinearLayout primaryActions = new LinearLayout(this);
        primaryActions.setGravity(Gravity.CENTER_VERTICAL);
        primaryActions.setOrientation(LinearLayout.HORIZONTAL);
        toolPanel.addView(primaryActions, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        ));

        refreshButton = new Button(this);
        refreshButton.setText("刷新");
        refreshButton.setTextSize(15);
        refreshButton.setTextColor(Color.WHITE);
        refreshButton.setAllCaps(false);
        refreshButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        refreshButton.setBackground(gradientRound(
                GradientDrawable.Orientation.LEFT_RIGHT,
                dark
                        ? new int[]{Color.rgb(0, 151, 204), Color.rgb(89, 221, 255)}
                        : new int[]{Color.rgb(0, 161, 214), Color.rgb(31, 184, 219)},
                dp(18)
        ));
        refreshButton.setElevation(dp(3));
        refreshButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                refreshFans(true);
            }
        });
        primaryActions.addView(refreshButton, new LinearLayout.LayoutParams(
                0,
                dp(50),
                1f
        ));

        progressBar = new ProgressBar(this);
        progressBar.setVisibility(View.GONE);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(dp(40), dp(40));
        progressParams.setMarginStart(dp(12));
        primaryActions.addView(progressBar, progressParams);

        LinearLayout secondaryActions = new LinearLayout(this);
        secondaryActions.setGravity(Gravity.CENTER_VERTICAL);
        secondaryActions.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams secondaryParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        secondaryParams.topMargin = dp(10);
        toolPanel.addView(secondaryActions, secondaryParams);

        addWidgetButton = new Button(this);
        addWidgetButton.setText("添加桌面卡片");
        addWidgetButton.setTextSize(13);
        addWidgetButton.setTextColor(dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166));
        addWidgetButton.setAllCaps(false);
        addWidgetButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        addWidgetButton.setBackground(roundRect(
                dark ? Color.argb(34, 255, 255, 255) : Color.argb(72, 255, 255, 255),
                dp(18),
                dark ? Color.argb(50, 186, 238, 255) : Color.argb(42, 0, 122, 166)
        ));
        addWidgetButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                requestPinWidget();
            }
        });
        secondaryActions.addView(addWidgetButton, new LinearLayout.LayoutParams(
                0,
                dp(44),
                1f
        ));

        batteryButton = new Button(this);
        batteryButton.setText(batteryButtonText());
        batteryButton.setTextSize(13);
        batteryButton.setTextColor(dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166));
        batteryButton.setAllCaps(false);
        batteryButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        batteryButton.setBackground(roundRect(
                dark ? Color.argb(34, 255, 255, 255) : Color.argb(72, 255, 255, 255),
                dp(18),
                dark ? Color.argb(50, 186, 238, 255) : Color.argb(42, 0, 122, 166)
        ));
        batteryButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                requestBatteryOptimizationWhitelist();
            }
        });
        LinearLayout.LayoutParams batteryParams = new LinearLayout.LayoutParams(
                0,
                dp(44),
                1f
        );
        batteryParams.setMarginStart(dp(10));
        secondaryActions.addView(batteryButton, batteryParams);

        LinearLayout quickActions = new LinearLayout(this);
        quickActions.setGravity(Gravity.CENTER_VERTICAL);
        quickActions.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams quickParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        quickParams.topMargin = dp(10);
        toolPanel.addView(quickActions, quickParams);

        copyButton = new Button(this);
        copyButton.setText("复制数量");
        copyButton.setTextSize(13);
        copyButton.setTextColor(dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166));
        copyButton.setAllCaps(false);
        copyButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        copyButton.setBackground(roundRect(
                dark ? Color.argb(26, 255, 255, 255) : Color.argb(56, 255, 255, 255),
                dp(18),
                dark ? Color.argb(38, 186, 238, 255) : Color.argb(34, 0, 122, 166)
        ));
        copyButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                copyFollowers();
            }
        });
        quickActions.addView(copyButton, new LinearLayout.LayoutParams(
                0,
                dp(44),
                1f
        ));

        profileButton = new Button(this);
        profileButton.setText("打开主页");
        profileButton.setTextSize(13);
        profileButton.setTextColor(dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166));
        profileButton.setAllCaps(false);
        profileButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        profileButton.setBackground(roundRect(
                dark ? Color.argb(26, 255, 255, 255) : Color.argb(56, 255, 255, 255),
                dp(18),
                dark ? Color.argb(38, 186, 238, 255) : Color.argb(34, 0, 122, 166)
        ));
        profileButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                openBilibiliProfile();
            }
        });
        LinearLayout.LayoutParams profileParams = new LinearLayout.LayoutParams(
                0,
                dp(44),
                1f
        );
        profileParams.setMarginStart(dp(10));
        quickActions.addView(profileButton, profileParams);

        TextView source = text("bilibili public data", 12, sourceColor, Typeface.NORMAL);
        source.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams sourceParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
        );
        sourceParams.topMargin = dp(22);
        root.addView(source, sourceParams);

        setContentView(scrollView);
    }

    private void refreshFans(final boolean manual) {
        if (isLoading) {
            return;
        }
        final BiliFansClient.Target target = AppState.loadTarget(this);
        currentTarget = target;
        isLoading = true;
        setLoading(true, manual ? "正在刷新..." : "自动刷新中...");

        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    BiliFansClient.FanResult result = BiliFansClient.fetchFanResult(target);
                    showResult(result);
                } catch (Exception error) {
                    showError(error);
                }
            }
        });
    }

    private void applyTargetUid() {
        String raw = uidInput == null ? "" : uidInput.getText().toString().trim();
        if (raw.isEmpty()) {
            setLoading(false, "请输入 B站 UID");
            return;
        }
        long mid;
        try {
            mid = Long.parseLong(raw);
        } catch (NumberFormatException error) {
            setLoading(false, "UID 格式不正确");
            return;
        }
        if (mid <= 0L) {
            setLoading(false, "UID 必须大于 0");
            return;
        }

        String name = currentTarget != null && currentTarget.mid == mid
                ? currentTarget.name
                : "UID " + mid;
        currentTarget = new BiliFansClient.Target(name, mid);
        latestResult = null;
        AppState.saveTarget(this, currentTarget);
        updateTargetViews(currentTarget);
        followerText.setText("--");
        updatedText.setText(getString(R.string.main_updated_format, "--"));
        FansWidgetProvider.updateAllTargetPlaceholder(this);
        refreshFans(true);
    }

    private void copyFollowers() {
        if (latestResult == null) {
            setLoading(false, "暂无数字可复制");
            return;
        }
        String value = numberFormat.format(latestResult.followers);
        ClipboardManager clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("BILI粉丝数", value));
            setLoading(false, "数字已复制");
        }
    }

    private void openBilibiliProfile() {
        long mid = currentTarget == null ? 0L : currentTarget.mid;
        if (mid <= 0L) {
            setLoading(false, "UID 无效");
            return;
        }
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("https://space.bilibili.com/" + mid));
        try {
            startActivity(intent);
            setLoading(false, "已打开 B站主页");
        } catch (Exception ignored) {
            setLoading(false, "无法打开浏览器");
        }
    }

    private void selectRefreshInterval(int index) {
        refreshIntervalMs = AppState.REFRESH_INTERVAL_OPTIONS_MS[index];
        AppState.saveRefreshInterval(this, refreshIntervalMs);
        intervalText.setText(intervalSummary());
        updateIntervalOptions();
        if (isVisible) {
            mainHandler.removeCallbacks(refreshLoop);
            scheduleNextRefresh();
        }
    }

    private void scheduleNextRefresh() {
        mainHandler.removeCallbacks(refreshLoop);
        if (isVisible) {
            mainHandler.postDelayed(refreshLoop, refreshIntervalMs);
        }
    }

    private String intervalSummary() {
        return refreshIntervalLabel(refreshIntervalMs) + " 自动刷新";
    }

    private String refreshIntervalLabel(long intervalMs) {
        return AppState.intervalLabel(intervalMs);
    }

    private void updateIntervalOptions() {
        if (intervalOptionViews == null) {
            return;
        }
        boolean dark = isDarkMode();
        for (int i = 0; i < intervalOptionViews.length; i++) {
            boolean selected = AppState.REFRESH_INTERVAL_OPTIONS_MS[i] == refreshIntervalMs;
            intervalOptionViews[i].setTextColor(selected
                    ? (dark ? Color.rgb(244, 250, 255) : Color.rgb(0, 122, 166))
                    : (dark ? Color.rgb(151, 174, 191) : Color.rgb(102, 112, 133)));
            intervalOptionViews[i].setBackground(selected
                    ? roundRect(dark ? Color.argb(46, 75, 217, 255) : Color.argb(120, 255, 255, 255), dp(999), Color.TRANSPARENT)
                    : null);
        }
    }

    private void showResult(final BiliFansClient.FanResult result) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isDestroyed) {
                    return;
                }
                isLoading = false;
                AppState.saveLatest(MainActivity.this, result);
                updateResultViews(result);
                FansWidgetProvider.updateAllFromResult(MainActivity.this, result);
                AppState.scheduleNextWidgetRefresh(MainActivity.this);
                setLoading(false, "刷新成功");
            }
        });
    }

    private void showCachedResult() {
        BiliFansClient.FanResult cached = AppState.loadLatest(this);
        if (cached != null) {
            updateResultViews(cached);
            statusText.setText("已同步缓存");
        }
    }

    private String batteryButtonText() {
        return isIgnoringBatteryOptimizations() ? "后台已允许" : "后台刷新";
    }

    private boolean isIgnoringBatteryOptimizations() {
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        return powerManager != null && powerManager.isIgnoringBatteryOptimizations(getPackageName());
    }

    private void requestBatteryOptimizationWhitelist() {
        if (isIgnoringBatteryOptimizations()) {
            setLoading(false, "后台刷新已允许");
            return;
        }
        if (isXiaomiDevice()) {
            if (openXiaomiBackgroundSettings()) {
                setLoading(false, "请在 HyperOS 中允许自启动/后台活动");
                return;
            }
        }
        Intent intent = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
        try {
            startActivity(intent);
            setLoading(false, "请在系统设置中允许后台活动");
        } catch (Exception ignored) {
            startActivity(new Intent(Settings.ACTION_SETTINGS));
            setLoading(false, "请在电池设置中允许后台活动");
        }
    }

    private void requestPinWidget() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            AppWidgetManager manager = getSystemService(AppWidgetManager.class);
            if (manager != null && manager.isRequestPinAppWidgetSupported()) {
                ComponentName provider = new ComponentName(this, FansWidgetProvider.class);
                manager.requestPinAppWidget(provider, null, null);
                setLoading(false, "已请求添加桌面小组件");
                return;
            }
        }
        setLoading(false, "请长按桌面，在小组件中选择谐门东西");
    }

    private boolean isXiaomiDevice() {
        String manufacturer = Build.MANUFACTURER == null ? "" : Build.MANUFACTURER.toLowerCase(Locale.US);
        String brand = Build.BRAND == null ? "" : Build.BRAND.toLowerCase(Locale.US);
        return manufacturer.contains("xiaomi")
                || brand.contains("xiaomi")
                || brand.contains("redmi")
                || brand.contains("poco");
    }

    private boolean openXiaomiBackgroundSettings() {
        Intent[] intents = new Intent[]{
                componentIntent("com.miui.securitycenter", "com.miui.permcenter.autostart.AutoStartManagementActivity"),
                componentIntent("com.miui.securitycenter", "com.miui.powerkeeper.ui.HiddenAppsContainerManagementActivity"),
                componentIntent("com.miui.securitycenter", "com.miui.powercenter.PowerSettings")
        };
        for (Intent intent : intents) {
            if (tryStart(intent)) {
                return true;
            }
        }
        return false;
    }

    private Intent componentIntent(String packageName, String className) {
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(packageName, className));
        intent.putExtra("package_name", getPackageName());
        intent.putExtra("pkg", getPackageName());
        return intent;
    }

    private boolean tryStart(Intent intent) {
        try {
            startActivity(intent);
            return true;
        } catch (Exception ignored) {
            return false;
        }
    }

    private void updateResultViews(BiliFansClient.FanResult result) {
        latestResult = result;
        currentTarget = new BiliFansClient.Target(result.name, result.mid);
        updateTargetViews(currentTarget);
        followerText.setText(numberFormat.format(result.followers));
        updatedText.setText(getString(R.string.main_updated_format, timeFormat.format(result.updatedAt)));
    }

    private void updateTargetViews(BiliFansClient.Target target) {
        nameText.setText(target.name);
        midText.setText(getString(R.string.main_uid_format, target.mid));
        if (uidInput != null) {
            uidInput.setText(String.valueOf(target.mid));
        }
    }

    private void showError(final Exception error) {
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                if (isDestroyed) {
                    return;
                }
                isLoading = false;
                String message = error.getMessage();
                if (TextUtils.isEmpty(message)) {
                    message = "网络请求失败";
                } else if (message.length() > 60) {
                    message = message.substring(0, 60);
                }
                FansWidgetProvider.updateAllFromError(MainActivity.this, message);
                AppState.scheduleNextWidgetRefresh(MainActivity.this);
                setLoading(false, getString(R.string.main_refresh_error_format, message));
            }
        });
    }

    private void setLoading(boolean loading, String status) {
        progressBar.setVisibility(loading ? View.VISIBLE : View.GONE);
        refreshButton.setEnabled(!loading);
        if (applyTargetButton != null) {
            applyTargetButton.setEnabled(!loading);
            applyTargetButton.setAlpha(loading ? 0.58f : 1f);
        }
        refreshButton.setAlpha(loading ? 0.58f : 1f);
        statusText.setText(status);
    }

    private TextView text(String value, int sp, int color, int typeface) {
        TextView view = new TextView(this);
        view.setText(value);
        view.setTextSize(sp);
        view.setTextColor(color);
        view.setTypeface(Typeface.DEFAULT, typeface);
        return view;
    }

    private GradientDrawable roundRect(int color, int radius, int strokeColor) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.RECTANGLE);
        drawable.setColor(color);
        drawable.setCornerRadius(radius);
        if (strokeColor != Color.TRANSPARENT) {
            drawable.setStroke(dp(1), strokeColor);
        }
        return drawable;
    }

    private Drawable glassCard(int radius, boolean dark) {
        return new LiquidGlassDrawable(radius, dark, getResources().getDisplayMetrics().density);
    }

    private boolean isDarkMode() {
        int mode = getResources().getConfiguration().uiMode & Configuration.UI_MODE_NIGHT_MASK;
        return mode == Configuration.UI_MODE_NIGHT_YES;
    }

    private GradientDrawable gradient(GradientDrawable.Orientation orientation, int[] colors) {
        GradientDrawable drawable = new GradientDrawable(orientation, colors);
        drawable.setShape(GradientDrawable.RECTANGLE);
        return drawable;
    }

    private GradientDrawable gradientRound(GradientDrawable.Orientation orientation, int[] colors, int radius) {
        GradientDrawable drawable = gradient(orientation, colors);
        drawable.setCornerRadius(radius);
        return drawable;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

}
