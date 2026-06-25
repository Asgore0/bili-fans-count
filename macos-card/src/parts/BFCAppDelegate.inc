// Application delegate, menus, window wiring, refresh lifecycle, and commands.
@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSMenuItemValidation, NSPopoverDelegate, NSMenuDelegate>
- (BOOL)prepareForSingleInstanceLaunch;
- (void)handleShowMainWindowNotification:(NSNotification *)notification;
- (BOOL)bundlePathIsMountedDiskImage:(NSString *)path;
- (BOOL)bundlePathIsInstalledApplication:(NSString *)path;
- (void)showCardWindowTemporarily;
- (void)saveWindowFrameIfAvailable;
- (void)applyRuntimeApplicationIcon;
- (void)copyHistoryCSV;
- (void)copyTrendCSV;
- (void)copyHistoryPoints:(NSArray<NSDictionary *> *)points status:(NSString *)status;
- (void)confirmAndClearCurrentHistory;
- (void)populateRecentTargetsMenu:(NSMenu *)menu;
- (void)refreshCachedFollowerTextsIfNeeded;
@end

@implementation AppDelegate {
    NSWindow *_window;
    NSView *_rootView;
    AppDashboardChromeView *_chromeView;
    LiquidCardView *_card;
    NSStatusItem *_statusItem;
    NSPopover *_statusPopover;
    NSPanel *_statusPanel;
    StatusPopoverView *_popoverView;
    NSView *_mainGlassView;
    NSView *_statusGlassView;
    NSDateFormatter *_formatter;
    NSTimer *_timer;
    NSTimer *_displayTimer;
    NSTimer *_windowFrameSaveTimer;
    NSTimer *_transientStatusTimer;
    NSDate *_nextAutoRefreshAt;
    NSImage *_statusIconActive;
    NSImage *_statusIconInactive;
    NSString *_cachedExactFollowersText;
    NSString *_cachedCompactFollowersText;
    NSInteger _cachedFollowersTextValue;
    BOOL _hasCachedFollowersTextValue;
    BOOL _isOpeningStatusSurface;
    NSTextField *_mainUIDField;
    NSTextField *_mainTargetLabel;
    NSTextField *_mainStatusLabel;
    NSTextField *_mainScheduleLabel;
    NSTextField *_mainHistoryLabel;
    NSButton *_mainRefreshButton;
    NSButton *_mainCopyButton;
    NSButton *_mainProfileButton;
    NSButton *_mainCopyHistoryButton;
    NSButton *_mainCopyTrendButton;
    NSButton *_mainClearHistoryButton;
    NSSegmentedControl *_mainAppearanceControl;
    NSSegmentedControl *_mainTrendControl;
    NSSegmentedControl *_mainDisplayControl;
    NSSegmentedControl *_mainIntervalControl;
    NSButton *_mainAutoRefreshButton;
    NSButton *_mainNotificationsButton;
    NSButton *_mainLaunchAtLoginButton;
    NSButton *_mainMenuBarOnlyButton;
    NSButton *_mainStatusWidgetButton;
    NSButton *_mainAlwaysOnTopButton;
    NSButton *_mainPositionLockedButton;
    NSWindow *_preferencesWindow;
    NSVisualEffectView *_preferencesEffectView;
    PreferencesGlassChromeView *_preferencesChromeView;
    NSTextField *_preferencesUIDField;
    NSTextField *_preferencesStatusLabel;
    NSSegmentedControl *_preferencesAppearanceControl;
    NSSegmentedControl *_preferencesDisplayControl;
    NSSegmentedControl *_preferencesIntervalControl;
    NSButton *_preferencesAutoRefreshButton;
    NSButton *_preferencesNotificationsButton;
    NSButton *_preferencesLaunchAtLoginButton;
    NSButton *_preferencesMenuBarOnlyButton;
    NSButton *_preferencesAlwaysOnTopButton;
    NSButton *_preferencesPositionLockedButton;
    NSButton *_preferencesRefreshButton;
    NSString *_lastStatusTitle;
    NSString *_lastStatusTooltip;
    CGFloat _lastStatusLength;
    BOOL _hasLastStatusLength;
    BOOL _lastStatusIconActive;
    BOOL _hasLastStatusIconState;
    BOOL _isRefreshing;
    long long _activeRefreshMID;
    BOOL _pendingRefreshAfterCurrent;
    BOOL _pendingRefreshManual;
    NSUInteger _consecutiveRefreshFailures;
    NSString *_stableCardStatus;
    NSString *_launchAtLoginUnavailableReason;
    BOOL _launchAtLoginAvailabilityResolved;
    NSUInteger _historyPointCount;
    NSUInteger _weekPointCount;
    NSUInteger _trendPointCount;
    NSInteger _historyDelta;
    BOOL _historyHasDelta;
    NSString *_currentTrendSummaryText;
    NSString *_mainTrendStatusText;
    NSString *_trendCSVTooltipText;
}

- (void)applyRuntimeApplicationIcon {
    NSImage *icon = [NSImage imageNamed:@"AppIcon"];
    if (!icon) {
        NSString *iconPath = [NSBundle.mainBundle pathForResource:@"AppIcon" ofType:@"icns"];
        if (iconPath.length > 0) {
            icon = [[NSImage alloc] initWithContentsOfFile:iconPath];
        }
    }
    if (icon) {
        NSApp.applicationIconImage = icon;
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    if (![self prepareForSingleInstanceLaunch]) {
        return;
    }
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                        selector:@selector(handleShowMainWindowNotification:)
                                                            name:ShowMainWindowNotificationName
                                                          object:nil
                                              suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self
                                                        selector:@selector(systemAppearanceDidChange:)
                                                            name:@"AppleInterfaceThemeChangedNotification"
                                                          object:nil
                                              suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];
    [NSWorkspace.sharedWorkspace.notificationCenter addObserver:self
                                                       selector:@selector(accessibilityDisplayOptionsDidChange:)
                                                           name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                                         object:nil];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [self applyRuntimeApplicationIcon];
    [self setupMainMenu];
    NSRect visible = NSScreen.mainScreen.visibleFrame;
    NSRect frame = NSMakeRect(NSMidX(visible) - MainWindowWidth / 2.0, NSMidY(visible) - MainWindowHeight / 2.0, MainWindowWidth, MainWindowHeight);
    frame.origin = [Settings windowOriginWithDefaultFrame:frame];

    _rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, MainWindowWidth, MainWindowHeight)];
    _rootView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _rootView.wantsLayer = YES;
    _rootView.layer.backgroundColor = NSColor.clearColor.CGColor;

    NSView *windowContentView = _rootView;
    if (@available(macOS 26.0, *)) {
        NSGlassEffectView *glassView = [[NSGlassEffectView alloc] initWithFrame:_rootView.bounds];
        glassView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        glassView.wantsLayer = YES;
        glassView.layer.backgroundColor = NSColor.clearColor.CGColor;
        glassView.contentView = _rootView;
        _mainGlassView = glassView;
        windowContentView = glassView;
    } else {
        NSVisualEffectView *effectView = [[NSVisualEffectView alloc] initWithFrame:_rootView.bounds];
        effectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        effectView.material = NSVisualEffectMaterialWindowBackground;
        effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
        effectView.state = NSVisualEffectStateActive;
        effectView.wantsLayer = YES;
        effectView.layer.backgroundColor = NSColor.clearColor.CGColor;
        [_rootView removeFromSuperview];
        [effectView addSubview:_rootView];
        _mainGlassView = effectView;
        windowContentView = effectView;
    }

    _chromeView = [[AppDashboardChromeView alloc] initWithFrame:_rootView.bounds];
    _chromeView.appearanceMode = [Settings appearanceMode];
    _chromeView.usesSystemGlass = _mainGlassView != nil && [_mainGlassView isKindOfClass:NSClassFromString(@"NSGlassEffectView")];
    _chromeView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [_rootView addSubview:_chromeView];

    NSRect cardFrame = NSMakeRect(40, 334, 424, 180);
    _card = [[LiquidCardView alloc] initWithFrame:cardFrame];
    _card.showsInlineControls = NO;
    _card.mid = [Settings mid];
    _card.name = [Settings name];
    _card.appearanceMode = [Settings appearanceMode];
    _card.refreshInterval = [Settings refreshInterval];
    _card.alwaysOnTop = [Settings alwaysOnTop];
    _card.positionLocked = [Settings positionLocked];
    [self updateHistoryValues];
    __weak typeof(self) weakSelf = self;
    _card.refreshHandler = ^{ [weakSelf refresh:YES]; };
    _card.settingsHandler = ^{ [weakSelf showPreferences:nil]; };
    _card.appearanceHandler = ^(NSInteger mode) { [weakSelf setAppearanceMode:mode]; };
    _card.refreshIntervalHandler = ^(NSTimeInterval interval) { [weakSelf setRefreshInterval:interval]; };
    _card.alwaysOnTopHandler = ^(BOOL enabled) { [weakSelf setAlwaysOnTop:enabled]; };
    _card.positionLockedHandler = ^(BOOL locked) { [weakSelf setPositionLocked:locked]; };
    _card.resetPositionHandler = ^{ [weakSelf resetWindowPosition]; };
    _card.copyHistoryHandler = ^{ [weakSelf copyHistoryCSV]; };
    _card.clearHistoryHandler = ^{ [weakSelf confirmAndClearCurrentHistory]; };
    _card.hideHandler = ^{ [weakSelf hideCardWindow]; };
    [_rootView addSubview:_card];
    [self buildMainAppPageControls];
    [self applyVisualEffectMaterials];

    _window = [[NSWindow alloc] initWithContentRect:frame
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"BILI粉丝数";
    _window.identifier = @"BILIFansCardMainWindow";
    _window.hasShadow = YES;
    _window.releasedWhenClosed = NO;
    _window.restorable = NO;
    _window.delegate = self;
    _window.contentView = windowContentView;
    ApplyWindowChrome(_window);
    ApplyWindowChromeAfterSystemTitlebarUpdate(_window);
    [self applyWindowBehavior];
    [self applyVisualEffectMaterials];
    if ([Settings launchHidden]) {
        [_window orderOut:nil];
    } else {
        [_window makeKeyAndOrderFront:nil];
        [self clearMainWindowTransientTextFocus];
        [self startDisplayRefreshTicker];
    }
    [self setupStatusItem];
    [self closeStatusPopover];
    [self syncMainStatusWidgetButton];

    _formatter = [NSDateFormatter new];
    _formatter.dateFormat = @"HH:mm";
    [self showCachedResultIfAvailable];
    [self startAutoRefresh];
    if ([Settings autoRefreshEnabled]) {
        [self refresh:NO];
    } else if (!_card.hasFollowers) {
        [self setStableCardStatus:@"自动刷新已暂停"];
        [self syncStatusPopoverIfVisible];
    }
}

- (BOOL)applicationShouldSaveApplicationState:(NSApplication *)sender {
    (void)sender;
    return NO;
}

- (BOOL)applicationShouldRestoreApplicationState:(NSApplication *)sender {
    (void)sender;
    return NO;
}

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    (void)app;
    return NO;
}

- (BOOL)prepareForSingleInstanceLaunch {
    NSString *bundleIdentifier = NSBundle.mainBundle.bundleIdentifier;
    if (bundleIdentifier.length == 0) {
        return YES;
    }
    pid_t currentPID = NSProcessInfo.processInfo.processIdentifier;
    NSArray<NSRunningApplication *> *runningApps = [NSRunningApplication runningApplicationsWithBundleIdentifier:bundleIdentifier];
    NSMutableArray<NSRunningApplication *> *otherApps = [NSMutableArray new];
    for (NSRunningApplication *app in runningApps) {
        if (app.processIdentifier != currentPID) {
            [otherApps addObject:app];
        }
    }
    if (otherApps.count == 0) {
        return YES;
    }

    NSString *currentPath = NSBundle.mainBundle.bundlePath.stringByStandardizingPath ?: @"";
    BOOL currentFromDMG = [self bundlePathIsMountedDiskImage:currentPath];
    BOOL currentInstalled = [self bundlePathIsInstalledApplication:currentPath];
    NSRunningApplication *installedExistingApp = nil;
    BOOL hasNonInstalledExistingApp = NO;
    for (NSRunningApplication *app in otherApps) {
        NSString *path = app.bundleURL.path.stringByStandardizingPath ?: @"";
        if ([self bundlePathIsInstalledApplication:path]) {
            if (!installedExistingApp) {
                installedExistingApp = app;
            }
        } else {
            hasNonInstalledExistingApp = YES;
        }
    }

    NSDictionary *userInfo = @{@"senderPID": @(currentPID)};

    if (currentInstalled && installedExistingApp) {
        for (NSRunningApplication *app in otherApps) {
            NSString *path = app.bundleURL.path.stringByStandardizingPath ?: @"";
            if (![self bundlePathIsInstalledApplication:path]) {
                [app terminate];
            }
        }
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName:ShowMainWindowNotificationName
                                                                       object:nil
                                                                     userInfo:userInfo
                                                           deliverImmediately:YES];
        [installedExistingApp activateWithOptions:NSApplicationActivateIgnoringOtherApps];
        [NSApp terminate:nil];
        return NO;
    }

    if (currentInstalled && hasNonInstalledExistingApp) {
        for (NSRunningApplication *app in otherApps) {
            [app terminate];
        }
        return YES;
    }

    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:ShowMainWindowNotificationName
                                                                   object:nil
                                                                 userInfo:userInfo
                                                       deliverImmediately:YES];
    NSRunningApplication *existing = otherApps.firstObject;
    [existing activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    if (currentFromDMG || !currentInstalled) {
        [NSApp terminate:nil];
        return NO;
    }
    for (NSRunningApplication *app in otherApps) {
        [app terminate];
    }
    return YES;
}

- (void)handleShowMainWindowNotification:(NSNotification *)notification {
    NSNumber *senderPID = notification.userInfo[@"senderPID"];
    if (senderPID && senderPID.intValue == NSProcessInfo.processInfo.processIdentifier) {
        return;
    }
    [self showCardWindowTemporarily];
}

- (BOOL)bundlePathIsMountedDiskImage:(NSString *)path {
    NSString *standardized = path.stringByStandardizingPath ?: @"";
    return [standardized hasPrefix:@"/Volumes/"];
}

- (BOOL)bundlePathIsInstalledApplication:(NSString *)path {
    NSString *standardized = path.stringByStandardizingPath ?: @"";
    NSArray<NSString *> *applicationDirs = NSSearchPathForDirectoriesInDomains(NSApplicationDirectory, NSAllDomainsMask, YES);
    for (NSString *dir in applicationDirs) {
        NSString *standardDir = dir.stringByStandardizingPath;
        if ([standardized isEqualToString:standardDir] || [standardized hasPrefix:[standardDir stringByAppendingString:@"/"]]) {
            return YES;
        }
    }
    return NO;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    [self showCardWindow];
    return YES;
}

- (void)styleGlassButton:(NSButton *)button {
    if (!button) return;
    button.bezelStyle = NSBezelStyleInline;
    button.bordered = NO;
    button.wantsLayer = YES;
    button.contentTintColor = NSColor.labelColor;
    if (@available(macOS 10.15, *)) {
        button.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

- (void)styleGlassTextField:(NSTextField *)field {
    if (!field) return;
    NSString *stringValue = field.stringValue ?: @"";
    NSString *placeholder = field.placeholderString;
    NSFont *font = field.font ?: [NSFont systemFontOfSize:13];
    NSTextAlignment alignment = field.alignment;
    NSLineBreakMode lineBreakMode = field.cell.lineBreakMode;
    BOOL usesSingleLine = field.cell.usesSingleLineMode;

    VerticallyCenteredTextFieldCell *cell = [[VerticallyCenteredTextFieldCell alloc] initTextCell:stringValue];
    cell.placeholderString = placeholder;
    cell.font = font;
    cell.alignment = alignment;
    cell.usesSingleLineMode = usesSingleLine;
    cell.lineBreakMode = lineBreakMode;
    cell.bezeled = NO;
    cell.bordered = NO;
    cell.drawsBackground = NO;
    cell.editable = YES;
    cell.selectable = YES;
    cell.enabled = YES;
    field.cell = cell;
    field.enabled = YES;
    field.editable = YES;
    field.selectable = YES;
    field.refusesFirstResponder = NO;
    field.bezeled = NO;
    field.drawsBackground = NO;
    field.focusRingType = NSFocusRingTypeNone;
    field.textColor = NSColor.labelColor;
    field.font = font;
    field.alignment = alignment;
}

- (void)styleGlassCheckbox:(NSButton *)button {
    if (!button) return;
    button.wantsLayer = YES;
    button.contentTintColor = NSColor.secondaryLabelColor;
    if (@available(macOS 10.15, *)) {
        button.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

- (void)styleGlassSegmentedControl:(NSSegmentedControl *)control {
    if (!control) return;
    control.segmentStyle = NSSegmentStyleCapsule;
    control.wantsLayer = YES;
    control.font = [NSFont systemFontOfSize:12 weight:NSFontWeightMedium];
    if (@available(macOS 10.15, *)) {
        control.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

- (NSButton *)pageButtonWithTitle:(NSString *)title symbolName:(NSString *)symbolName frame:(NSRect)frame action:(SEL)action {
    NSButton *button = [NSButton buttonWithTitle:title target:self action:action];
    button.frame = frame;
    button.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    button.toolTip = title;
    [self styleGlassButton:button];
    if (symbolName.length > 0) {
        NSImage *image = TemplateSystemSymbolImage(symbolName, title);
        if (image) {
            button.image = image;
            button.imagePosition = NSImageLeft;
        }
    }
    return button;
}

- (void)buildMainAppPageControls {
    [_rootView addSubview:[self preferencesLabelWithString:@"BILI粉丝数"
                                                     frame:NSMakeRect(40, 574, 260, 28)
                                                      font:[NSFont systemFontOfSize:23 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    [_rootView addSubview:[self preferencesLabelWithString:@"完整程序页面、菜单栏小组件和刷新缓存共用同一份实时数据。"
                                                     frame:NSMakeRect(40, 552, 440, 20)
                                                      font:[NSFont systemFontOfSize:12 weight:NSFontWeightRegular]
                                                     color:NSColor.secondaryLabelColor]];
    [_rootView addSubview:[self preferencesLabelWithString:[NSString stringWithFormat:@"macOS · v%@", AppVersionString()]
                                                     frame:NSMakeRect(750, 580, 126, 22)
                                                      font:[NSFont systemFontOfSize:12 weight:NSFontWeightMedium]
                                                     color:NSColor.tertiaryLabelColor]];

    [_rootView addSubview:[self preferencesLabelWithString:@"实时数据"
                                                     frame:NSMakeRect(40, 518, 160, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    [_rootView addSubview:[self preferencesLabelWithString:@"桌面卡片预览"
                                                     frame:NSMakeRect(346, 518, 118, 20)
                                                      font:[NSFont systemFontOfSize:12 weight:NSFontWeightMedium]
                                                     color:NSColor.tertiaryLabelColor]];

    _mainTargetLabel = [self preferencesLabelWithString:@""
                                                  frame:NSMakeRect(40, 264, 424, 22)
                                                   font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                  color:NSColor.secondaryLabelColor];
    [_rootView addSubview:_mainTargetLabel];
    _mainStatusLabel = [self preferencesLabelWithString:@""
                                                  frame:NSMakeRect(40, 242, 424, 22)
                                                   font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                  color:NSColor.secondaryLabelColor];
    [_rootView addSubview:_mainStatusLabel];

    NSArray<NSString *> *actionTitles = @[@"刷新", @"复制粉丝数", @"打开主页"];
    NSArray<NSString *> *actionSymbols = @[@"arrow.clockwise", @"doc.on.doc", @"safari"];
    NSArray<NSString *> *actionSelectors = @[@"mainRefreshNow:", @"mainCopyFollowers:", @"mainOpenProfile:"];
    CGFloat buttonX = 40;
    for (NSUInteger i = 0; i < actionTitles.count; i++) {
        CGFloat width = i == 1 ? 108 : 86;
        NSButton *button = [self pageButtonWithTitle:actionTitles[i]
                                          symbolName:actionSymbols[i]
                                               frame:NSMakeRect(buttonX, 202, width, 31)
                                              action:NSSelectorFromString(actionSelectors[i])];
        [_rootView addSubview:button];
        if (i == 0) {
            _mainRefreshButton = button;
        } else if (i == 1) {
            _mainCopyButton = button;
        } else if (i == 2) {
            _mainProfileButton = button;
        }
        buttonX += button.frame.size.width + 14;
    }

    [_rootView addSubview:[self preferencesLabelWithString:@"趋势与导出"
                                                     frame:NSMakeRect(40, 124, 160, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    _mainHistoryLabel = [self preferencesLabelWithString:@""
                                                   frame:NSMakeRect(40, 98, 236, 22)
                                                    font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                   color:NSColor.secondaryLabelColor];
    [_rootView addSubview:_mainHistoryLabel];
    _mainCopyHistoryButton = [self pageButtonWithTitle:@"复制一周 CSV"
                                            symbolName:@"tablecells"
                                                 frame:NSMakeRect(40, 54, 128, 31)
                                                action:@selector(mainCopyHistory:)];
    _mainCopyTrendButton = [self pageButtonWithTitle:@"复制趋势 CSV"
                                          symbolName:@"chart.xyaxis.line"
                                               frame:NSMakeRect(182, 54, 128, 31)
                                              action:@selector(mainCopyTrend:)];
    _mainClearHistoryButton = [self pageButtonWithTitle:@"清空历史"
                                             symbolName:@"trash"
                                                  frame:NSMakeRect(324, 54, 98, 31)
                                                 action:@selector(mainClearHistory:)];
    [_rootView addSubview:_mainCopyHistoryButton];
    [_rootView addSubview:_mainCopyTrendButton];
    [_rootView addSubview:_mainClearHistoryButton];
    [_rootView addSubview:[self preferencesLabelWithString:@"完整历史按月保存；可复制近 7 天 CSV"
                                                     frame:NSMakeRect(40, 30, 390, 18)
                                                      font:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]
                                                     color:NSColor.tertiaryLabelColor]];

    [_rootView addSubview:[self preferencesLabelWithString:@"监控账号"
                                                     frame:NSMakeRect(520, 508, 140, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    [_rootView addSubview:[self preferencesLabelWithString:@"输入 UID 或 B站链接后立即切换监控对象。"
                                                     frame:NSMakeRect(520, 484, 332, 20)
                                                      font:[NSFont systemFontOfSize:12 weight:NSFontWeightRegular]
                                                     color:NSColor.secondaryLabelColor]];
    [_rootView addSubview:[self preferencesLabelWithString:@"UID/链接"
                                                     frame:NSMakeRect(548, 454, 72, 18)
                                                      font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                     color:NSColor.secondaryLabelColor]];
    _mainUIDField = [[NSTextField alloc] initWithFrame:NSMakeRect(548, 428, 232, 28)];
    _mainUIDField.placeholderString = @"UID 或空间链接";
    _mainUIDField.toolTip = @"可输入 UID，或粘贴 B站空间、移动端、App 内打开等链接";
    _mainUIDField.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    _mainUIDField.alignment = NSTextAlignmentLeft;
    _mainUIDField.cell.usesSingleLineMode = YES;
    _mainUIDField.cell.lineBreakMode = NSLineBreakByClipping;
    _mainUIDField.target = self;
    _mainUIDField.action = @selector(mainApplyUID:);
    [self styleGlassTextField:_mainUIDField];
    [_rootView addSubview:_mainUIDField];
    [_rootView addSubview:[self pageButtonWithTitle:@"应用" symbolName:@"checkmark.circle" frame:NSMakeRect(792, 427, 66, 30) action:@selector(mainApplyUID:)]];

    _mainScheduleLabel = [self preferencesLabelWithString:@""
                                                    frame:NSMakeRect(520, 358, 340, 20)
                                                     font:[NSFont systemFontOfSize:12 weight:NSFontWeightMedium]
                                                    color:NSColor.secondaryLabelColor];
    [_rootView addSubview:[self preferencesLabelWithString:@"刷新与提醒"
                                                     frame:NSMakeRect(520, 382, 140, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    [_rootView addSubview:_mainScheduleLabel];
    _mainIntervalControl = [self preferencesSegmentedControlWithLabels:@[@"1 分钟", @"5 分钟", @"15 分钟"]
                                                                 frame:NSMakeRect(520, 320, 340, 28)
                                                                action:@selector(mainIntervalChanged:)];
    [_rootView addSubview:_mainIntervalControl];

    _mainAutoRefreshButton = [self preferencesCheckboxWithTitle:@"自动刷新" frame:NSMakeRect(520, 284, 160, 24) action:@selector(mainAutoRefreshChanged:)];
    _mainNotificationsButton = [self preferencesCheckboxWithTitle:@"粉丝变化提醒" frame:NSMakeRect(700, 284, 160, 24) action:@selector(mainNotificationsChanged:)];
    [_rootView addSubview:_mainAutoRefreshButton];
    [_rootView addSubview:_mainNotificationsButton];

    [_rootView addSubview:[self preferencesLabelWithString:@"顶栏组件"
                                                     frame:NSMakeRect(520, 248, 140, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    _mainDisplayControl = [self preferencesSegmentedControlWithLabels:@[@"精确数字", @"紧凑数字", @"仅图标"]
                                                               frame:NSMakeRect(520, 210, 340, 28)
                                                              action:@selector(mainDisplayChanged:)];
    [_rootView addSubview:_mainDisplayControl];
    _mainMenuBarOnlyButton = [self preferencesCheckboxWithTitle:@"仅顶栏模式" frame:NSMakeRect(520, 174, 160, 24) action:@selector(mainMenuBarOnlyChanged:)];
    [_rootView addSubview:_mainMenuBarOnlyButton];
    _mainStatusWidgetButton = [self pageButtonWithTitle:@"打开顶栏卡片" symbolName:@"menubar.rectangle" frame:NSMakeRect(700, 173, 160, 30) action:@selector(mainOpenStatusWidget:)];
    [_rootView addSubview:_mainStatusWidgetButton];

    [_rootView addSubview:[self preferencesLabelWithString:@"应用行为"
                                                     frame:NSMakeRect(520, 128, 140, 22)
                                                      font:[NSFont systemFontOfSize:14 weight:NSFontWeightSemibold]
                                                     color:NSColor.labelColor]];
    _mainLaunchAtLoginButton = [self preferencesCheckboxWithTitle:@"登录时启动" frame:NSMakeRect(520, 104, 104, 24) action:@selector(mainLaunchAtLoginChanged:)];
    _mainAlwaysOnTopButton = [self preferencesCheckboxWithTitle:@"窗口置顶" frame:NSMakeRect(638, 104, 104, 24) action:@selector(mainAlwaysOnTopChanged:)];
    _mainPositionLockedButton = [self preferencesCheckboxWithTitle:@"锁定位置" frame:NSMakeRect(756, 104, 104, 24) action:@selector(mainPositionLockedChanged:)];
    [_rootView addSubview:_mainLaunchAtLoginButton];
    [_rootView addSubview:_mainAlwaysOnTopButton];
    [_rootView addSubview:_mainPositionLockedButton];

    [_rootView addSubview:[self pageButtonWithTitle:@"居中" symbolName:@"scope" frame:NSMakeRect(520, 66, 96, 30) action:@selector(mainResetPosition:)]];
    [_rootView addSubview:[self pageButtonWithTitle:@"隐藏窗口" symbolName:@"rectangle.dashed" frame:NSMakeRect(638, 66, 110, 30) action:@selector(mainHideWindow:)]];
    [_rootView addSubview:[self pageButtonWithTitle:@"退出" symbolName:@"power" frame:NSMakeRect(762, 66, 98, 30) action:@selector(mainQuit:)]];

    [_rootView addSubview:[self preferencesLabelWithString:@"涨粉趋势"
                                                     frame:NSMakeRect(308, 124, 108, 22)
                                                      font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                     color:NSColor.secondaryLabelColor]];
    _mainTrendControl = [self preferencesSegmentedControlWithLabels:@[@"7天", @"30天", @"90天"]
                                                              frame:NSMakeRect(308, 90, 156, 28)
                                                             action:@selector(mainTrendChanged:)];
    [_rootView addSubview:_mainTrendControl];
    _mainAppearanceControl = [self preferencesSegmentedControlWithLabels:@[@"极透", @"清透", @"可读"]
                                                                   frame:NSMakeRect(308, 30, 156, 28)
                                                                  action:@selector(mainAppearanceChanged:)];
    [_rootView addSubview:_mainAppearanceControl];

    [self syncMainPageControls];
}

- (NSMenuItem *)mainMenuItemWithTitle:(NSString *)title action:(SEL)action keyEquivalent:(NSString *)key {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key ?: @""];
    item.target = self;
    return item;
}

- (NSMenuItem *)submenuItemWithTitle:(NSString *)title submenu:(NSMenu *)submenu {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    item.submenu = submenu;
    return item;
}

- (NSMenu *)trendRangeMenu {
    NSMenu *menu = [NSMenu new];
    [menu addItem:[self mainMenuItemWithTitle:@"7 天" action:@selector(statusUseSevenDayTrend:) keyEquivalent:@""]];
    [menu addItem:[self mainMenuItemWithTitle:@"30 天" action:@selector(statusUseThirtyDayTrend:) keyEquivalent:@""]];
    [menu addItem:[self mainMenuItemWithTitle:@"90 天" action:@selector(statusUseNinetyDayTrend:) keyEquivalent:@""]];
    return menu;
}

- (NSMenu *)recentTargetsMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"最近监控"];
    menu.delegate = self;
    [self populateRecentTargetsMenu:menu];
    return menu;
}

- (void)populateRecentTargetsMenu:(NSMenu *)menu {
    [menu removeAllItems];
    NSArray<FanResult *> *results = [Settings recentResults];
    if (results.count == 0) {
        NSMenuItem *empty = [[NSMenuItem alloc] initWithTitle:@"暂无缓存账号" action:nil keyEquivalent:@""];
        empty.enabled = NO;
        [menu addItem:empty];
        return;
    }

    long long currentMID = [Settings mid];
    for (FanResult *result in results) {
        NSString *name = result.name.length > 0 ? result.name : UnknownTargetName;
        NSString *followers = result.followers > 0 ? DecimalStringForInteger(result.followers) : @"--";
        NSString *title = [NSString stringWithFormat:@"%@ · %@", name, followers];
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(statusUseRecentTarget:) keyEquivalent:@""];
        item.target = self;
        item.representedObject = @(result.mid);
        item.toolTip = [NSString stringWithFormat:@"UID %lld", result.mid];
        SetMenuItemStateIfChanged(item, result.mid == currentMID ? NSControlStateValueOn : NSControlStateValueOff);
        [menu addItem:item];
    }
}

- (void)menuNeedsUpdate:(NSMenu *)menu {
    if ([menu.title isEqualToString:@"最近监控"]) {
        [self populateRecentTargetsMenu:menu];
    }
}

- (void)setupMainMenu {
    NSMenu *mainMenu = [NSMenu new];

    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"BILI粉丝数"];
    [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"关于 BILI粉丝数" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""]];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:[self mainMenuItemWithTitle:@"设置..." action:@selector(showPreferences:) keyEquivalent:@","]];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"隐藏 BILI粉丝数" action:@selector(hide:) keyEquivalent:@"h"]];
    NSMenuItem *hideOthers = [[NSMenuItem alloc] initWithTitle:@"隐藏其他" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [appMenu addItem:hideOthers];
    [appMenu addItem:[[NSMenuItem alloc] initWithTitle:@"全部显示" action:@selector(unhideAllApplications:) keyEquivalent:@""]];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:[self mainMenuItemWithTitle:@"退出 BILI粉丝数" action:@selector(statusQuit:) keyEquivalent:@"q"]];
    [mainMenu addItem:[self submenuItemWithTitle:@"BILI粉丝数" submenu:appMenu]];

    NSMenu *cardMenu = [[NSMenu alloc] initWithTitle:@"窗口"];
    [cardMenu addItem:[self mainMenuItemWithTitle:@"显示/隐藏主窗口" action:@selector(toggleCardWindow:) keyEquivalent:@"1"]];
    [cardMenu addItem:[self mainMenuItemWithTitle:@"仅顶栏模式" action:@selector(toggleMenuBarOnly:) keyEquivalent:@""]];
    [cardMenu addItem:[self mainMenuItemWithTitle:@"回到屏幕中央" action:@selector(resetWindowPosition) keyEquivalent:@""]];
    [cardMenu addItem:[NSMenuItem separatorItem]];
    [cardMenu addItem:[self mainMenuItemWithTitle:@"窗口置顶" action:@selector(statusToggleAlwaysOnTop:) keyEquivalent:@""]];
    [cardMenu addItem:[self mainMenuItemWithTitle:@"锁定位置" action:@selector(statusTogglePositionLocked:) keyEquivalent:@""]];
    [mainMenu addItem:[self submenuItemWithTitle:@"窗口" submenu:cardMenu]];

    NSMenu *monitorMenu = [[NSMenu alloc] initWithTitle:@"监控"];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"刷新" action:@selector(statusRefresh:) keyEquivalent:@"r"]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"自动刷新" action:@selector(statusToggleAutoRefresh:) keyEquivalent:@""]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"粉丝变化提醒" action:@selector(statusToggleChangeNotifications:) keyEquivalent:@""]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"登录时启动" action:@selector(statusToggleLaunchAtLogin:) keyEquivalent:@""]];
    [monitorMenu addItem:[NSMenuItem separatorItem]];
    [monitorMenu addItem:[self submenuItemWithTitle:@"最近监控" submenu:[self recentTargetsMenu]]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"复制粉丝数" action:@selector(statusCopyFollowers:) keyEquivalent:@"c"]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"复制一周 CSV" action:@selector(statusCopyHistory:) keyEquivalent:@""]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"复制当前趋势 CSV" action:@selector(statusCopyTrend:) keyEquivalent:@""]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"清空当前历史" action:@selector(confirmAndClearCurrentHistory) keyEquivalent:@""]];
    [monitorMenu addItem:[self submenuItemWithTitle:@"涨粉趋势" submenu:[self trendRangeMenu]]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"打开 B站主页" action:@selector(statusOpenProfile:) keyEquivalent:@""]];
    [monitorMenu addItem:[self mainMenuItemWithTitle:@"设置监控 UID..." action:@selector(showPreferences:) keyEquivalent:@""]];
    [mainMenu addItem:[self submenuItemWithTitle:@"监控" submenu:monitorMenu]];

    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"显示"];
    NSMenu *appearanceMenu = [NSMenu new];
    [appearanceMenu addItem:[self mainMenuItemWithTitle:@"极清透" action:@selector(statusUseUltraClear:) keyEquivalent:@""]];
    [appearanceMenu addItem:[self mainMenuItemWithTitle:@"清透" action:@selector(statusUseClear:) keyEquivalent:@""]];
    [appearanceMenu addItem:[self mainMenuItemWithTitle:@"增强可读" action:@selector(statusUseReadable:) keyEquivalent:@""]];
    [viewMenu addItem:[self submenuItemWithTitle:@"透明度" submenu:appearanceMenu]];

    NSMenu *displayMenu = [NSMenu new];
    [displayMenu addItem:[self mainMenuItemWithTitle:@"精确数字" action:@selector(statusUseExactDisplay:) keyEquivalent:@""]];
    [displayMenu addItem:[self mainMenuItemWithTitle:@"紧凑数字" action:@selector(statusUseCompactDisplay:) keyEquivalent:@""]];
    [displayMenu addItem:[self mainMenuItemWithTitle:@"仅图标" action:@selector(statusUseIconOnlyDisplay:) keyEquivalent:@""]];
    [viewMenu addItem:[self submenuItemWithTitle:@"顶栏显示" submenu:displayMenu]];

    NSMenu *intervalMenu = [NSMenu new];
    [intervalMenu addItem:[self mainMenuItemWithTitle:@"1 分钟" action:@selector(statusUseOneMinute:) keyEquivalent:@""]];
    [intervalMenu addItem:[self mainMenuItemWithTitle:@"5 分钟" action:@selector(statusUseFiveMinutes:) keyEquivalent:@""]];
    [intervalMenu addItem:[self mainMenuItemWithTitle:@"15 分钟" action:@selector(statusUseFifteenMinutes:) keyEquivalent:@""]];
    [viewMenu addItem:[self submenuItemWithTitle:@"刷新间隔" submenu:intervalMenu]];
    [mainMenu addItem:[self submenuItemWithTitle:@"显示" submenu:viewMenu]];

    [NSApp setMainMenu:mainMenu];
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    (void)notification;
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
                                                               name:ShowMainWindowNotificationName
                                                             object:nil];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self
                                                               name:@"AppleInterfaceThemeChangedNotification"
                                                             object:nil];
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self
                                                              name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                                            object:nil];
    [_timer invalidate];
    _timer = nil;
    [_displayTimer invalidate];
    _displayTimer = nil;
    [_windowFrameSaveTimer invalidate];
    _windowFrameSaveTimer = nil;
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    [self saveWindowFrameIfAvailable];
}

- (void)accessibilityDisplayOptionsDidChange:(NSNotification *)notification {
    (void)notification;
    [self refreshAppearanceDrivenSurfaces];
}

- (void)systemAppearanceDidChange:(NSNotification *)notification {
    (void)notification;
    [self refreshAppearanceDrivenSurfaces];
}

- (void)applyChromeForWindowFromNotification:(NSNotification *)notification {
    NSWindow *window = [notification.object isKindOfClass:NSWindow.class] ? (NSWindow *)notification.object : nil;
    if (window == _window || window == _preferencesWindow) {
        ApplyWindowChrome(window);
        ApplyWindowChromeAfterSystemTitlebarUpdate(window);
    }
}

- (void)refreshAppearanceDrivenSurfaces {
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self refreshAppearanceDrivenSurfaces];
        });
        return;
    }
    InvalidateAppAppearanceCache();
    [self applyVisualEffectMaterials];
    _chromeView.needsDisplay = YES;
    _preferencesChromeView.needsDisplay = YES;
    _card.needsDisplay = YES;
    _popoverView.needsDisplay = YES;
    [self invalidateAppearanceDrivenControls];
    [self syncVisibleControlSurfaces];
}

- (void)invalidateAppearanceDrivenControls {
    NSView *views[] = {
        _mainAppearanceControl,
        _mainTrendControl,
        _mainDisplayControl,
        _mainIntervalControl,
        _mainAutoRefreshButton,
        _mainNotificationsButton,
        _mainLaunchAtLoginButton,
        _mainMenuBarOnlyButton,
        _mainStatusWidgetButton,
        _mainAlwaysOnTopButton,
        _mainPositionLockedButton,
        _preferencesAppearanceControl,
        _preferencesDisplayControl,
        _preferencesIntervalControl,
        _preferencesAutoRefreshButton,
        _preferencesNotificationsButton,
        _preferencesLaunchAtLoginButton,
        _preferencesMenuBarOnlyButton,
        _preferencesAlwaysOnTopButton,
        _preferencesPositionLockedButton,
        _preferencesRefreshButton
    };
    for (NSUInteger i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
        MarkViewNeedsDisplay(views[i]);
    }
    [_popoverView invalidateAppearanceDrivenControls];
}

- (void)windowDidMove:(NSNotification *)notification {
    [self scheduleWindowFrameSave];
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    if (notification.object == _statusPanel) {
        [self applyStatusPopoverGlassStyle];
        [_popoverView invalidateAppearanceDrivenControls];
        _popoverView.needsDisplay = YES;
        return;
    }
    [self applyChromeForWindowFromNotification:notification];
}

- (void)windowDidResignKey:(NSNotification *)notification {
    if (notification.object == _statusPanel) {
        [self applyStatusPopoverGlassStyle];
        [_popoverView invalidateAppearanceDrivenControls];
        _popoverView.needsDisplay = YES;
        return;
    }
    [self applyChromeForWindowFromNotification:notification];
}

- (void)windowDidChangeScreen:(NSNotification *)notification {
    if (notification.object == _statusPanel) {
        [self applyStatusPopoverGlassStyle];
        _popoverView.needsDisplay = YES;
        return;
    }
    [self applyChromeForWindowFromNotification:notification];
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification {
    if (notification.object == _statusPanel) {
        [self applyStatusPopoverGlassStyle];
        _popoverView.needsDisplay = YES;
        return;
    }
    [self applyChromeForWindowFromNotification:notification];
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
    if (sender == _window) {
        [self hideCardWindow];
        return NO;
    }
    return YES;
}

- (void)scheduleWindowFrameSave {
    [_windowFrameSaveTimer invalidate];
    __weak typeof(self) weakSelf = self;
    _windowFrameSaveTimer = [NSTimer timerWithTimeInterval:0.8 repeats:NO block:^(__unused NSTimer *timer) {
        AppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        [strongSelf saveWindowFrameIfAvailable];
        strongSelf->_windowFrameSaveTimer = nil;
    }];
    _windowFrameSaveTimer.tolerance = 0.4;
    [[NSRunLoop mainRunLoop] addTimer:_windowFrameSaveTimer forMode:NSRunLoopCommonModes];
}

- (void)saveWindowFrameNow {
    [_windowFrameSaveTimer invalidate];
    _windowFrameSaveTimer = nil;
    [self saveWindowFrameIfAvailable];
}

- (void)saveWindowFrameIfAvailable {
    if (!_window) {
        return;
    }
    [Settings saveWindowFrame:_window.frame];
}

- (void)applyWindowBehavior {
    _window.level = [Settings alwaysOnTop] ? NSFloatingWindowLevel : NSNormalWindowLevel;
    _window.movableByWindowBackground = ![Settings positionLocked];
}

- (NSVisualEffectMaterial)cardMaterialForAppearance:(NSInteger)mode {
    mode = EffectiveAppearanceMode(mode);
    if (mode == AppearanceModeReadable) {
        return NSVisualEffectMaterialWindowBackground;
    }
    if (mode == AppearanceModeClear) {
        return NSVisualEffectMaterialPopover;
    }
    return NSVisualEffectMaterialUnderWindowBackground;
}

- (BOOL)isStatusSurfaceShown {
    return _statusPanel.isVisible || _statusPopover.shown;
}

- (void)clearStatusButtonHighlight {
    NSButton *button = _statusItem.button;
    if (!button) return;
    if (button.isHighlighted) {
        button.highlighted = NO;
    }
    [button highlight:NO];
}

- (void)applyGlassEffectStyleToView:(NSView *)view cornerRadius:(CGFloat)cornerRadius {
    if (!view) return;
    if (@available(macOS 26.0, *)) {
        NSGlassEffectView *glassView = (NSGlassEffectView *)view;
        NSInteger mode = EffectiveAppearanceMode([Settings appearanceMode]);
        BOOL dark = AppAppearanceIsDark();
        glassView.appearance = [NSAppearance appearanceNamed:(dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
        glassView.cornerRadius = cornerRadius;
        glassView.style = mode == AppearanceModeReadable ? NSGlassEffectViewStyleRegular : NSGlassEffectViewStyleClear;
        glassView.tintColor = mode == AppearanceModeReadable
            ? [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.42 : 0.16)]
            : (mode == AppearanceModeClear
                ? [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.34 : 0.09)]
                : [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.26 : 0.05)]);
        if (@available(macOS 27.0, *)) {
            glassView.effectIsInteractive = NO;
        }
    }
}

- (void)applyStatusPopoverGlassStyle {
    if (@available(macOS 26.0, *)) {
        if ([_statusGlassView isKindOfClass:NSClassFromString(@"NSGlassEffectView")]) {
            NSGlassEffectView *glassView = (NSGlassEffectView *)_statusGlassView;
            NSInteger mode = EffectiveAppearanceMode([Settings appearanceMode]);
            BOOL dark = AppAppearanceIsDark();
            glassView.appearance = [NSAppearance appearanceNamed:(dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
            glassView.cornerRadius = 20.0;
            glassView.style = NSGlassEffectViewStyleClear;
            CGFloat tintAlpha = dark
                ? LiquidGlassAlphaForMode(mode, 0.38, 0.30, 0.22)
                : LiquidGlassAlphaForMode(mode, 0.13, 0.075, 0.044);
            NSColor *glassTintColor = dark ? NSColor.blackColor : NSColor.whiteColor;
            glassView.tintColor = [glassTintColor colorWithAlphaComponent:tintAlpha];
            if (@available(macOS 27.0, *)) {
                glassView.effectIsInteractive = NO;
            }
        }
    } else {
        [self applyGlassEffectStyleToView:_statusGlassView cornerRadius:20.0];
    }
    _popoverView.appearance = [NSAppearance appearanceNamed:(AppAppearanceIsDark() ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
}

- (NSRect)statusButtonScreenRect {
    NSButton *button = _statusItem.button;
    if (!button || !button.window) return NSZeroRect;
    NSRect rectInWindow = [button convertRect:button.bounds toView:nil];
    return [button.window convertRectToScreen:rectInWindow];
}

- (NSRect)statusPanelFrameForButtonRect:(NSRect)buttonRect {
    NSSize size = _statusPanel.frame.size;
    if (size.width < 1 || size.height < 1) {
        size = NSMakeSize(318, 320);
    }
    NSScreen *screen = _statusItem.button.window.screen ?: NSScreen.mainScreen;
    NSRect visible = screen.visibleFrame;
    CGFloat x = NSMidX(buttonRect) - size.width / 2.0;
    CGFloat y = NSMinY(buttonRect) - size.height - 6.0;
    x = MAX(NSMinX(visible) + 8.0, MIN(x, NSMaxX(visible) - size.width - 8.0));
    y = MAX(NSMinY(visible) + 8.0, y);
    return NSMakeRect(round(x), round(y), size.width, size.height);
}

- (void)positionStatusPanel {
    if (!_statusPanel) return;
    NSRect buttonRect = [self statusButtonScreenRect];
    NSRect frame = [self statusPanelFrameForButtonRect:buttonRect];
    [_statusPanel setFrame:frame display:NO];
}

- (void)ensureStatusPanel {
    if (_statusPanel) return;
    if (@available(macOS 26.0, *)) {
        NSView *panelContentView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 318, 320)];
        panelContentView.wantsLayer = YES;
        panelContentView.layer.backgroundColor = NSColor.clearColor.CGColor;

        NSGlassEffectView *glassView = [[NSGlassEffectView alloc] initWithFrame:_popoverView.bounds];
        glassView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        glassView.wantsLayer = YES;
        glassView.layer.backgroundColor = NSColor.clearColor.CGColor;
        glassView.contentView = _popoverView;
        _statusGlassView = glassView;
        _popoverView.usesSystemGlass = YES;
        [panelContentView addSubview:glassView];

        _statusPanel = [[StatusGlassPanel alloc] initWithContentRect:panelContentView.bounds
                                                           styleMask:NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
        _statusPanel.opaque = NO;
        _statusPanel.backgroundColor = NSColor.clearColor;
        _statusPanel.hasShadow = NO;
        _statusPanel.releasedWhenClosed = NO;
        _statusPanel.hidesOnDeactivate = NO;
        _statusPanel.level = NSStatusWindowLevel;
        _statusPanel.restorable = NO;
        _statusPanel.delegate = self;
        _statusPanel.excludedFromWindowsMenu = YES;
        _statusPanel.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces
            | NSWindowCollectionBehaviorTransient
            | NSWindowCollectionBehaviorFullScreenAuxiliary;
        _statusPanel.contentView = panelContentView;
        [_statusPanel orderOut:nil];
        [self applyStatusPopoverGlassStyle];
    }
}

- (void)applyVisualEffectMaterials {
    NSInteger mode = [Settings appearanceMode];
    if (_window) {
        ApplyWindowChrome(_window);
    }
    if (_mainGlassView) {
        if (@available(macOS 26.0, *)) {
            if ([_mainGlassView isKindOfClass:NSClassFromString(@"NSGlassEffectView")]) {
                [self applyGlassEffectStyleToView:_mainGlassView cornerRadius:0.0];
            }
        }
        if ([_mainGlassView isKindOfClass:[NSVisualEffectView class]]) {
            NSVisualEffectView *effectView = (NSVisualEffectView *)_mainGlassView;
            effectView.material = NSVisualEffectMaterialWindowBackground;
            effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
            effectView.state = NSVisualEffectStateActive;
            effectView.alphaValue = 1.0;
        }
    }
    _chromeView.usesSystemGlass = _mainGlassView != nil && [_mainGlassView isKindOfClass:NSClassFromString(@"NSGlassEffectView")];
    _chromeView.appearanceMode = mode;
    if (_preferencesWindow) {
        ApplyWindowChrome(_preferencesWindow);
    }
    if (_preferencesEffectView) {
        _preferencesEffectView.material = [self cardMaterialForAppearance:mode];
        _preferencesEffectView.state = NSVisualEffectStateActive;
        _preferencesEffectView.alphaValue = 1.0;
    }
    [self applyStatusPopoverGlassStyle];
    [self updateStatusItemTitleSyncVisibleSurfaces:NO];
}

- (void)setupStatusItem {
    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    _statusItem.button.imagePosition = NSImageLeft;
    _statusItem.button.imageScaling = NSImageScaleProportionallyDown;
    _statusItem.button.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightSemibold];
    _statusItem.button.bordered = NO;
    _statusItem.button.wantsLayer = YES;
    _statusItem.button.target = self;
    _statusItem.button.action = @selector(statusItemClicked:);
    [_statusItem.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
    [self applyStatusItemLength:NSVariableStatusItemLength imageText:@"" active:NO tooltip:@"BILI粉丝数"];
    [self setupStatusPopover];
    [self updateStatusItemTitle];
}

- (void)setupStatusPopover {
    _popoverView = [[StatusPopoverView alloc] initWithFrame:NSMakeRect(0, 0, 318, 320)];
    __weak typeof(self) weakSelf = self;
    _popoverView.refreshHandler = ^{ [weakSelf refresh:YES]; };
    _popoverView.copyHandler = ^{ [weakSelf statusCopyFollowers:nil]; };
    _popoverView.profileHandler = ^{ [weakSelf statusOpenProfile:nil]; };
    _popoverView.historyHandler = ^{ [weakSelf statusCopyHistory:nil]; };
    _popoverView.notificationHandler = ^(BOOL enabled) { [weakSelf setChangeNotificationsEnabled:enabled]; };
    _popoverView.launchAtLoginHandler = ^(BOOL enabled) { [weakSelf setLaunchAtLoginEnabled:enabled]; };
    _popoverView.settingsHandler = ^{ [weakSelf closeStatusPopover]; [weakSelf statusSettings:nil]; };
    _popoverView.showCardHandler = ^{ [weakSelf closeStatusPopover]; [weakSelf showCardWindow]; };
    _popoverView.refreshIntervalHandler = ^(NSTimeInterval interval) { [weakSelf setRefreshInterval:interval]; };
    _popoverView.autoRefreshHandler = ^(BOOL enabled) { [weakSelf setAutoRefreshEnabled:enabled]; };
    _popoverView.menuBarOnlyHandler = ^(BOOL enabled) { [weakSelf setMenuBarOnlyEnabled:enabled]; };
    _popoverView.statusDisplayHandler = ^(NSInteger mode) { [weakSelf setStatusDisplayMode:mode]; };
    _popoverView.appearanceHandler = ^(NSInteger mode) {
        AppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        if (strongSelf->_isOpeningStatusSurface) {
            strongSelf->_popoverView.appearanceMode = [Settings appearanceMode];
            return;
        }
        [strongSelf setAppearanceMode:mode];
    };
    _popoverView.quitHandler = ^{ [weakSelf statusQuit:nil]; };

    _popoverView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _popoverView.wantsLayer = YES;
    _popoverView.layer.backgroundColor = NSColor.clearColor.CGColor;
    if (@available(macOS 26.0, *)) {
        _popoverView.usesSystemGlass = YES;
    } else {
        _statusGlassView = nil;
        _popoverView.usesSystemGlass = NO;
        NSViewController *controller = [NSViewController new];
        controller.view = _popoverView;

        _statusPopover = [NSPopover new];
        _statusPopover.contentSize = _popoverView.bounds.size;
        _statusPopover.behavior = NSPopoverBehaviorSemitransient;
        _statusPopover.animates = YES;
        _statusPopover.delegate = self;
        _statusPopover.contentViewController = controller;
    }
    [self applyVisualEffectMaterials];
    [self syncVisibleControlSurfaces];
}

- (void)syncStatusPopoverWithScheduleText:(NSString *)providedScheduleText {
    if (!_popoverView) return;
    BOOL autoRefreshEnabled = [Settings autoRefreshEnabled];
    BOOL launchHidden = [Settings launchHidden];
    BOOL notificationsEnabled = [Settings changeNotificationsEnabled];
    NSTimeInterval refreshInterval = [Settings refreshInterval];
    NSInteger statusDisplayMode = [Settings statusDisplayMode];
    NSInteger appearanceMode = [Settings appearanceMode];
    _popoverView.name = _card.name;
    _popoverView.followers = _card.followers;
    _popoverView.hasFollowers = _card.hasFollowers;
    _popoverView.followerDelta = _card.followerDelta;
    _popoverView.hasFollowerDelta = _card.hasFollowerDelta;
    _popoverView.historyValues = _card.historyValues;
    _popoverView.status = _card.status;
    _popoverView.updatedText = _card.updatedText;
    _popoverView.refreshScheduleText = providedScheduleText ?: [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled];
    _popoverView.menuBarOnly = launchHidden;
    _popoverView.copyEnabled = _card.hasFollowers;
    _popoverView.refreshing = _isRefreshing;
    _popoverView.profileEnabled = _card.mid > 0;
    _popoverView.historyEnabled = _weekPointCount > 0;
    _popoverView.notificationsEnabled = notificationsEnabled;
    BOOL launchAtLoginSupported = [self launchAtLoginSupported];
    _popoverView.launchAtLoginAvailable = launchAtLoginSupported;
    _popoverView.launchAtLoginEnabled = [self launchAtLoginEnabledIfSupported:launchAtLoginSupported];
    _popoverView.refreshInterval = refreshInterval;
    _popoverView.autoRefreshEnabled = autoRefreshEnabled;
    _popoverView.statusDisplayMode = statusDisplayMode;
    _popoverView.appearanceMode = appearanceMode;
    [self syncVisibleControlSurfaces];
}

- (void)syncStatusPopover {
    [self syncStatusPopoverWithScheduleText:nil];
}

- (void)syncVisibleControlSurfaces {
    [self syncMainPageControlsIfVisible];
    [self syncPreferencesWindowControlsIfVisible];
}

- (void)syncStatusPopoverIfVisibleWithScheduleText:(NSString *)providedScheduleText {
    if ([self isStatusSurfaceShown]) {
        [self syncStatusPopoverWithScheduleText:providedScheduleText];
    } else {
        [self syncVisibleControlSurfaces];
    }
}

- (void)syncStatusPopoverIfVisible {
    [self syncStatusPopoverIfVisibleWithScheduleText:nil];
}

- (void)syncMainPageControlsIfVisible {
    if (_window.isVisible) {
        [self syncMainPageControls];
    }
}

- (void)statusItemClicked:(id)sender {
    NSEvent *event = NSApp.currentEvent;
    if (event.type == NSEventTypeRightMouseUp || (event.modifierFlags & NSEventModifierFlagControl)) {
        [self closeStatusPopover];
        [NSMenu popUpContextMenu:[self makeStatusMenu] withEvent:event forView:_statusItem.button];
        [self clearStatusButtonHighlight];
        return;
    }
    [self toggleStatusPopover];
}

- (void)toggleStatusPopover {
    if ([self isStatusSurfaceShown]) {
        [self closeStatusPopover];
        return;
    }
    [self syncStatusPopover];
    _isOpeningStatusSurface = YES;
    [self ensureStatusPanel];
    [self syncStatusPopover];
    if (_statusPanel) {
        [self positionStatusPanel];
        [self applyStatusPopoverGlassStyle];
        [_statusPanel orderFrontRegardless];
    } else {
        [_statusPopover showRelativeToRect:_statusItem.button.bounds ofView:_statusItem.button preferredEdge:NSRectEdgeMinY];
    }
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.45 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        self->_isOpeningStatusSurface = NO;
        [self syncStatusPopoverIfVisible];
        [self applyStatusPopoverGlassStyle];
        [self clearStatusButtonHighlight];
    });
    [self clearStatusButtonHighlight];
    [self updateStatusItemTitleSyncVisibleSurfaces:NO];
    [self syncMainStatusWidgetButton];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)closeStatusPopover {
    if (_statusPanel.isVisible) {
        [_statusPanel orderOut:nil];
    }
    if (_statusPopover.shown) {
        [_statusPopover close];
    }
    [self clearStatusButtonHighlight];
    [self updateStatusItemTitleSyncVisibleSurfaces:NO];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)popoverDidClose:(NSNotification *)notification {
    [self updateStatusItemTitleSyncVisibleSurfaces:NO];
    [self syncMainStatusWidgetButton];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (NSImage *)statusBarWidgetIconActive:(BOOL)active {
    (void)active;
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(18, 18)];
    image.template = YES;
    [image lockFocus];
    DrawBilibiliBLetter(NSMakeRect(1.0, 2.0, 16.0, 14.2), [NSColor.blackColor colorWithAlphaComponent:0.95]);
    [image unlockFocus];
    return image;
}

- (NSImage *)cachedStatusBarWidgetIconActive:(BOOL)active {
    if (active) {
        if (!_statusIconActive) {
            _statusIconActive = [self statusBarWidgetIconActive:YES];
        }
        return _statusIconActive;
    }
    if (!_statusIconInactive) {
        _statusIconInactive = [self statusBarWidgetIconActive:NO];
    }
    return _statusIconInactive;
}

- (NSString *)exactFollowersText {
    [self refreshCachedFollowerTextsIfNeeded];
    return _cachedExactFollowersText ?: @"0";
}

- (NSString *)compactValue:(double)value suffix:(NSString *)suffix {
    NSString *text = value >= 100 ? [NSString stringWithFormat:@"%.0f", value] : [NSString stringWithFormat:@"%.1f", value];
    if ([text hasSuffix:@".0"]) {
        text = [text substringToIndex:text.length - 2];
    }
    return [text stringByAppendingString:suffix];
}

- (NSString *)compactFollowersText {
    [self refreshCachedFollowerTextsIfNeeded];
    return _cachedCompactFollowersText ?: [self exactFollowersText];
}

- (void)refreshCachedFollowerTextsIfNeeded {
    NSInteger followers = _card.followers;
    if (_hasCachedFollowersTextValue && _cachedFollowersTextValue == followers) {
        return;
    }
    _cachedFollowersTextValue = followers;
    _hasCachedFollowersTextValue = YES;
    _cachedExactFollowersText = [DecimalStringForInteger(followers) copy];
    if (followers >= 100000000) {
        _cachedCompactFollowersText = [[self compactValue:followers / 100000000.0 suffix:@"亿"] copy];
    } else if (followers >= 10000) {
        _cachedCompactFollowersText = [[self compactValue:followers / 10000.0 suffix:@"万"] copy];
    } else {
        _cachedCompactFollowersText = [_cachedExactFollowersText copy];
    }
}

- (NSString *)currentTrendSummaryText {
    return _currentTrendSummaryText.length > 0 ? _currentTrendSummaryText : @"暂无趋势";
}

- (void)updateStatusItemTitle {
    [self updateStatusItemTitleSyncVisibleSurfaces:YES];
}

- (void)updateStatusItemTitleSyncVisibleSurfaces:(BOOL)syncVisibleSurfaces {
    [self updateStatusItemTitleSyncVisibleSurfaces:syncVisibleSurfaces scheduleText:nil autoRefreshEnabled:[Settings autoRefreshEnabled]];
}

- (void)updateStatusItemTitleSyncVisibleSurfaces:(BOOL)syncVisibleSurfaces scheduleText:(NSString *)providedScheduleText autoRefreshEnabled:(BOOL)autoRefreshEnabled {
    if (!_statusItem.button) return;
    BOOL iconActive = [self isStatusSurfaceShown] || _isRefreshing || (_card.hasFollowers && autoRefreshEnabled);
    NSInteger displayMode = [Settings statusDisplayMode];
    NSString *imageText = @"";
    NSString *tooltip = nil;
    if (!_card.hasFollowers) {
        NSString *mode = providedScheduleText ?: [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled];
        NSString *name = _card.name.length > 0 ? _card.name : ([Settings name].length > 0 ? [Settings name] : @"BILI粉丝数");
        long long mid = _card.mid > 0 ? _card.mid : [Settings mid];
        NSString *target = mid > 0
            ? [NSString stringWithFormat:@"%@ · UID %lld", name, mid]
            : name;
        tooltip = mode.length > 0
            ? [NSString stringWithFormat:@"BILI粉丝数 · %@ · %@ · 左键小组件 · 右键菜单", target, mode]
            : [NSString stringWithFormat:@"BILI粉丝数 · %@ · 左键小组件 · 右键菜单", target];
        [self applyStatusItemLength:NSVariableStatusItemLength imageText:imageText active:iconActive tooltip:tooltip];
        if (syncVisibleSurfaces) {
            [self syncStatusPopoverIfVisibleWithScheduleText:mode];
        }
        return;
    }
    NSString *followers = [self exactFollowersText];
    NSString *name = _card.name.length > 0 ? _card.name : @"BILI粉丝数";
    NSString *updated = _card.updatedText.length > 0 ? _card.updatedText : @"等待刷新";
    if (displayMode == StatusDisplayModeIconOnly) {
        imageText = @"";
    } else {
        imageText = displayMode == StatusDisplayModeCompact ? [self compactFollowersText] : followers;
    }
    NSString *mode = autoRefreshEnabled ? (providedScheduleText ?: [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled]) : @"已暂停";
    tooltip = [NSString stringWithFormat:@"%@ · %@ · %@ · %@ · %@ · 左键小组件 · 右键菜单",
               name,
               followers,
               updated,
               [self currentTrendSummaryText],
               mode.length > 0 ? mode : @"自动刷新"];
    [self applyStatusItemLength:NSVariableStatusItemLength imageText:imageText active:iconActive tooltip:tooltip];
    if (syncVisibleSurfaces) {
        [self syncStatusPopoverIfVisibleWithScheduleText:mode];
    }
}

- (void)updateTimeSensitiveStatusText {
    BOOL autoRefreshEnabled = [Settings autoRefreshEnabled];
    NSString *scheduleText = [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled];
    [self updateStatusItemTitleSyncVisibleSurfaces:NO scheduleText:scheduleText autoRefreshEnabled:autoRefreshEnabled];

    if ([self isStatusSurfaceShown]) {
        _popoverView.refreshScheduleText = scheduleText;
    }
    if (_window.isVisible && _mainScheduleLabel) {
        [self syncMainScheduleLabelWithScheduleText:scheduleText autoRefreshEnabled:autoRefreshEnabled];
    }
}

- (void)syncMainScheduleLabelWithScheduleText:(NSString *)scheduleText autoRefreshEnabled:(BOOL)autoRefreshEnabled {
    if (!_mainScheduleLabel) return;

    static NSString * const prefix = @"自动刷新：";
    NSString *suffix = scheduleText.length > 0 ? scheduleText : @"等待计时";
    NSString *current = _mainScheduleLabel.stringValue ?: @"";
    BOOL matches = current.length == prefix.length + suffix.length &&
        [current hasPrefix:prefix] &&
        [current compare:suffix options:0 range:NSMakeRange(prefix.length, suffix.length)] == NSOrderedSame;
    if (!matches) {
        SetTextFieldStringIfChanged(_mainScheduleLabel, [prefix stringByAppendingString:suffix]);
    }
    SetTextFieldColorIfChanged(_mainScheduleLabel,
                               autoRefreshEnabled ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor);
}

- (void)applyStatusItemLength:(CGFloat)length imageText:(NSString *)imageText active:(BOOL)active tooltip:(NSString *)tooltip {
    (void)length;
    CGFloat effectiveLength = NSVariableStatusItemLength;
    if (!_hasLastStatusLength || _lastStatusLength != effectiveLength) {
        _statusItem.length = effectiveLength;
        _lastStatusLength = effectiveLength;
        _hasLastStatusLength = YES;
    }
    NSString *title = imageText.length > 0 ? [@" " stringByAppendingString:imageText] : @"";
    if (![_lastStatusTitle isEqualToString:title]) {
        _statusItem.button.title = title;
        _lastStatusTitle = [title copy];
    }
    if (!_hasLastStatusIconState || _lastStatusIconActive != active || !_statusItem.button.image) {
        _statusItem.button.image = [self cachedStatusBarWidgetIconActive:active];
        _lastStatusIconActive = active;
        _hasLastStatusIconState = YES;
    }
    if (![_lastStatusTooltip isEqualToString:tooltip]) {
        _statusItem.button.toolTip = tooltip;
        _lastStatusTooltip = [tooltip copy];
    }
}

- (NSMenu *)makeStatusMenu {
    NSMenu *menu = [NSMenu new];
    NSMenuItem *toggle = [[NSMenuItem alloc] initWithTitle:@"显示/隐藏主窗口" action:@selector(toggleCardWindow:) keyEquivalent:@""];
    toggle.target = self;
    [menu addItem:toggle];

    NSMenuItem *menuBarOnly = [[NSMenuItem alloc] initWithTitle:@"仅顶栏模式" action:@selector(toggleMenuBarOnly:) keyEquivalent:@""];
    menuBarOnly.target = self;
    [menu addItem:menuBarOnly];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *refresh = [[NSMenuItem alloc] initWithTitle:@"刷新" action:@selector(statusRefresh:) keyEquivalent:@""];
    refresh.target = self;
    [menu addItem:refresh];

    NSMenuItem *autoRefresh = [[NSMenuItem alloc] initWithTitle:@"自动刷新" action:@selector(statusToggleAutoRefresh:) keyEquivalent:@""];
    autoRefresh.target = self;
    [menu addItem:autoRefresh];

    NSMenuItem *notify = [[NSMenuItem alloc] initWithTitle:@"粉丝变化提醒" action:@selector(statusToggleChangeNotifications:) keyEquivalent:@""];
    notify.target = self;
    [menu addItem:notify];

    NSMenuItem *login = [[NSMenuItem alloc] initWithTitle:@"登录时启动" action:@selector(statusToggleLaunchAtLogin:) keyEquivalent:@""];
    login.target = self;
    [menu addItem:login];

    [menu addItem:[NSMenuItem separatorItem]];

    [menu addItem:[self submenuItemWithTitle:@"最近监控" submenu:[self recentTargetsMenu]]];

    NSMenuItem *profile = [[NSMenuItem alloc] initWithTitle:@"打开 B站主页" action:@selector(statusOpenProfile:) keyEquivalent:@""];
    profile.target = self;
    [menu addItem:profile];

    NSMenuItem *settings = [[NSMenuItem alloc] initWithTitle:@"设置 B站 UID..." action:@selector(statusSettings:) keyEquivalent:@""];
    settings.target = self;
    [menu addItem:settings];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *statusDisplay = [[NSMenuItem alloc] initWithTitle:@"顶栏显示" action:nil keyEquivalent:@""];
    NSMenu *statusDisplayMenu = [NSMenu new];
    NSMenuItem *exactDisplay = [[NSMenuItem alloc] initWithTitle:@"精确数字" action:@selector(statusUseExactDisplay:) keyEquivalent:@""];
    exactDisplay.target = self;
    [statusDisplayMenu addItem:exactDisplay];
    NSMenuItem *compactDisplay = [[NSMenuItem alloc] initWithTitle:@"紧凑数字" action:@selector(statusUseCompactDisplay:) keyEquivalent:@""];
    compactDisplay.target = self;
    [statusDisplayMenu addItem:compactDisplay];
    NSMenuItem *iconOnlyDisplay = [[NSMenuItem alloc] initWithTitle:@"仅图标" action:@selector(statusUseIconOnlyDisplay:) keyEquivalent:@""];
    iconOnlyDisplay.target = self;
    [statusDisplayMenu addItem:iconOnlyDisplay];
    statusDisplay.submenu = statusDisplayMenu;
    [menu addItem:statusDisplay];

    [menu addItem:[self submenuItemWithTitle:@"涨粉趋势" submenu:[self trendRangeMenu]]];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *copy = [[NSMenuItem alloc] initWithTitle:@"复制粉丝数" action:@selector(statusCopyFollowers:) keyEquivalent:@""];
    copy.target = self;
    [menu addItem:copy];

    NSMenuItem *history = [[NSMenuItem alloc] initWithTitle:@"复制一周 CSV" action:@selector(statusCopyHistory:) keyEquivalent:@""];
    history.target = self;
    [menu addItem:history];

    NSMenuItem *trendCSV = [[NSMenuItem alloc] initWithTitle:@"复制当前趋势 CSV" action:@selector(statusCopyTrend:) keyEquivalent:@""];
    trendCSV.target = self;
    [menu addItem:trendCSV];

    [menu addItem:[NSMenuItem separatorItem]];

    NSMenuItem *appearance = [[NSMenuItem alloc] initWithTitle:@"外观" action:nil keyEquivalent:@""];
    NSMenu *appearanceMenu = [NSMenu new];
    NSMenuItem *ultraClear = [[NSMenuItem alloc] initWithTitle:@"极清透" action:@selector(statusUseUltraClear:) keyEquivalent:@""];
    ultraClear.target = self;
    [appearanceMenu addItem:ultraClear];
    NSMenuItem *clear = [[NSMenuItem alloc] initWithTitle:@"清透" action:@selector(statusUseClear:) keyEquivalent:@""];
    clear.target = self;
    [appearanceMenu addItem:clear];
    NSMenuItem *readable = [[NSMenuItem alloc] initWithTitle:@"增强可读" action:@selector(statusUseReadable:) keyEquivalent:@""];
    readable.target = self;
    [appearanceMenu addItem:readable];
    appearance.submenu = appearanceMenu;
    [menu addItem:appearance];

    NSMenuItem *interval = [[NSMenuItem alloc] initWithTitle:@"刷新间隔" action:nil keyEquivalent:@""];
    NSMenu *intervalMenu = [NSMenu new];
    NSMenuItem *oneMinute = [[NSMenuItem alloc] initWithTitle:@"1 分钟" action:@selector(statusUseOneMinute:) keyEquivalent:@""];
    oneMinute.target = self;
    [intervalMenu addItem:oneMinute];
    NSMenuItem *fiveMinutes = [[NSMenuItem alloc] initWithTitle:@"5 分钟" action:@selector(statusUseFiveMinutes:) keyEquivalent:@""];
    fiveMinutes.target = self;
    [intervalMenu addItem:fiveMinutes];
    NSMenuItem *fifteenMinutes = [[NSMenuItem alloc] initWithTitle:@"15 分钟" action:@selector(statusUseFifteenMinutes:) keyEquivalent:@""];
    fifteenMinutes.target = self;
    [intervalMenu addItem:fifteenMinutes];
    interval.submenu = intervalMenu;
    [menu addItem:interval];

    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"退出 BILI粉丝数" action:@selector(statusQuit:) keyEquivalent:@""];
    quit.target = self;
    [menu addItem:quit];
    return menu;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    SEL action = item.action;
    if (action == @selector(toggleCardWindow:)) {
        SetMenuItemTitleIfChanged(item, _window.isVisible ? @"隐藏主窗口" : @"显示主窗口");
        return YES;
    }
    if (action == @selector(showCardWindow)) {
        return !_window.isVisible;
    }
    if (action == @selector(hideCardWindow)) {
        return _window.isVisible;
    }
    if (action == @selector(statusRefresh:)) {
        SetMenuItemTitleIfChanged(item, _isRefreshing ? @"正在刷新" : @"刷新");
        return !_isRefreshing;
    }
    if (action == @selector(toggleMenuBarOnly:)) {
        SetMenuItemStateIfChanged(item, [Settings launchHidden] ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusToggleAlwaysOnTop:)) {
        SetMenuItemStateIfChanged(item, [Settings alwaysOnTop] ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusTogglePositionLocked:)) {
        SetMenuItemStateIfChanged(item, [Settings positionLocked] ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusToggleAutoRefresh:)) {
        BOOL autoRefreshEnabled = [Settings autoRefreshEnabled];
        NSString *schedule = [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled];
        SetMenuItemTitleIfChanged(item,
                                  autoRefreshEnabled && schedule.length > 0
                                    ? [NSString stringWithFormat:@"自动刷新（%@）", schedule]
                                    : @"自动刷新");
        SetMenuItemStateIfChanged(item, autoRefreshEnabled ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusToggleChangeNotifications:)) {
        SetMenuItemStateIfChanged(item, [Settings changeNotificationsEnabled] ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusToggleLaunchAtLogin:)) {
        NSString *unavailableReason = [self launchAtLoginUnavailableReason];
        if (unavailableReason) {
            SetMenuItemTitleIfChanged(item, @"登录时启动（需先安装）");
            SetMenuItemStateIfChanged(item, NSControlStateValueOff);
            return NO;
        }
        SetMenuItemTitleIfChanged(item, @"登录时启动");
        SetMenuItemStateIfChanged(item, [self launchAtLoginEnabledIfSupported:YES] ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusUseExactDisplay:) ||
        action == @selector(statusUseCompactDisplay:) ||
        action == @selector(statusUseIconOnlyDisplay:)) {
        NSInteger mode = [Settings statusDisplayMode];
        BOOL checked = (action == @selector(statusUseExactDisplay:) && mode == StatusDisplayModeExact)
                || (action == @selector(statusUseCompactDisplay:) && mode == StatusDisplayModeCompact)
                || (action == @selector(statusUseIconOnlyDisplay:) && mode == StatusDisplayModeIconOnly);
        SetMenuItemStateIfChanged(item, checked ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusCopyFollowers:)) {
        return _card.hasFollowers;
    }
    if (action == @selector(statusCopyHistory:)) {
        SetMenuItemTitleIfChanged(item, @"复制一周 CSV");
        return _weekPointCount > 0;
    }
    if (action == @selector(statusCopyTrend:)) {
        SetMenuItemTitleIfChanged(item, [NSString stringWithFormat:@"复制%@趋势 CSV", [self trendRangeTitle]]);
        return _trendPointCount > 0;
    }
    if (action == @selector(confirmAndClearCurrentHistory)) {
        return _historyPointCount > 0;
    }
    if (action == @selector(statusUseUltraClear:) ||
        action == @selector(statusUseClear:) ||
        action == @selector(statusUseReadable:)) {
        NSInteger mode = [Settings appearanceMode];
        BOOL checked = (action == @selector(statusUseUltraClear:) && mode == AppearanceModeUltraClear)
                || (action == @selector(statusUseClear:) && mode == AppearanceModeClear)
                || (action == @selector(statusUseReadable:) && mode == AppearanceModeReadable);
        SetMenuItemStateIfChanged(item, checked ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusUseOneMinute:) ||
        action == @selector(statusUseFiveMinutes:) ||
        action == @selector(statusUseFifteenMinutes:)) {
        NSTimeInterval interval = [Settings refreshInterval];
        BOOL checked = (action == @selector(statusUseOneMinute:) && interval <= 90)
                || (action == @selector(statusUseFiveMinutes:) && interval > 90 && interval <= 450)
                || (action == @selector(statusUseFifteenMinutes:) && interval > 450);
        SetMenuItemStateIfChanged(item, checked ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    if (action == @selector(statusUseSevenDayTrend:) ||
        action == @selector(statusUseThirtyDayTrend:) ||
        action == @selector(statusUseNinetyDayTrend:)) {
        NSInteger range = [Settings trendRange];
        BOOL checked = (action == @selector(statusUseSevenDayTrend:) && range == TrendRangeSevenDays)
                || (action == @selector(statusUseThirtyDayTrend:) && range == TrendRangeThirtyDays)
                || (action == @selector(statusUseNinetyDayTrend:) && range == TrendRangeNinetyDays);
        SetMenuItemStateIfChanged(item, checked ? NSControlStateValueOn : NSControlStateValueOff);
        return YES;
    }
    return YES;
}

- (void)hideCardWindow {
    [_window orderOut:nil];
    [self showTransientCardStatus:@"主窗口已隐藏"];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)clearMainWindowTransientTextFocus {
    if (!_window || !_mainUIDField) return;
    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        __strong typeof(weakSelf) self = weakSelf;
        if (!self || !self->_window.isVisible) return;
        if ([self textFieldIsBeingEdited:self->_mainUIDField] || self->_window.firstResponder == self->_mainUIDField) {
            [self->_window makeFirstResponder:nil];
        }
    });
}

- (void)showCardWindow {
    [NSApp activateIgnoringOtherApps:YES];
    [_window makeKeyAndOrderFront:nil];
    [self clearMainWindowTransientTextFocus];
    [self showTransientCardStatus:@"主窗口已显示"];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)showCardWindowTemporarily {
    [NSApp activateIgnoringOtherApps:YES];
    [_window makeKeyAndOrderFront:nil];
    [self clearMainWindowTransientTextFocus];
    [self syncMainPageControlsIfVisible];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)toggleCardWindow:(id)sender {
    if (_window.isVisible) {
        [self hideCardWindow];
    } else {
        [self showCardWindow];
    }
}

- (void)toggleMenuBarOnly:(id)sender {
    BOOL enabled = ![Settings launchHidden];
    [self setMenuBarOnlyEnabled:enabled];
}

- (void)setMenuBarOnlyEnabled:(BOOL)enabled {
    [Settings setLaunchHidden:enabled];
    if (enabled) {
        [_window orderOut:nil];
        [self showTransientCardStatus:@"仅顶栏模式"];
    } else {
        [NSApp activateIgnoringOtherApps:YES];
        [_window makeKeyAndOrderFront:nil];
        [self clearMainWindowTransientTextFocus];
        [self showTransientCardStatus:@"主窗口已显示"];
    }
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)statusRefresh:(id)sender {
    if (_isRefreshing) {
        [self showTransientCardStatus:@"正在刷新"];
        [self syncStatusPopoverIfVisible];
        return;
    }
    [self refresh:YES];
}

- (void)statusToggleAutoRefresh:(id)sender {
    [self setAutoRefreshEnabled:![Settings autoRefreshEnabled]];
}

- (void)statusToggleChangeNotifications:(id)sender {
    [self setChangeNotificationsEnabled:![Settings changeNotificationsEnabled]];
}

- (void)statusToggleLaunchAtLogin:(id)sender {
    [self setLaunchAtLoginEnabled:![self launchAtLoginEnabled]];
}

- (void)statusToggleAlwaysOnTop:(id)sender {
    [self setAlwaysOnTop:![Settings alwaysOnTop]];
}

- (void)statusTogglePositionLocked:(id)sender {
    [self setPositionLocked:![Settings positionLocked]];
}

- (void)statusUseExactDisplay:(id)sender {
    [self setStatusDisplayMode:StatusDisplayModeExact];
}

- (void)statusUseCompactDisplay:(id)sender {
    [self setStatusDisplayMode:StatusDisplayModeCompact];
}

- (void)statusUseIconOnlyDisplay:(id)sender {
    [self setStatusDisplayMode:StatusDisplayModeIconOnly];
}

- (void)statusCopyFollowers:(id)sender {
    if (!_card.hasFollowers) {
        [self showTransientCardStatus:@"暂无数据"];
        [self showCardWindowTemporarily];
        return;
    }
    NSString *value = DecimalStringForInteger(_card.followers);
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:value forType:NSPasteboardTypeString];
    [self showTransientCardStatus:@"已复制"];
}

- (void)statusCopyHistory:(id)sender {
    [self copyHistoryCSV];
}

- (void)statusCopyTrend:(id)sender {
    [self copyTrendCSV];
}

- (void)statusOpenProfile:(id)sender {
    if (_card.mid <= 0) {
        [self showTransientCardStatus:@"UID 无效"];
        [self showCardWindowTemporarily];
        return;
    }
    NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"https://space.bilibili.com/%lld", _card.mid]];
    BOOL opened = [[NSWorkspace sharedWorkspace] openURL:url];
    [self showTransientCardStatus:opened ? @"已打开主页" : @"主页打开失败"];
}

- (void)statusSettings:(id)sender {
    [self showPreferences:sender];
}

- (void)statusUseRecentTarget:(id)sender {
    if (![sender isKindOfClass:NSMenuItem.class]) {
        return;
    }
    long long mid = [((NSMenuItem *)sender).representedObject longLongValue];
    if (mid <= 0) {
        [self showTransientCardStatus:@"UID 无效"];
        return;
    }
    if (mid == [Settings mid]) {
        [self showTransientCardStatus:@"已在监控"];
        return;
    }
    [self applyMID:mid];
}

- (void)showPreferences:(id)sender {
    [self showPreferencesWindow];
}

- (void)statusUseUltraClear:(id)sender {
    [self setAppearanceMode:AppearanceModeUltraClear];
}

- (void)statusUseClear:(id)sender {
    [self setAppearanceMode:AppearanceModeClear];
}

- (void)statusUseReadable:(id)sender {
    [self setAppearanceMode:AppearanceModeReadable];
}

- (void)statusUseOneMinute:(id)sender {
    [self setRefreshInterval:60];
}

- (void)statusUseFiveMinutes:(id)sender {
    [self setRefreshInterval:300];
}

- (void)statusUseFifteenMinutes:(id)sender {
    [self setRefreshInterval:900];
}

- (void)setTrendRange:(NSInteger)range {
    [Settings setTrendRange:range];
    [self updateHistoryValues];
    _card.needsDisplay = YES;
    [self syncStatusPopoverIfVisible];
    [self showTransientCardStatus:[NSString stringWithFormat:@"已切换%@趋势", [self trendRangeTitle]]];
}

- (void)statusUseSevenDayTrend:(id)sender {
    [self setTrendRange:TrendRangeSevenDays];
}

- (void)statusUseThirtyDayTrend:(id)sender {
    [self setTrendRange:TrendRangeThirtyDays];
}

- (void)statusUseNinetyDayTrend:(id)sender {
    [self setTrendRange:TrendRangeNinetyDays];
}

- (void)statusQuit:(id)sender {
    [NSApp terminate:nil];
}

- (void)setStatusDisplayMode:(NSInteger)mode {
    [Settings setStatusDisplayMode:mode];
    NSInteger normalized = [Settings statusDisplayMode];
    NSString *message = @"顶栏精确数字";
    if (normalized == StatusDisplayModeIconOnly) {
        message = @"顶栏仅图标";
    } else if (normalized == StatusDisplayModeCompact) {
        message = @"顶栏紧凑数字";
    }
    [self showTransientCardStatus:message];
}

- (NSRect)defaultWindowFrame {
    NSRect visible = NSScreen.mainScreen.visibleFrame;
    return NSMakeRect(NSMidX(visible) - MainWindowWidth / 2.0, NSMidY(visible) - MainWindowHeight / 2.0, MainWindowWidth, MainWindowHeight);
}

- (void)setAppearanceMode:(NSInteger)mode {
    [Settings setAppearanceMode:mode];
    _chromeView.appearanceMode = [Settings appearanceMode];
    _preferencesChromeView.appearanceMode = [Settings appearanceMode];
    _card.appearanceMode = [Settings appearanceMode];
    _popoverView.appearanceMode = [Settings appearanceMode];
    NSString *message = @"极清透";
    if (_card.appearanceMode == AppearanceModeReadable) {
        message = @"增强可读";
    } else if (_card.appearanceMode == AppearanceModeClear) {
        message = @"清透显示";
    }
    [self applyVisualEffectMaterials];
    [self showTransientCardStatus:message];
}

- (void)setRefreshInterval:(NSTimeInterval)interval {
    [Settings setRefreshInterval:interval];
    _card.refreshInterval = [Settings refreshInterval];
    _consecutiveRefreshFailures = 0;
    [self startAutoRefresh];
    NSString *message = [Settings autoRefreshEnabled]
        ? [NSString stringWithFormat:@"每 %.0f 分钟刷新", _card.refreshInterval / 60.0]
        : [NSString stringWithFormat:@"已设为 %.0f 分钟", _card.refreshInterval / 60.0];
    [self showTransientCardStatus:message];
}

- (void)setAutoRefreshEnabled:(BOOL)enabled {
    [Settings setAutoRefreshEnabled:enabled];
    _consecutiveRefreshFailures = 0;
    if (enabled && !_card.hasFollowers && !_isRefreshing) {
        [_timer invalidate];
        _timer = nil;
        _nextAutoRefreshAt = nil;
        [self refresh:YES];
        [self showTransientCardStatus:@"正在刷新"];
        return;
    }
    [self startAutoRefresh];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
    if (enabled) {
        _stableCardStatus = [_card.hasFollowers ? @"实时统计" : @"等待刷新" copy];
        [self showTransientCardStatus:@"自动刷新已开启"];
    } else {
        [self setStableCardStatus:@"自动刷新已暂停"];
    }
}

- (void)setChangeNotificationsEnabled:(BOOL)enabled {
    [Settings setChangeNotificationsEnabled:enabled];
    if (enabled) {
        [self requestNotificationAuthorizationIfNeeded];
        [self showTransientCardStatus:@"变化提醒已开启"];
    } else {
        [self showTransientCardStatus:@"变化提醒已关闭"];
    }
}

- (BOOL)isRunningFromMountedDiskImage {
    return [self bundlePathIsMountedDiskImage:NSBundle.mainBundle.bundlePath];
}

- (NSString *)launchAtLoginUnavailableReason {
    if (_launchAtLoginAvailabilityResolved) {
        return _launchAtLoginUnavailableReason;
    }
    NSString *reason = nil;
    if (@available(macOS 13.0, *)) {
        if ([self isRunningFromMountedDiskImage]) {
            reason = @"请先拖到“应用程序”后再开启登录启动";
        }
    } else {
        reason = @"登录启动需要 macOS 13+";
    }
    _launchAtLoginUnavailableReason = [reason copy];
    _launchAtLoginAvailabilityResolved = YES;
    return _launchAtLoginUnavailableReason;
}

- (BOOL)launchAtLoginSupported {
    return [self launchAtLoginUnavailableReason] == nil;
}

- (BOOL)launchAtLoginEnabledIfSupported:(BOOL)supported {
    if (!supported) {
        return NO;
    }
    if (@available(macOS 13.0, *)) {
        return SMAppService.mainAppService.status == SMAppServiceStatusEnabled;
    }
    return NO;
}

- (BOOL)launchAtLoginEnabled {
    return [self launchAtLoginEnabledIfSupported:[self launchAtLoginSupported]];
}

- (void)setLaunchAtLoginEnabled:(BOOL)enabled {
    NSString *unavailableReason = [self launchAtLoginUnavailableReason];
    if (unavailableReason) {
        [self showTransientCardStatus:unavailableReason];
        return;
    }
    if (@available(macOS 13.0, *)) {
        NSError *error = nil;
        BOOL ok = enabled
            ? [SMAppService.mainAppService registerAndReturnError:&error]
            : [SMAppService.mainAppService unregisterAndReturnError:&error];
        if (ok) {
            SMAppServiceStatus status = SMAppService.mainAppService.status;
            if (enabled && status == SMAppServiceStatusRequiresApproval) {
                [self showTransientCardStatus:@"需系统批准"];
            } else {
                [self showTransientCardStatus:enabled ? @"登录启动已开启" : @"登录启动已关闭"];
            }
        } else {
            [self showTransientCardStatus:error.localizedDescription.length > 0 ? error.localizedDescription : @"登录启动失败"];
        }
    }
}

- (void)requestNotificationAuthorizationIfNeeded {
    UNUserNotificationCenter *center = UNUserNotificationCenter.currentNotificationCenter;
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                          completionHandler:^(BOOL granted, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!granted || error) {
                [Settings setChangeNotificationsEnabled:NO];
                [self showTransientCardStatus:@"通知未授权"];
            }
        });
    }];
}

- (void)notifyFollowerChangeForResult:(FanResult *)result delta:(NSInteger)delta {
    if (![Settings changeNotificationsEnabled] || delta == 0) {
        return;
    }
    NSString *direction = delta > 0 ? @"增加" : @"减少";
    NSString *followers = DecimalStringForInteger(result.followers);
    NSString *body = [NSString stringWithFormat:@"%@ %@ %ld，当前 %@", result.name ?: @"BILI粉丝数", direction, labs((long)delta), followers];

    UNMutableNotificationContent *content = [UNMutableNotificationContent new];
    content.title = @"BILI粉丝数变化";
    content.body = body;
    content.sound = UNNotificationSound.defaultSound;

    NSString *identifier = [NSString stringWithFormat:@"fans-change-%lld-%.0f", result.mid, result.updatedAt.timeIntervalSince1970];
    UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:identifier content:content trigger:nil];
    [UNUserNotificationCenter.currentNotificationCenter addNotificationRequest:request withCompletionHandler:nil];
}

- (void)setAlwaysOnTop:(BOOL)enabled {
    [Settings setAlwaysOnTop:enabled];
    _card.alwaysOnTop = enabled;
    [self applyWindowBehavior];
    [self showTransientCardStatus:enabled ? @"置顶显示" : @"取消置顶"];
}

- (void)setPositionLocked:(BOOL)locked {
    [Settings setPositionLocked:locked];
    _card.positionLocked = locked;
    [self applyWindowBehavior];
    [self showTransientCardStatus:locked ? @"位置已锁定" : @"可拖动"];
}

- (void)resetWindowPosition {
    NSRect frame = [self defaultWindowFrame];
    [_window setFrame:frame display:YES animate:YES];
    [self saveWindowFrameNow];
    [self showTransientCardStatus:@"已回到屏幕中央"];
}

- (NSString *)fallbackStableCardStatus {
    if (_stableCardStatus.length > 0) {
        return _stableCardStatus;
    }
    if (_card.hasFollowers) {
        return @"实时统计";
    }
    return [Settings autoRefreshEnabled] ? @"等待刷新" : @"自动刷新已暂停";
}

- (void)setStableCardStatus:(NSString *)status {
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    _stableCardStatus = [(status.length > 0 ? status : [self fallbackStableCardStatus]) copy];
    _card.status = _stableCardStatus;
    [self updateStatusItemTitle];
}

- (void)showTransientCardStatus:(NSString *)status {
    NSString *transient = [status copy] ?: @"";
    if (transient.length == 0) {
        return;
    }
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    _card.status = transient;
    [self updateStatusItemTitle];

    __weak typeof(self) weakSelf = self;
    _transientStatusTimer = [NSTimer timerWithTimeInterval:2.6 repeats:NO block:^(__unused NSTimer *timer) {
        AppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        strongSelf->_transientStatusTimer = nil;
        if (strongSelf->_isRefreshing || ![strongSelf->_card.status isEqualToString:transient]) {
            return;
        }
        strongSelf->_card.status = [strongSelf fallbackStableCardStatus];
        [strongSelf updateStatusItemTitle];
    }];
    _transientStatusTimer.tolerance = 0.4;
    [[NSRunLoop mainRunLoop] addTimer:_transientStatusTimer forMode:NSRunLoopCommonModes];
}

- (NSTimeInterval)trendIntervalForRange:(NSInteger)range {
    NSTimeInterval day = 24 * 60 * 60;
    if (range == TrendRangeNinetyDays) {
        return 90 * day;
    }
    if (range == TrendRangeSevenDays) {
        return 7 * day;
    }
    return 30 * day;
}

- (NSString *)trendRangeTitleForRange:(NSInteger)range {
    if (range == TrendRangeNinetyDays) {
        return @"90天";
    }
    if (range == TrendRangeSevenDays) {
        return @"7天";
    }
    return @"30天";
}

- (NSString *)trendRangeTitle {
    return [self trendRangeTitleForRange:[Settings trendRange]];
}

- (NSArray<NSDictionary *> *)history:(NSArray<NSDictionary *> *)history filteredForTrendRange:(NSInteger)range {
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - [self trendIntervalForRange:range];
    return TrendHistoryIncludingBaseline(history, cutoff);
}

- (NSArray<NSNumber *> *)sparklineValuesForTrendHistory:(NSArray<NSDictionary *> *)history {
    if (history.count == 0) {
        return @[];
    }
    NSMutableArray<NSNumber *> *values = [NSMutableArray new];
    if (history.count <= MaxSparklinePoints) {
        for (NSDictionary *point in history) {
            [values addObject:point[@"followers"]];
        }
        return values.copy;
    }
    for (NSUInteger i = 0; i < MaxSparklinePoints; i++) {
        double position = (double)i * (double)(history.count - 1) / (double)(MaxSparklinePoints - 1);
        NSUInteger index = (NSUInteger)llround(position);
        index = MIN(index, history.count - 1);
        [values addObject:history[index][@"followers"]];
    }
    return values.copy;
}

- (void)updateHistoryValues {
    long long currentMID = [Settings mid];
    NSTimeInterval now = NSDate.date.timeIntervalSince1970;
    NSInteger trendRange = [Settings trendRange];
    NSArray<NSDictionary *> *history = SortedUsableHistoryPoints([Settings historyForCurrentTarget]);
    [Settings backfillSegmentedHistoryCSVIfNeededForMID:currentMID history:history];
    NSTimeInterval cutoff = now - [self trendIntervalForRange:trendRange];
    NSTimeInterval weekCutoff = now - OneWeekInterval;
    NSArray<NSDictionary *> *trendHistory = TrendHistoryIncludingBaselineFromSortedPoints(history, cutoff);
    _historyPointCount = history.count;
    _weekPointCount = HistoryPointCountAtOrAfterCutoffFromSortedUsablePoints(history, weekCutoff);
    _trendPointCount = HistoryPointCountAtOrAfterCutoffFromSortedUsablePoints(history, cutoff);
    _historyHasDelta = trendHistory.count >= 2;
    _historyDelta = _historyHasDelta
        ? [trendHistory.lastObject[@"followers"] integerValue] - [trendHistory.firstObject[@"followers"] integerValue]
        : 0;
    NSString *trendTitle = [self trendRangeTitleForRange:trendRange];
    if (_historyHasDelta) {
        NSString *deltaText = CompactSignedDelta(_historyDelta);
        _currentTrendSummaryText = [[NSString stringWithFormat:@"%@趋势 %@（%lu点）",
                                     trendTitle,
                                     deltaText,
                                     (unsigned long)_trendPointCount] copy];
        _mainTrendStatusText = [[NSString stringWithFormat:@"%@趋势 · %lu点 · %@",
                                 trendTitle,
                                 (unsigned long)_trendPointCount,
                                 deltaText] copy];
    } else if (_historyPointCount > 0) {
        _currentTrendSummaryText = [[NSString stringWithFormat:@"%@趋势积累中", trendTitle] copy];
        _mainTrendStatusText = [[NSString stringWithFormat:@"%@趋势 · 数据继续积累中", trendTitle] copy];
    } else {
        _currentTrendSummaryText = @"暂无趋势";
        _mainTrendStatusText = @"暂无历史点，刷新成功后自动记录。";
    }
    _trendCSVTooltipText = _trendPointCount > 0
        ? [[NSString stringWithFormat:@"复制%@趋势 CSV（含基线点）", trendTitle] copy]
        : [[NSString stringWithFormat:@"%@趋势数据继续积累中", trendTitle] copy];
    _card.historyValues = [self sparklineValuesForTrendHistory:trendHistory];
}

- (void)copyHistoryCSV {
    NSArray<NSDictionary *> *history = SortedUsableHistoryPoints([Settings historyForCurrentTarget]);
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - OneWeekInterval;
    NSArray<NSDictionary *> *weekHistory = HistoryPointsAtOrAfterCutoffFromSortedPoints(history, cutoff);
    if (weekHistory.count == 0) {
        [self showTransientCardStatus:@"近7天暂无历史"];
        return;
    }

    [self copyHistoryPoints:weekHistory status:@"一周 CSV 已复制"];
}

- (void)copyTrendCSV {
    NSInteger trendRange = [Settings trendRange];
    NSString *trendTitle = [self trendRangeTitleForRange:trendRange];
    NSArray<NSDictionary *> *history = SortedUsableHistoryPoints([Settings historyForCurrentTarget]);
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - [self trendIntervalForRange:trendRange];
    NSArray<NSDictionary *> *trendHistory = TrendHistoryIncludingBaselineFromSortedPoints(history, cutoff);
    if (trendHistory.count == 0) {
        [self showTransientCardStatus:@"暂无趋势"];
        return;
    }

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:CSVStringForHistoryPoints(trendHistory, YES, cutoff) forType:NSPasteboardTypeString];
    [self showTransientCardStatus:[NSString stringWithFormat:@"%@趋势已复制", trendTitle]];
}

- (void)copyHistoryPoints:(NSArray<NSDictionary *> *)points status:(NSString *)status {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:CSVStringForHistoryPoints(points, NO, 0) forType:NSPasteboardTypeString];
    [self showTransientCardStatus:status.length > 0 ? status : @"CSV 已复制"];
}

- (void)clearCurrentHistory {
    [Settings clearHistoryForCurrentTarget];
    [self updateHistoryValues];
    _card.hasFollowerDelta = NO;
    [self showTransientCardStatus:@"历史已清空"];
}

- (void)confirmAndClearCurrentHistory {
    if (_historyPointCount == 0) {
        [self showTransientCardStatus:@"暂无历史"];
        return;
    }

    NSString *name = _card.name.length > 0 ? _card.name : ([Settings name].length > 0 ? [Settings name] : UnknownTargetName);
    NSAlert *alert = [NSAlert new];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = @"清空当前历史？";
    alert.informativeText = [NSString stringWithFormat:@"将删除“%@”的 %lu 个本地历史点。当前粉丝数、最近监控账号和其他账号历史不会被清空。",
                             name,
                             (unsigned long)_historyPointCount];
    [alert addButtonWithTitle:@"清空历史"];
    [alert addButtonWithTitle:@"取消"];
    NSButton *deleteButton = alert.buttons.firstObject;
    deleteButton.hasDestructiveAction = YES;
    NSModalResponse response = [alert runModal];
    if (response == NSAlertFirstButtonReturn) {
        [self clearCurrentHistory];
    }
}

- (void)setMainStatus:(NSString *)text error:(BOOL)error {
    if (!_mainStatusLabel) return;
    SetTextFieldStringIfChanged(_mainStatusLabel, text ?: @"");
    SetTextFieldColorIfChanged(_mainStatusLabel, error ? NSColor.systemRedColor : NSColor.secondaryLabelColor);
}

- (BOOL)textFieldIsBeingEdited:(NSTextField *)field {
    if (!field || !field.window) return NO;
    id firstResponder = field.window.firstResponder;
    if (firstResponder == field) return YES;
    id currentEditor = field.currentEditor;
    if (currentEditor && firstResponder == currentEditor) return YES;
    if ([firstResponder isKindOfClass:NSTextView.class]) {
        NSTextView *editor = (NSTextView *)firstResponder;
        if ((id)editor.delegate == (id)field) return YES;
    }
    return NO;
}

- (void)setButton:(NSButton *)button enabled:(BOOL)enabled {
    if (!button) return;
    CGFloat alpha = enabled ? 1.0 : 0.42;
    if (button.enabled != enabled) {
        button.enabled = enabled;
    }
    if (fabs(button.alphaValue - alpha) > 0.001) {
        button.alphaValue = alpha;
    }
}

- (void)syncMainStatusWidgetButton {
    if (!_mainStatusWidgetButton) return;
    BOOL shown = [self isStatusSurfaceShown];
    NSString *title = shown ? @"关闭顶栏卡片" : @"打开顶栏卡片";
    NSString *symbol = shown ? @"xmark.circle" : @"menubar.rectangle";
    BOOL needsImage = !_mainStatusWidgetButton.image || ![_mainStatusWidgetButton.toolTip isEqualToString:title];
    SetButtonTitleIfChanged(_mainStatusWidgetButton, title);
    SetViewToolTipIfChanged(_mainStatusWidgetButton, title);
    if (needsImage) {
        NSImage *image = TemplateSystemSymbolImage(symbol, title);
        if (image) {
            _mainStatusWidgetButton.image = image;
            _mainStatusWidgetButton.imagePosition = NSImageLeft;
        }
    }
}

- (void)syncMainPageControls {
    if (!_mainUIDField) return;
    long long currentMID = [Settings mid];
    NSString *settingsName = [Settings name];
    BOOL autoRefreshEnabled = [Settings autoRefreshEnabled];
    BOOL notificationsEnabled = [Settings changeNotificationsEnabled];
    BOOL launchHidden = [Settings launchHidden];
    BOOL alwaysOnTop = [Settings alwaysOnTop];
    BOOL positionLocked = [Settings positionLocked];
    NSInteger appearance = [Settings appearanceMode];
    NSInteger trendRange = [Settings trendRange];
    NSInteger display = [Settings statusDisplayMode];
    NSTimeInterval interval = [Settings refreshInterval];

    if (![self textFieldIsBeingEdited:_mainUIDField]) {
        SetTextFieldStringIfChanged(_mainUIDField, [NSString stringWithFormat:@"%lld", currentMID]);
    }

    NSString *name = _card.name.length > 0 ? _card.name : settingsName;
    if (name.length == 0) {
        name = UnknownTargetName;
    }
    SetTextFieldStringIfChanged(_mainTargetLabel, [NSString stringWithFormat:@"当前监控：%@", name]);
    if (_card.hasFollowers) {
        NSString *followers = [self exactFollowersText];
        NSString *updated = _card.updatedText.length > 0 ? _card.updatedText : @"--";
        NSString *status = _card.status.length > 0 ? _card.status : @"实时统计";
        SetTextFieldStringIfChanged(_mainStatusLabel, [NSString stringWithFormat:@"%@ · %@ · %@", followers, status, updated]);
        SetTextFieldColorIfChanged(_mainStatusLabel, NSColor.secondaryLabelColor);
    } else {
        SetTextFieldStringIfChanged(_mainStatusLabel, _card.status.length > 0 ? _card.status : @"等待刷新");
        SetTextFieldColorIfChanged(_mainStatusLabel, NSColor.secondaryLabelColor);
    }
    NSString *scheduleText = [self refreshScheduleTextWithAutoRefreshEnabled:autoRefreshEnabled];
    [self syncMainScheduleLabelWithScheduleText:scheduleText autoRefreshEnabled:autoRefreshEnabled];

    for (NSSegmentedControl *control in @[_mainAppearanceControl, _mainTrendControl, _mainDisplayControl, _mainIntervalControl]) {
        if ([control isKindOfClass:StatusGlassSegmentedControl.class]) {
            ((StatusGlassSegmentedControl *)control).appearanceMode = appearance;
        }
    }

    SetSegmentedControlSelectedSegmentIfChanged(_mainAppearanceControl, appearance == AppearanceModeReadable
        ? 2
        : (appearance == AppearanceModeClear ? 1 : 0));

    SetSegmentedControlSelectedSegmentIfChanged(_mainTrendControl, trendRange == TrendRangeNinetyDays
        ? 2
        : (trendRange == TrendRangeThirtyDays ? 1 : 0));

    SetSegmentedControlSelectedSegmentIfChanged(_mainDisplayControl, display == StatusDisplayModeIconOnly
        ? 2
        : (display == StatusDisplayModeCompact ? 1 : 0));

    SetSegmentedControlSelectedSegmentIfChanged(_mainIntervalControl, interval > 450 ? 2 : (interval > 90 ? 1 : 0));
    SetControlEnabledIfChanged(_mainIntervalControl, YES);
    SetViewAlphaIfChanged(_mainIntervalControl, 1.0);
    SetViewToolTipIfChanged(_mainIntervalControl, autoRefreshEnabled
        ? @"选择自动刷新间隔"
        : @"自动刷新暂停中，重新开启后按此间隔刷新");

    SetButtonStateIfChanged(_mainAutoRefreshButton, autoRefreshEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    SetButtonStateIfChanged(_mainNotificationsButton, notificationsEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    NSString *launchAtLoginReason = [self launchAtLoginUnavailableReason];
    BOOL launchAtLoginSupported = launchAtLoginReason == nil;
    SetControlEnabledIfChanged(_mainLaunchAtLoginButton, launchAtLoginSupported);
    SetViewAlphaIfChanged(_mainLaunchAtLoginButton, launchAtLoginSupported ? 1.0 : 0.45);
    SetButtonStateIfChanged(_mainLaunchAtLoginButton, [self launchAtLoginEnabledIfSupported:launchAtLoginSupported] ? NSControlStateValueOn : NSControlStateValueOff);
    SetViewToolTipIfChanged(_mainLaunchAtLoginButton, launchAtLoginReason ?: @"登录后自动启动");
    SetButtonStateIfChanged(_mainMenuBarOnlyButton, launchHidden ? NSControlStateValueOn : NSControlStateValueOff);
    [self syncMainStatusWidgetButton];
    SetButtonStateIfChanged(_mainAlwaysOnTopButton, alwaysOnTop ? NSControlStateValueOn : NSControlStateValueOff);
    SetButtonStateIfChanged(_mainPositionLockedButton, positionLocked ? NSControlStateValueOn : NSControlStateValueOff);

    BOOL hasHistory = _historyPointCount > 0;
    BOOL hasWeekHistory = _weekPointCount > 0;
    if (_mainHistoryLabel) {
        if (_historyHasDelta) {
            SetTextFieldStringIfChanged(_mainHistoryLabel, _mainTrendStatusText ?: @"");
            SetTextFieldColorIfChanged(_mainHistoryLabel, FollowerDeltaColor(_historyDelta));
        } else if (_historyPointCount > 0) {
            SetTextFieldStringIfChanged(_mainHistoryLabel, _mainTrendStatusText ?: @"");
            SetTextFieldColorIfChanged(_mainHistoryLabel, NSColor.secondaryLabelColor);
        } else {
            SetTextFieldStringIfChanged(_mainHistoryLabel, _mainTrendStatusText ?: @"暂无历史点，刷新成功后自动记录。");
            SetTextFieldColorIfChanged(_mainHistoryLabel, NSColor.tertiaryLabelColor);
        }
    }
    [self setButton:_mainRefreshButton enabled:!_isRefreshing];
    SetButtonTitleIfChanged(_mainRefreshButton, _isRefreshing ? @"刷新中" : @"刷新");
    SetViewToolTipIfChanged(_mainRefreshButton, _isRefreshing ? @"正在刷新当前账号" : @"立即刷新当前账号");
    [self setButton:_mainCopyButton enabled:_card.hasFollowers];
    SetViewToolTipIfChanged(_mainCopyButton, _card.hasFollowers ? @"复制当前粉丝数" : @"刷新成功后可复制粉丝数");
    [self setButton:_mainProfileButton enabled:_card.mid > 0];
    SetViewToolTipIfChanged(_mainProfileButton, _card.mid > 0 ? @"打开当前 B站主页" : @"UID 有效后可打开主页");
    [self setButton:_mainCopyHistoryButton enabled:hasWeekHistory];
    SetViewToolTipIfChanged(_mainCopyHistoryButton, hasWeekHistory ? @"复制当前账号近 7 天 CSV" : @"近 7 天暂无可复制历史");
    BOOL hasTrend = _trendPointCount > 0;
    [self setButton:_mainCopyTrendButton enabled:hasTrend];
    SetViewToolTipIfChanged(_mainCopyTrendButton, _trendCSVTooltipText ?: @"趋势数据继续积累中");
    [self setButton:_mainClearHistoryButton enabled:hasHistory];
    SetViewToolTipIfChanged(_mainClearHistoryButton, hasHistory ? @"清空当前账号历史" : @"暂无可清空的历史");
}

- (long long)validatedMIDFromField:(NSTextField *)field statusTarget:(void (^)(NSString *message, BOOL error))statusTarget {
    NSString *raw = [field.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (raw.length == 0) {
        if (statusTarget) statusTarget(@"请输入 UID 或空间链接", YES);
        return 0;
    }
    long long mid = BilibiliMIDFromInput(raw);
    if (mid <= 0) {
        if (statusTarget) statusTarget(@"请粘贴 UID 或 B站链接", YES);
        return 0;
    }
    field.stringValue = [NSString stringWithFormat:@"%lld", mid];
    if (mid == [Settings mid]) {
        if (statusTarget) statusTarget(@"UID 未变化", NO);
        return 0;
    }
    return mid;
}

- (void)mainApplyUID:(id)sender {
    __weak typeof(self) weakSelf = self;
    long long mid = [self validatedMIDFromField:_mainUIDField statusTarget:^(NSString *message, BOOL error) {
        [weakSelf setMainStatus:message error:error];
    }];
    if (mid <= 0) return;
    [self applyMID:mid];
    [self setMainStatus:@"已切换，正在刷新" error:NO];
}

- (void)mainRefreshNow:(id)sender {
    if (_isRefreshing) {
        [self setMainStatus:@"正在刷新" error:NO];
        [self syncMainPageControls];
        return;
    }
    [self refresh:YES];
    [self setMainStatus:@"正在刷新" error:NO];
}

- (void)mainCopyFollowers:(id)sender {
    [self statusCopyFollowers:sender];
    [self syncMainPageControls];
}

- (void)mainOpenProfile:(id)sender {
    [self statusOpenProfile:sender];
}

- (void)mainCopyHistory:(id)sender {
    [self statusCopyHistory:sender];
    [self syncMainPageControls];
}

- (void)mainCopyTrend:(id)sender {
    [self statusCopyTrend:sender];
    [self syncMainPageControls];
}

- (void)mainClearHistory:(id)sender {
    [self confirmAndClearCurrentHistory];
    [self syncMainPageControls];
}

- (void)mainAppearanceChanged:(id)sender {
    NSInteger segment = _mainAppearanceControl.selectedSegment;
    [self setAppearanceMode:segment == 2 ? AppearanceModeReadable : (segment == 1 ? AppearanceModeClear : AppearanceModeUltraClear)];
    [self setMainStatus:@"透明度已更新" error:NO];
}

- (void)mainTrendChanged:(id)sender {
    NSInteger segment = _mainTrendControl.selectedSegment;
    NSInteger range = segment == 2 ? TrendRangeNinetyDays : (segment == 1 ? TrendRangeThirtyDays : TrendRangeSevenDays);
    [self setTrendRange:range];
    [self setMainStatus:[NSString stringWithFormat:@"已切换%@趋势", [self trendRangeTitle]] error:NO];
}

- (void)mainDisplayChanged:(id)sender {
    NSInteger segment = _mainDisplayControl.selectedSegment;
    [self setStatusDisplayMode:segment == 2 ? StatusDisplayModeIconOnly : (segment == 1 ? StatusDisplayModeCompact : StatusDisplayModeExact)];
    [self setMainStatus:@"顶栏显示已更新" error:NO];
}

- (void)mainIntervalChanged:(id)sender {
    NSInteger segment = _mainIntervalControl.selectedSegment;
    [self setRefreshInterval:segment == 2 ? 900 : (segment == 1 ? 300 : 60)];
    [self setMainStatus:@"刷新间隔已更新" error:NO];
}

- (void)mainAutoRefreshChanged:(id)sender {
    [self setAutoRefreshEnabled:_mainAutoRefreshButton.state == NSControlStateValueOn];
    [self setMainStatus:[Settings autoRefreshEnabled] ? @"自动刷新已开启" : @"自动刷新已暂停" error:NO];
}

- (void)mainNotificationsChanged:(id)sender {
    [self setChangeNotificationsEnabled:_mainNotificationsButton.state == NSControlStateValueOn];
    [self setMainStatus:[Settings changeNotificationsEnabled] ? @"变化提醒已开启" : @"变化提醒已关闭" error:NO];
}

- (void)mainLaunchAtLoginChanged:(id)sender {
    [self setLaunchAtLoginEnabled:_mainLaunchAtLoginButton.state == NSControlStateValueOn];
    NSString *unavailableReason = [self launchAtLoginUnavailableReason];
    if (unavailableReason) {
        [self setMainStatus:unavailableReason error:YES];
    } else {
        [self setMainStatus:[self launchAtLoginEnabled] ? @"登录启动已开启" : @"登录启动已关闭" error:NO];
    }
}

- (void)mainMenuBarOnlyChanged:(id)sender {
    [self setMenuBarOnlyEnabled:_mainMenuBarOnlyButton.state == NSControlStateValueOn];
    [self setMainStatus:[Settings launchHidden] ? @"仅顶栏模式已开启" : @"主窗口已显示" error:NO];
}

- (void)mainAlwaysOnTopChanged:(id)sender {
    [self setAlwaysOnTop:_mainAlwaysOnTopButton.state == NSControlStateValueOn];
    [self setMainStatus:[Settings alwaysOnTop] ? @"窗口已置顶" : @"窗口取消置顶" error:NO];
}

- (void)mainPositionLockedChanged:(id)sender {
    [self setPositionLocked:_mainPositionLockedButton.state == NSControlStateValueOn];
    [self setMainStatus:[Settings positionLocked] ? @"位置已锁定" : @"位置可拖动" error:NO];
}

- (void)mainResetPosition:(id)sender {
    [self resetWindowPosition];
    [self setMainStatus:@"已回到屏幕中央" error:NO];
}

- (void)mainOpenStatusWidget:(id)sender {
    BOOL wasShown = [self isStatusSurfaceShown];
    [self toggleStatusPopover];
    [self syncMainStatusWidgetButton];
    [self setMainStatus:wasShown ? @"顶栏卡片已关闭" : @"顶栏卡片已打开" error:NO];
}

- (void)mainHideWindow:(id)sender {
    [self hideCardWindow];
}

- (void)mainQuit:(id)sender {
    [NSApp terminate:nil];
}

- (NSTextField *)preferencesLabelWithString:(NSString *)string frame:(NSRect)frame font:(NSFont *)font color:(NSColor *)color {
    NSTextField *label = [NSTextField labelWithString:string ?: @""];
    label.frame = frame;
    label.font = font ?: [NSFont systemFontOfSize:13];
    label.textColor = color ?: NSColor.labelColor;
    return label;
}

- (NSButton *)preferencesButtonWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action {
    NSButton *button = [NSButton buttonWithTitle:title target:self action:action];
    button.frame = frame;
    button.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    [self styleGlassButton:button];
    return button;
}

- (NSButton *)preferencesCheckboxWithTitle:(NSString *)title frame:(NSRect)frame action:(SEL)action {
    NSButton *button = [NSButton checkboxWithTitle:title target:self action:action];
    button.frame = frame;
    button.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    [self styleGlassCheckbox:button];
    return button;
}

- (NSSegmentedControl *)preferencesSegmentedControlWithLabels:(NSArray<NSString *> *)labels frame:(NSRect)frame action:(SEL)action {
    StatusGlassSegmentedControl *control = [[StatusGlassSegmentedControl alloc] initWithFrame:frame];
    control.appearanceMode = [Settings appearanceMode];
    control.segmentCount = labels.count;
    control.trackingMode = NSSegmentSwitchTrackingSelectOne;
    control.target = self;
    control.action = action;
    [self styleGlassSegmentedControl:control];
    for (NSUInteger i = 0; i < labels.count; i++) {
        [control setLabel:labels[i] forSegment:i];
    }
    return control;
}

- (void)buildPreferencesWindowIfNeeded {
    if (_preferencesWindow) return;

    NSRect frame = NSMakeRect(0, 0, 480, 430);
    _preferencesWindow = [[NSWindow alloc] initWithContentRect:frame
                                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskFullSizeContentView
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
    _preferencesWindow.title = @"BILI粉丝数设置";
    _preferencesWindow.releasedWhenClosed = NO;
    _preferencesWindow.level = NSNormalWindowLevel;
    _preferencesWindow.delegate = self;

    NSVisualEffectView *root = [[NSVisualEffectView alloc] initWithFrame:frame];
    root.material = NSVisualEffectMaterialWindowBackground;
    root.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    root.state = NSVisualEffectStateActive;
    root.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _preferencesWindow.contentView = root;
    _preferencesEffectView = root;
    ApplyWindowChrome(_preferencesWindow);
    ApplyWindowChromeAfterSystemTitlebarUpdate(_preferencesWindow);

    _preferencesChromeView = [[PreferencesGlassChromeView alloc] initWithFrame:root.bounds];
    _preferencesChromeView.appearanceMode = [Settings appearanceMode];
    _preferencesChromeView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [root addSubview:_preferencesChromeView];

    [root addSubview:[self preferencesLabelWithString:@"BILI粉丝数设置"
                                                frame:NSMakeRect(28, 382, 260, 28)
                                                 font:[NSFont systemFontOfSize:20 weight:NSFontWeightSemibold]
                                                color:NSColor.labelColor]];
    [root addSubview:[self preferencesLabelWithString:@"主窗口和顶栏小组件共用这些设置。"
                                                frame:NSMakeRect(28, 360, 360, 20)
                                                 font:[NSFont systemFontOfSize:12 weight:NSFontWeightRegular]
                                                color:NSColor.secondaryLabelColor]];

    [root addSubview:[self preferencesLabelWithString:@"UID/链接"
                                                frame:NSMakeRect(32, 316, 92, 22)
                                                 font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                color:NSColor.secondaryLabelColor]];
    _preferencesUIDField = [[NSTextField alloc] initWithFrame:NSMakeRect(132, 312, 230, 28)];
    _preferencesUIDField.placeholderString = @"UID 或空间链接";
    _preferencesUIDField.toolTip = @"可输入 UID，或粘贴 B站空间、移动端、App 内打开等链接";
    _preferencesUIDField.font = [NSFont systemFontOfSize:13 weight:NSFontWeightMedium];
    _preferencesUIDField.alignment = NSTextAlignmentLeft;
    _preferencesUIDField.cell.usesSingleLineMode = YES;
    _preferencesUIDField.cell.lineBreakMode = NSLineBreakByClipping;
    _preferencesUIDField.target = self;
    _preferencesUIDField.action = @selector(preferencesApplyUID:);
    [self styleGlassTextField:_preferencesUIDField];
    [root addSubview:_preferencesUIDField];
    [root addSubview:[self preferencesButtonWithTitle:@"应用" frame:NSMakeRect(372, 312, 48, 28) action:@selector(preferencesApplyUID:)]];
    [root addSubview:[self preferencesButtonWithTitle:@"主页" frame:NSMakeRect(424, 312, 44, 28) action:@selector(preferencesOpenProfile:)]];

    [root addSubview:[self preferencesLabelWithString:@"透明度"
                                                frame:NSMakeRect(32, 270, 92, 22)
                                                 font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                color:NSColor.secondaryLabelColor]];
    _preferencesAppearanceControl = [self preferencesSegmentedControlWithLabels:@[@"极清透", @"清透", @"增强可读"]
                                                                          frame:NSMakeRect(132, 267, 214, 28)
                                                                         action:@selector(preferencesAppearanceChanged:)];
    [root addSubview:_preferencesAppearanceControl];

    [root addSubview:[self preferencesLabelWithString:@"顶栏显示"
                                                frame:NSMakeRect(32, 230, 92, 22)
                                                 font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                color:NSColor.secondaryLabelColor]];
    _preferencesDisplayControl = [self preferencesSegmentedControlWithLabels:@[@"精确数字", @"紧凑数字", @"仅图标"]
                                                                      frame:NSMakeRect(132, 227, 214, 28)
                                                                     action:@selector(preferencesDisplayChanged:)];
    [root addSubview:_preferencesDisplayControl];

    [root addSubview:[self preferencesLabelWithString:@"刷新间隔"
                                                frame:NSMakeRect(32, 190, 92, 22)
                                                 font:[NSFont systemFontOfSize:13 weight:NSFontWeightMedium]
                                                color:NSColor.secondaryLabelColor]];
    _preferencesIntervalControl = [self preferencesSegmentedControlWithLabels:@[@"1 分钟", @"5 分钟", @"15 分钟"]
                                                                       frame:NSMakeRect(132, 187, 214, 28)
                                                                      action:@selector(preferencesIntervalChanged:)];
    [root addSubview:_preferencesIntervalControl];

    _preferencesAutoRefreshButton = [self preferencesCheckboxWithTitle:@"自动刷新" frame:NSMakeRect(132, 147, 102, 24) action:@selector(preferencesAutoRefreshChanged:)];
    _preferencesNotificationsButton = [self preferencesCheckboxWithTitle:@"粉丝变化提醒" frame:NSMakeRect(258, 147, 132, 24) action:@selector(preferencesNotificationsChanged:)];
    _preferencesLaunchAtLoginButton = [self preferencesCheckboxWithTitle:@"登录时启动" frame:NSMakeRect(132, 117, 112, 24) action:@selector(preferencesLaunchAtLoginChanged:)];
    _preferencesMenuBarOnlyButton = [self preferencesCheckboxWithTitle:@"仅顶栏模式" frame:NSMakeRect(258, 117, 122, 24) action:@selector(preferencesMenuBarOnlyChanged:)];
    _preferencesAlwaysOnTopButton = [self preferencesCheckboxWithTitle:@"置顶显示" frame:NSMakeRect(132, 87, 102, 24) action:@selector(preferencesAlwaysOnTopChanged:)];
    _preferencesPositionLockedButton = [self preferencesCheckboxWithTitle:@"锁定位置" frame:NSMakeRect(258, 87, 102, 24) action:@selector(preferencesPositionLockedChanged:)];
    [root addSubview:_preferencesAutoRefreshButton];
    [root addSubview:_preferencesNotificationsButton];
    [root addSubview:_preferencesLaunchAtLoginButton];
    [root addSubview:_preferencesMenuBarOnlyButton];
    [root addSubview:_preferencesAlwaysOnTopButton];
    [root addSubview:_preferencesPositionLockedButton];

    [root addSubview:[self preferencesButtonWithTitle:@"回到屏幕中央" frame:NSMakeRect(132, 48, 116, 28) action:@selector(preferencesResetPosition:)]];
    _preferencesRefreshButton = [self preferencesButtonWithTitle:@"立即刷新" frame:NSMakeRect(246, 48, 92, 28) action:@selector(preferencesRefreshNow:)];
    [root addSubview:_preferencesRefreshButton];
    [root addSubview:[self preferencesButtonWithTitle:@"完成" frame:NSMakeRect(378, 48, 74, 28) action:@selector(preferencesClose:)]];

    _preferencesStatusLabel = [self preferencesLabelWithString:@""
                                                         frame:NSMakeRect(132, 20, 320, 20)
                                                          font:[NSFont systemFontOfSize:12 weight:NSFontWeightRegular]
                                                         color:NSColor.secondaryLabelColor];
    [root addSubview:_preferencesStatusLabel];
    [_preferencesWindow center];
}

- (void)setPreferencesStatus:(NSString *)text error:(BOOL)error {
    if (!_preferencesStatusLabel) return;
    SetTextFieldStringIfChanged(_preferencesStatusLabel, text ?: @"");
    SetTextFieldColorIfChanged(_preferencesStatusLabel, error ? NSColor.systemRedColor : NSColor.secondaryLabelColor);
}

- (void)syncPreferencesWindowControls {
    if (!_preferencesWindow) return;
    long long currentMID = [Settings mid];
    NSString *settingsName = [Settings name];
    BOOL autoRefreshEnabled = [Settings autoRefreshEnabled];
    BOOL notificationsEnabled = [Settings changeNotificationsEnabled];
    BOOL launchHidden = [Settings launchHidden];
    BOOL alwaysOnTop = [Settings alwaysOnTop];
    BOOL positionLocked = [Settings positionLocked];
    NSInteger appearance = [Settings appearanceMode];
    NSInteger display = [Settings statusDisplayMode];
    NSTimeInterval interval = [Settings refreshInterval];

    if (![self textFieldIsBeingEdited:_preferencesUIDField]) {
        SetTextFieldStringIfChanged(_preferencesUIDField, [NSString stringWithFormat:@"%lld", currentMID]);
    }

    SetSegmentedControlSelectedSegmentIfChanged(_preferencesAppearanceControl, appearance == AppearanceModeReadable
        ? 2
        : (appearance == AppearanceModeClear ? 1 : 0));

    SetSegmentedControlSelectedSegmentIfChanged(_preferencesDisplayControl, display == StatusDisplayModeIconOnly
        ? 2
        : (display == StatusDisplayModeCompact ? 1 : 0));

    SetSegmentedControlSelectedSegmentIfChanged(_preferencesIntervalControl, interval > 450 ? 2 : (interval > 90 ? 1 : 0));
    SetControlEnabledIfChanged(_preferencesIntervalControl, YES);
    SetViewAlphaIfChanged(_preferencesIntervalControl, 1.0);
    SetViewToolTipIfChanged(_preferencesIntervalControl, autoRefreshEnabled
        ? @"选择自动刷新间隔"
        : @"自动刷新暂停中，重新开启后按此间隔刷新");

    SetButtonStateIfChanged(_preferencesAutoRefreshButton, autoRefreshEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    SetButtonStateIfChanged(_preferencesNotificationsButton, notificationsEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    NSString *launchAtLoginReason = [self launchAtLoginUnavailableReason];
    BOOL launchAtLoginSupported = launchAtLoginReason == nil;
    SetControlEnabledIfChanged(_preferencesLaunchAtLoginButton, launchAtLoginSupported);
    SetViewAlphaIfChanged(_preferencesLaunchAtLoginButton, launchAtLoginSupported ? 1.0 : 0.45);
    SetButtonStateIfChanged(_preferencesLaunchAtLoginButton, [self launchAtLoginEnabledIfSupported:launchAtLoginSupported] ? NSControlStateValueOn : NSControlStateValueOff);
    SetViewToolTipIfChanged(_preferencesLaunchAtLoginButton, launchAtLoginReason ?: @"登录后自动启动");
    SetButtonStateIfChanged(_preferencesMenuBarOnlyButton, launchHidden ? NSControlStateValueOn : NSControlStateValueOff);
    SetButtonStateIfChanged(_preferencesAlwaysOnTopButton, alwaysOnTop ? NSControlStateValueOn : NSControlStateValueOff);
    SetButtonStateIfChanged(_preferencesPositionLockedButton, positionLocked ? NSControlStateValueOn : NSControlStateValueOff);
    [self setButton:_preferencesRefreshButton enabled:!_isRefreshing];
    SetButtonTitleIfChanged(_preferencesRefreshButton, _isRefreshing ? @"刷新中" : @"立即刷新");

    NSString *name = settingsName.length > 0 ? settingsName : UnknownTargetName;
    if (launchAtLoginReason) {
        [self setPreferencesStatus:launchAtLoginReason error:NO];
    } else {
        [self setPreferencesStatus:[NSString stringWithFormat:@"当前 %@ · UID %lld", name, currentMID] error:NO];
    }
}

- (void)syncPreferencesWindowControlsIfVisible {
    if (_preferencesWindow.isVisible) {
        [self syncPreferencesWindowControls];
    }
}

- (void)showPreferencesWindow {
    [NSApp activateIgnoringOtherApps:YES];
    [self buildPreferencesWindowIfNeeded];
    [self syncPreferencesWindowControls];
    [_preferencesWindow makeKeyAndOrderFront:nil];
}

- (void)showUIDSettings {
    [self showPreferencesWindow];
}

- (void)preferencesApplyUID:(id)sender {
    __weak typeof(self) weakSelf = self;
    long long mid = [self validatedMIDFromField:_preferencesUIDField statusTarget:^(NSString *message, BOOL error) {
        [weakSelf setPreferencesStatus:message error:error];
    }];
    if (mid <= 0) return;
    [self applyMID:mid];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:@"已切换，正在刷新" error:NO];
}

- (void)preferencesOpenProfile:(id)sender {
    [self statusOpenProfile:sender];
    [self setPreferencesStatus:_card.status error:[_card.status isEqualToString:@"主页打开失败"] || [_card.status isEqualToString:@"UID 无效"]];
}

- (void)preferencesAppearanceChanged:(id)sender {
    NSInteger segment = _preferencesAppearanceControl.selectedSegment;
    [self setAppearanceMode:segment == 2 ? AppearanceModeReadable : (segment == 1 ? AppearanceModeClear : AppearanceModeUltraClear)];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:@"透明度已更新" error:NO];
}

- (void)preferencesDisplayChanged:(id)sender {
    NSInteger segment = _preferencesDisplayControl.selectedSegment;
    [self setStatusDisplayMode:segment == 2 ? StatusDisplayModeIconOnly : (segment == 1 ? StatusDisplayModeCompact : StatusDisplayModeExact)];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:@"顶栏显示已更新" error:NO];
}

- (void)preferencesIntervalChanged:(id)sender {
    NSInteger segment = _preferencesIntervalControl.selectedSegment;
    [self setRefreshInterval:segment == 2 ? 900 : (segment == 1 ? 300 : 60)];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:@"刷新间隔已更新" error:NO];
}

- (void)preferencesAutoRefreshChanged:(id)sender {
    [self setAutoRefreshEnabled:_preferencesAutoRefreshButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:[Settings autoRefreshEnabled] ? @"自动刷新已开启" : @"自动刷新已暂停" error:NO];
}

- (void)preferencesNotificationsChanged:(id)sender {
    [self setChangeNotificationsEnabled:_preferencesNotificationsButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:[Settings changeNotificationsEnabled] ? @"变化提醒已开启" : @"变化提醒已关闭" error:NO];
}

- (void)preferencesLaunchAtLoginChanged:(id)sender {
    [self setLaunchAtLoginEnabled:_preferencesLaunchAtLoginButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    NSString *unavailableReason = [self launchAtLoginUnavailableReason];
    if (unavailableReason) {
        [self setPreferencesStatus:unavailableReason error:YES];
    } else {
        [self setPreferencesStatus:[self launchAtLoginEnabled] ? @"登录启动已开启" : @"登录启动已关闭" error:NO];
    }
}

- (void)preferencesMenuBarOnlyChanged:(id)sender {
    [self setMenuBarOnlyEnabled:_preferencesMenuBarOnlyButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:[Settings launchHidden] ? @"仅顶栏模式已开启" : @"主窗口已显示" error:NO];
}

- (void)preferencesAlwaysOnTopChanged:(id)sender {
    [self setAlwaysOnTop:_preferencesAlwaysOnTopButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:[Settings alwaysOnTop] ? @"窗口已置顶" : @"窗口取消置顶" error:NO];
}

- (void)preferencesPositionLockedChanged:(id)sender {
    [self setPositionLocked:_preferencesPositionLockedButton.state == NSControlStateValueOn];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:[Settings positionLocked] ? @"位置已锁定" : @"位置可拖动" error:NO];
}

- (void)preferencesResetPosition:(id)sender {
    [self resetWindowPosition];
    [self syncPreferencesWindowControls];
    [self setPreferencesStatus:@"已回到屏幕中央" error:NO];
}

- (void)preferencesRefreshNow:(id)sender {
    if (_isRefreshing) {
        [self setPreferencesStatus:@"正在刷新" error:NO];
        [self syncPreferencesWindowControls];
        return;
    }
    [self refresh:YES];
    [self setPreferencesStatus:@"正在刷新" error:NO];
}

- (void)preferencesClose:(id)sender {
    [_preferencesWindow orderOut:nil];
}

- (void)applyMID:(long long)mid {
    [Settings setMid:mid];
    _consecutiveRefreshFailures = 0;
    FanResult *cached = [Settings latestResultForCurrentTarget];
    [Settings setName:cached.name.length > 0 ? cached.name : UnknownTargetName];
    _card.mid = mid;
    _card.name = [Settings name];
    _card.hasFollowerDelta = NO;
    [self updateHistoryValues];
    if (cached) {
        [self applyResultToCard:cached status:@"上次统计"];
        _card.status = @"正在刷新 · 保留上次";
    } else {
        _card.hasFollowers = NO;
        _card.updatedText = @"--";
        _card.status = @"正在刷新";
        [self updateStatusItemTitle];
    }
    [self refresh:YES];
}

- (void)showCachedResultIfAvailable {
    FanResult *cached = [Settings latestResultForCurrentTarget];
    if (!cached) return;
    _card.hasFollowerDelta = NO;
    [self updateHistoryValues];
    [self applyResultToCard:cached status:@"上次统计"];
}

- (NSTimeInterval)autoRefreshDelayAfterFailure {
    if (_consecutiveRefreshFailures == 0) {
        return [Settings refreshInterval];
    }
    NSUInteger exponent = MIN(_consecutiveRefreshFailures - 1, (NSUInteger)3);
    NSTimeInterval retryDelay = 90.0 * pow(2.0, (double)exponent);
    retryDelay = MIN(retryDelay, 600.0);
    return MIN([Settings refreshInterval], MAX(60.0, retryDelay));
}

- (void)startAutoRefreshAfterDelay:(NSTimeInterval)delay {
    [_timer invalidate];
    _timer = nil;
    if (![Settings autoRefreshEnabled]) {
        _nextAutoRefreshAt = nil;
        [self updateStatusItemTitle];
        [self restartDisplayRefreshTickerForVisibleSurfaces];
        return;
    }
    NSTimeInterval effectiveDelay = MAX(60.0, delay);
    _nextAutoRefreshAt = [NSDate dateWithTimeIntervalSinceNow:effectiveDelay];
    __weak typeof(self) weakSelf = self;
    _timer = [NSTimer timerWithTimeInterval:effectiveDelay repeats:NO block:^(__unused NSTimer *timer) {
        AppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        strongSelf->_timer = nil;
        strongSelf->_nextAutoRefreshAt = nil;
        [strongSelf refresh:NO];
        [strongSelf updateTimeSensitiveStatusText];
    }];
    _timer.tolerance = MIN(30, effectiveDelay * 0.15);
    [[NSRunLoop mainRunLoop] addTimer:_timer forMode:NSRunLoopCommonModes];
    [self updateStatusItemTitle];
    [self restartDisplayRefreshTickerForVisibleSurfaces];
}

- (void)startAutoRefresh {
    [self startAutoRefreshAfterDelay:[Settings refreshInterval]];
}

- (NSTimeInterval)nextDisplayRefreshTickInterval {
    if (!_nextAutoRefreshAt) {
        return 30;
    }
    NSTimeInterval remaining = [_nextAutoRefreshAt timeIntervalSinceNow];
    if (remaining <= 60) {
        return 1;
    }
    CGFloat currentMinuteLabel = ceil(remaining / 60.0);
    NSTimeInterval nextMinuteBoundary = MAX(60, (currentMinuteLabel - 1) * 60.0);
    NSTimeInterval interval = remaining - nextMinuteBoundary + 0.08;
    return MIN(60, MAX(1, interval));
}

- (NSTimeInterval)displayRefreshToleranceForInterval:(NSTimeInterval)interval {
    if (interval <= 1.5) {
        return 0.12;
    }
    return MIN(10, interval * 0.2);
}

- (void)startDisplayRefreshTicker {
    if ((![self isStatusSurfaceShown] && !_window.isVisible) || ![Settings autoRefreshEnabled]) {
        return;
    }
    if (_displayTimer) {
        return;
    }
    NSTimeInterval interval = [self nextDisplayRefreshTickInterval];
    __weak typeof(self) weakSelf = self;
    _displayTimer = [NSTimer timerWithTimeInterval:interval repeats:NO block:^(__unused NSTimer *timer) {
        AppDelegate *strongSelf = weakSelf;
        if (!strongSelf) return;
        strongSelf->_displayTimer = nil;
        if (![Settings autoRefreshEnabled] || (![strongSelf isStatusSurfaceShown] && !strongSelf->_window.isVisible)) {
            [strongSelf syncDisplayRefreshTickerForVisibleSurfaces];
            return;
        }
        [strongSelf updateTimeSensitiveStatusText];
        [strongSelf startDisplayRefreshTicker];
    }];
    _displayTimer.tolerance = [self displayRefreshToleranceForInterval:interval];
    [[NSRunLoop mainRunLoop] addTimer:_displayTimer forMode:NSRunLoopCommonModes];
}

- (void)stopDisplayRefreshTicker {
    [_displayTimer invalidate];
    _displayTimer = nil;
}

- (void)syncDisplayRefreshTickerForVisibleSurfaces {
    if ([Settings autoRefreshEnabled] && ([self isStatusSurfaceShown] || _window.isVisible)) {
        [self startDisplayRefreshTicker];
    } else {
        [self stopDisplayRefreshTicker];
    }
}

- (void)restartDisplayRefreshTickerForVisibleSurfaces {
    [self stopDisplayRefreshTicker];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (NSString *)refreshScheduleText {
    return [self refreshScheduleTextWithAutoRefreshEnabled:[Settings autoRefreshEnabled]];
}

- (NSString *)refreshScheduleTextWithAutoRefreshEnabled:(BOOL)autoRefreshEnabled {
    if (_isRefreshing) {
        return @"正在刷新";
    }
    if (!autoRefreshEnabled) {
        return @"已暂停";
    }
    if (!_nextAutoRefreshAt) {
        return @"等待计时";
    }
    NSTimeInterval remaining = [_nextAutoRefreshAt timeIntervalSinceNow];
    if (remaining <= 1) {
        return @"即将刷新";
    }
    NSString *prefix = _consecutiveRefreshFailures > 0 ? @"重试" : @"下次";
    if (remaining < 60) {
        return [NSString stringWithFormat:@"%@ %.0f秒", prefix, ceil(remaining)];
    }
    return [NSString stringWithFormat:@"%@ %.0f分钟", prefix, ceil(remaining / 60.0)];
}

- (NSString *)updatedDisplayTextForDate:(NSDate *)date {
    if (!date) {
        return @"--";
    }
    if (!_formatter) {
        _formatter = [NSDateFormatter new];
        _formatter.dateFormat = @"HH:mm";
    }
    NSString *timeText = [_formatter stringFromDate:date] ?: @"--";
    NSCalendar *calendar = NSCalendar.currentCalendar;
    if ([calendar isDateInToday:date]) {
        return [NSString stringWithFormat:@"%@ 更新", timeText];
    }
    if ([calendar isDateInYesterday:date]) {
        return [NSString stringWithFormat:@"昨天 %@ 更新", timeText];
    }

    NSDateComponents *now = [calendar components:NSCalendarUnitYear fromDate:[NSDate date]];
    NSDateComponents *then = [calendar components:NSCalendarUnitYear fromDate:date];
    NSString *format = now.year == then.year ? @"M月d日 HH:mm" : @"yyyy年M月d日 HH:mm";
    NSDateFormatter *dateFormatter = ThreadLocalDateFormatterWithLocale([@"BILIFansUpdatedDisplayFormatter." stringByAppendingString:format],
                                                                        format,
                                                                        @"zh_CN");
    return [NSString stringWithFormat:@"%@ 更新", [dateFormatter stringFromDate:date] ?: timeText];
}

- (void)applyResultToCard:(FanResult *)result status:(NSString *)status {
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    _stableCardStatus = [(status.length > 0 ? status : @"实时统计") copy];
    _card.mid = result.mid;
    _card.name = result.name;
    _card.followers = result.followers;
    _card.hasFollowers = YES;
    _card.updatedText = [self updatedDisplayTextForDate:result.updatedAt];
    _card.status = _stableCardStatus;
    _card.needsDisplay = YES;
    [self updateStatusItemTitle];
}

- (void)refresh:(BOOL)manual {
    long long mid = [Settings mid];
    if (_isRefreshing) {
        _card.refreshing = YES;
        if (_activeRefreshMID != mid) {
            _pendingRefreshAfterCurrent = YES;
            _pendingRefreshManual = _pendingRefreshManual || manual;
            _card.status = @"切换后刷新";
        } else if (manual) {
            _card.status = @"正在刷新";
        }
        _card.needsDisplay = YES;
        [self syncStatusPopoverIfVisible];
        return;
    }
    _isRefreshing = YES;
    _card.refreshing = YES;
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    _activeRefreshMID = mid;
    NSString *fallbackName = [Settings name];
    _card.status = manual ? @"正在刷新" : @"自动刷新";
    [self updateStatusItemTitle];
    [BiliFansClient fetchMID:mid fallbackName:fallbackName completion:^(FanResult *result, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            _isRefreshing = NO;
            _card.refreshing = NO;
            _activeRefreshMID = 0;
            BOOL targetChanged = mid != [Settings mid];
            BOOL refreshFailedForCurrentTarget = NO;
            if (result) {
                if (targetChanged || result.mid != [Settings mid]) {
                    _card.status = @"已切换账号";
                } else {
                    _consecutiveRefreshFailures = 0;
                    FanResult *previous = [Settings latestResultForCurrentTarget];
                    if (previous && previous.mid == result.mid && previous.followers != result.followers) {
                        _card.followerDelta = result.followers - previous.followers;
                        _card.hasFollowerDelta = YES;
                        [self notifyFollowerChangeForResult:result delta:_card.followerDelta];
                    } else {
                        _card.hasFollowerDelta = NO;
                    }
                    [Settings setMid:result.mid];
                    [Settings setName:result.name];
                    [Settings saveLatestResult:result];
                    [Settings saveHistoryPoint:result];
                    [self updateHistoryValues];
                    [self applyResultToCard:result status:@"实时统计"];
                }
            } else if (!targetChanged) {
                refreshFailedForCurrentTarget = YES;
                _consecutiveRefreshFailures = MIN(_consecutiveRefreshFailures + 1, (NSUInteger)8);
                FanResult *cached = [Settings latestResultForCurrentTarget];
                NSString *failureStatus = StatusWithRefreshError(_card.hasFollowers || cached ? @"保留上次" : @"刷新失败", error);
                _card.hasFollowerDelta = NO;
                if (cached) {
                    [self applyResultToCard:cached status:failureStatus];
                } else {
                    [self setStableCardStatus:failureStatus];
                    _card.updatedText = _card.hasFollowers ? _card.updatedText : @"--";
                }
            } else {
                _card.status = @"已切换账号";
            }
            _card.needsDisplay = YES;
            BOOL shouldRunPendingRefresh = _pendingRefreshAfterCurrent;
            BOOL pendingManual = _pendingRefreshManual;
            _pendingRefreshAfterCurrent = NO;
            _pendingRefreshManual = NO;
            if (shouldRunPendingRefresh) {
                [self refresh:pendingManual];
            } else {
                if ([Settings autoRefreshEnabled]) {
                    [self startAutoRefreshAfterDelay:refreshFailedForCurrentTarget
                        ? [self autoRefreshDelayAfterFailure]
                        : [Settings refreshInterval]];
                } else {
                    [self updateStatusItemTitle];
                }
            }
        });
    }];
}
@end
