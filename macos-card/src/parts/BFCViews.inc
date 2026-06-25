// Liquid-glass AppKit views for the card, dashboard, preferences, and status panel.
@interface LiquidCardView : NSView
@property(nonatomic, copy) NSString *name;
@property(nonatomic) long long mid;
@property(nonatomic) NSInteger followers;
@property(nonatomic) BOOL hasFollowers;
@property(nonatomic) NSInteger followerDelta;
@property(nonatomic) BOOL hasFollowerDelta;
@property(nonatomic, copy) NSArray<NSNumber *> *historyValues;
@property(nonatomic, copy) NSString *status;
@property(nonatomic, copy) NSString *updatedText;
@property(nonatomic) NSInteger appearanceMode;
@property(nonatomic) NSTimeInterval refreshInterval;
@property(nonatomic) BOOL alwaysOnTop;
@property(nonatomic) BOOL positionLocked;
@property(nonatomic) BOOL showsInlineControls;
@property(nonatomic) BOOL refreshing;
@property(nonatomic, copy) void (^refreshHandler)(void);
@property(nonatomic, copy) void (^settingsHandler)(void);
@property(nonatomic, copy) void (^appearanceHandler)(NSInteger mode);
@property(nonatomic, copy) void (^refreshIntervalHandler)(NSTimeInterval interval);
@property(nonatomic, copy) void (^alwaysOnTopHandler)(BOOL enabled);
@property(nonatomic, copy) void (^positionLockedHandler)(BOOL locked);
@property(nonatomic, copy) void (^resetPositionHandler)(void);
@property(nonatomic, copy) void (^copyHistoryHandler)(void);
@property(nonatomic, copy) void (^clearHistoryHandler)(void);
@property(nonatomic, copy) void (^hideHandler)(void);
@end

@implementation LiquidCardView {
    NSButton *_refreshButton;
    NSButton *_closeButton;
    NSTrackingArea *_trackingArea;
    BOOL _hovering;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.wantsLayer = YES;
    self.name = [Settings name];
    self.mid = [Settings mid];
    self.appearanceMode = [Settings appearanceMode];
    self.refreshInterval = [Settings refreshInterval];
    self.alwaysOnTop = [Settings alwaysOnTop];
    self.positionLocked = [Settings positionLocked];
    self.showsInlineControls = YES;
    self.historyValues = @[];
    self.status = @"等待刷新";
    self.updatedText = @"--";

    _refreshButton = [NSButton buttonWithImage:[self refreshImage] target:self action:@selector(refresh)];
    _refreshButton.bezelStyle = NSBezelStyleInline;
    _refreshButton.bordered = NO;
    _refreshButton.imagePosition = NSImageOnly;
    _refreshButton.contentTintColor = NSColor.secondaryLabelColor;
    _refreshButton.toolTip = @"刷新";
    [self addSubview:_refreshButton];

    _closeButton = [NSButton buttonWithImage:[self exitImage] target:self action:@selector(quitApp)];
    _closeButton.bezelStyle = NSBezelStyleInline;
    _closeButton.bordered = NO;
    _closeButton.imagePosition = NSImageOnly;
    _closeButton.contentTintColor = NSColor.systemRedColor;
    _closeButton.toolTip = @"退出 BILI粉丝数";
    [self addSubview:_closeButton];
    [self updateControlVisibility];
    return self;
}

- (void)layout {
    [super layout];
    _closeButton.frame = NSMakeRect(self.bounds.size.width - 46, self.bounds.size.height - 47, 28, 28);
    _refreshButton.frame = NSMakeRect(self.bounds.size.width - 82, self.bounds.size.height - 49, 32, 32);
}

- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
    }
    _trackingArea = [[NSTrackingArea alloc] initWithRect:self.bounds
                                                 options:NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways | NSTrackingInVisibleRect
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)mouseEntered:(NSEvent *)event {
    _hovering = YES;
    [self updateControlVisibility];
}

- (void)mouseExited:(NSEvent *)event {
    _hovering = NO;
    [self updateControlVisibility];
}

- (void)mouseUp:(NSEvent *)event {
    if (event.clickCount == 2 && !self.refreshing) {
        [self refresh];
        return;
    }
    [super mouseUp:event];
}

- (void)updateControlVisibility {
    if (!self.showsInlineControls) {
        SetViewHiddenIfChanged(_refreshButton, YES);
        SetViewHiddenIfChanged(_closeButton, YES);
        return;
    }
    SetViewHiddenIfChanged(_refreshButton, NO);
    SetViewHiddenIfChanged(_closeButton, NO);
    SetControlEnabledIfChanged(_refreshButton, !self.refreshing);
    SetViewAlphaIfChanged(_refreshButton, self.refreshing ? 0.26 : (_hovering ? 0.92 : 0.36));
    SetViewAlphaIfChanged(_closeButton, _hovering ? 0.98 : 0.64);
}

- (void)setName:(NSString *)name {
    NSString *next = [name copy] ?: @"";
    if ([_name isEqualToString:next]) return;
    _name = next;
    self.needsDisplay = YES;
}
- (void)setStatus:(NSString *)status {
    NSString *next = [status copy] ?: @"";
    if ([_status isEqualToString:next]) return;
    _status = next;
    self.needsDisplay = YES;
}
- (void)setUpdatedText:(NSString *)updatedText {
    NSString *next = [updatedText copy] ?: @"";
    if ([_updatedText isEqualToString:next]) return;
    _updatedText = next;
    self.needsDisplay = YES;
}
- (void)setAppearanceMode:(NSInteger)appearanceMode {
    NSInteger next = appearanceMode == AppearanceModeReadable
        ? AppearanceModeReadable
        : (appearanceMode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    if (_appearanceMode == next) return;
    _appearanceMode = next;
    self.needsDisplay = YES;
}
- (void)setFollowers:(NSInteger)followers {
    if (_followers == followers) return;
    _followers = followers;
    self.needsDisplay = YES;
}
- (void)setHasFollowers:(BOOL)hasFollowers {
    if (_hasFollowers == hasFollowers) return;
    _hasFollowers = hasFollowers;
    self.needsDisplay = YES;
}
- (void)setFollowerDelta:(NSInteger)followerDelta {
    if (_followerDelta == followerDelta) return;
    _followerDelta = followerDelta;
    self.needsDisplay = YES;
}
- (void)setHasFollowerDelta:(BOOL)hasFollowerDelta {
    if (_hasFollowerDelta == hasFollowerDelta) return;
    _hasFollowerDelta = hasFollowerDelta;
    self.needsDisplay = YES;
}
- (void)setHistoryValues:(NSArray<NSNumber *> *)historyValues {
    if (_historyValues == historyValues) return;
    NSArray<NSNumber *> *next = [historyValues copy] ?: @[];
    if ([_historyValues isEqualToArray:next]) return;
    _historyValues = next;
    self.needsDisplay = YES;
}
- (void)setMid:(long long)mid {
    if (_mid == mid) return;
    _mid = mid;
}
- (void)setShowsInlineControls:(BOOL)showsInlineControls {
    if (_showsInlineControls == showsInlineControls) return;
    _showsInlineControls = showsInlineControls;
    [self updateControlVisibility];
}

- (void)setRefreshing:(BOOL)refreshing {
    if (_refreshing == refreshing) return;
    _refreshing = refreshing;
    _refreshButton.toolTip = refreshing ? @"正在刷新" : @"刷新";
    [self updateControlVisibility];
    self.needsDisplay = YES;
}

- (NSMenu *)menuForEvent:(NSEvent *)event {
    if (!self.showsInlineControls) {
        return nil;
    }
    NSMenu *menu = [NSMenu new];
    [menu addItem:[self menuItemWithTitle:@"刷新" action:@selector(refresh)]];
    NSMenuItem *copyItem = [self menuItemWithTitle:@"复制粉丝数" action:@selector(copyFollowers)];
    copyItem.enabled = self.hasFollowers;
    [menu addItem:copyItem];
    [menu addItem:[self menuItemWithTitle:@"打开 B站主页" action:@selector(openBilibiliProfile)]];
    [menu addItem:[self menuItemWithTitle:@"设置..." action:@selector(openSettings)]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItemWithTitle:@"隐藏主窗口" action:@selector(hideCard)]];
    [menu addItem:[self menuItemWithTitle:@"退出 BILI粉丝数" action:@selector(quitApp)]];
    return menu;
}

- (NSMenuItem *)menuItemWithTitle:(NSString *)title action:(SEL)action {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:@""];
    item.target = self;
    return item;
}

- (NSMenuItem *)checkedMenuItemWithTitle:(NSString *)title action:(SEL)action checked:(BOOL)checked {
    NSMenuItem *item = [self menuItemWithTitle:title action:action];
    SetMenuItemStateIfChanged(item, checked ? NSControlStateValueOn : NSControlStateValueOff);
    return item;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(refresh)) {
        SetMenuItemTitleIfChanged(item, self.refreshing ? @"正在刷新" : @"刷新");
        return !self.refreshing;
    }
    if (item.action == @selector(copyFollowers)) {
        return self.hasFollowers;
    }
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    NSRect rect = NSInsetRect(self.bounds, 2, 2);
    NSBezierPath *clip = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:24 yRadius:24];
    DrawFloatingGlassShadow(rect, 24, self.appearanceMode, 1.10);
    [NSGraphicsContext saveGraphicsState];
    [clip addClip];

    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    [[[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.10 : (clear ? 0.020 : 0.0))] setFill];
    [clip fill];
    [self drawGlassBodyInRect:rect];
    [NSGraphicsContext restoreGraphicsState];

    [self drawSoftEdgesInRect:rect shape:clip];
    NSRect trendRect = NSMakeRect(24, 19, self.bounds.size.width - 116, 20);
    [self drawHistorySparklineInRect:trendRect];
    [self drawTrendSummaryInRect:NSMakeRect(NSMaxX(trendRect) + 8, 20, 60, 18)];
    [self drawLabels];
}

- (void)drawGlassBodyInRect:(NSRect)rect {
    DrawLiquidGlassOpticalOverlay(rect, 24, NSColor.controlAccentColor, self.appearanceMode, AppAppearanceIsDark() ? 1.0 : 0.92);
}

- (void)drawSoftEdgesInRect:(NSRect)rect shape:(NSBezierPath *)shape {
    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL dark = AppAppearanceIsDark();
    (void)shape;
    CGFloat strength = dark
        ? (readable ? 0.34 : (clear ? 0.20 : 0.12))
        : (readable ? 0.72 : (clear ? 0.56 : 0.44));
    DrawLiquidGlassRim(rect, 24, NSColor.controlAccentColor, strength);
}

- (void)drawHistorySparklineInRect:(NSRect)rect {
    DrawFollowerSparkline(self.historyValues, rect, 4.0, 2.2, 0.72);
}

- (void)drawTrendSummaryInRect:(NSRect)rect {
    if (self.historyValues.count < 2) return;
    NSInteger first = self.historyValues.firstObject.integerValue;
    NSInteger last = self.historyValues.lastObject.integerValue;
    NSInteger delta = last - first;
    NSString *text = CompactSignedDelta(delta);
    NSDictionary *attrs = @{NSFontAttributeName: CachedMonospacedDigitFont(11, NSFontWeightSemibold),
                            NSForegroundColorAttributeName: FollowerDeltaColor(delta),
                            NSShadowAttributeName: [self shadowWithAlpha:0.14 blur:1.5 y:-1]};
    NSSize size = [text sizeWithAttributes:attrs];
    CGFloat x = NSMaxX(rect) - size.width;
    CGFloat y = NSMidY(rect) - size.height / 2.0 + 1.0;
    [text drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
}

- (void)drawLabels {
    NSDictionary *nameAttrs = @{NSFontAttributeName: CachedSystemFont(18, NSFontWeightBold),
                                NSForegroundColorAttributeName: NSColor.labelColor,
                                NSShadowAttributeName: [self shadowWithAlpha:0.22 blur:2.0 y:-1]};
    [self.name drawAtPoint:NSMakePoint(24, self.bounds.size.height - 45) withAttributes:nameAttrs];

    NSString *number = self.hasFollowers ? DecimalStringForInteger(self.followers) : @"--";
    NSDictionary *numberAttrs = @{NSFontAttributeName: CachedMonospacedDigitFont(56, NSFontWeightSemibold),
                                  NSForegroundColorAttributeName: NSColor.controlAccentColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.24 blur:3.5 y:-1]};
    NSSize numberSize = [number sizeWithAttributes:numberAttrs];
    CGFloat numberX = (self.bounds.size.width - numberSize.width) / 2;
    [number drawAtPoint:NSMakePoint(numberX, 70) withAttributes:numberAttrs];

    if (self.hasFollowerDelta && self.followerDelta != 0) {
        NSString *delta = [NSString stringWithFormat:@"%@%ld", self.followerDelta > 0 ? @"+" : @"", (long)self.followerDelta];
        NSColor *deltaColor = self.followerDelta > 0
            ? NSColor.systemGreenColor
            : NSColor.systemRedColor;
        NSDictionary *deltaAttrs = @{NSFontAttributeName: CachedMonospacedDigitFont(15, NSFontWeightSemibold),
                                     NSForegroundColorAttributeName: deltaColor,
                                     NSShadowAttributeName: [self shadowWithAlpha:0.24 blur:2.5 y:-1]};
        [delta drawAtPoint:NSMakePoint(numberX + numberSize.width + 8, 96) withAttributes:deltaAttrs];
    }

    NSDictionary *statusAttrs = @{NSFontAttributeName: CachedSystemFont(13, NSFontWeightBold),
                                  NSForegroundColorAttributeName: NSColor.secondaryLabelColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.20 blur:2.0 y:-1]};
    [self.status drawAtPoint:NSMakePoint(24, 52) withAttributes:statusAttrs];

    NSDictionary *updatedAttrs = @{NSFontAttributeName: CachedSystemFont(13, NSFontWeightMedium),
                                   NSForegroundColorAttributeName: NSColor.tertiaryLabelColor,
                                   NSShadowAttributeName: [self shadowWithAlpha:0.18 blur:2.0 y:-1]};
    NSSize updatedSize = [self.updatedText sizeWithAttributes:updatedAttrs];
    [self.updatedText drawAtPoint:NSMakePoint(self.bounds.size.width - updatedSize.width - 24, 52) withAttributes:updatedAttrs];
}

- (NSShadow *)shadowWithAlpha:(CGFloat)alpha blur:(CGFloat)blur y:(CGFloat)y {
    return CachedTextShadow(alpha, blur, y);
}

- (NSImage *)refreshImage {
    NSImage *image = TemplateSystemSymbolImage(@"arrow.clockwise", @"刷新");
    if (image) return image;
    NSImage *fallback = [[NSImage alloc] initWithSize:NSMakeSize(24, 24)];
    fallback.template = YES;
    [fallback lockFocus];
    [NSColor.blackColor setStroke];
    NSBezierPath *path = [NSBezierPath bezierPath];
    path.lineWidth = 2.1;
    path.lineCapStyle = NSLineCapStyleRound;
    [path appendBezierPathWithArcWithCenter:NSMakePoint(12, 12) radius:7.5 startAngle:35 endAngle:325 clockwise:NO];
    [path stroke];
    [fallback unlockFocus];
    return fallback;
}

- (NSImage *)exitImage {
    NSImage *image = TemplateSystemSymbolImage(@"power.circle.fill", @"退出");
    if (image) return image;
    NSImage *fallback = [[NSImage alloc] initWithSize:NSMakeSize(24, 24)];
    fallback.template = YES;
    [fallback lockFocus];
    [NSColor.blackColor setStroke];
    NSBezierPath *circle = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(5, 5, 14, 14)];
    circle.lineWidth = 1.8;
    [circle stroke];
    NSBezierPath *path = [NSBezierPath bezierPath];
    path.lineWidth = 2.0;
    path.lineCapStyle = NSLineCapStyleRound;
    [path moveToPoint:NSMakePoint(12, 6.8)];
    [path lineToPoint:NSMakePoint(12, 12.6)];
    [path stroke];
    [fallback unlockFocus];
    return fallback;
}

- (void)refresh {
    if (self.refreshing) return;
    if (self.refreshHandler) self.refreshHandler();
}
- (void)copyFollowers {
    if (!self.hasFollowers) {
        self.status = @"暂无数据";
        return;
    }
    NSString *value = DecimalStringForInteger(self.followers);
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:value forType:NSPasteboardTypeString];
    self.status = @"已复制";
}
- (void)openBilibiliProfile {
    if (self.mid <= 0) {
        self.status = @"UID 无效";
        return;
    }
    NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"https://space.bilibili.com/%lld", self.mid]];
    [[NSWorkspace sharedWorkspace] openURL:url];
}
- (void)openSettings {
    if (self.settingsHandler) self.settingsHandler();
}
- (void)copyHistory {
    if (self.copyHistoryHandler) self.copyHistoryHandler();
}
- (void)clearHistory {
    if (self.clearHistoryHandler) self.clearHistoryHandler();
}
- (void)useUltraClearAppearance {
    if (self.appearanceHandler) self.appearanceHandler(AppearanceModeUltraClear);
}
- (void)useClearAppearance {
    if (self.appearanceHandler) self.appearanceHandler(AppearanceModeClear);
}
- (void)useReadableAppearance {
    if (self.appearanceHandler) self.appearanceHandler(AppearanceModeReadable);
}
- (void)useOneMinuteInterval {
    if (self.refreshIntervalHandler) self.refreshIntervalHandler(60);
}
- (void)useFiveMinuteInterval {
    if (self.refreshIntervalHandler) self.refreshIntervalHandler(300);
}
- (void)useFifteenMinuteInterval {
    if (self.refreshIntervalHandler) self.refreshIntervalHandler(900);
}
- (void)toggleAlwaysOnTop {
    if (self.alwaysOnTopHandler) self.alwaysOnTopHandler(!self.alwaysOnTop);
}
- (void)togglePositionLocked {
    if (self.positionLockedHandler) self.positionLockedHandler(!self.positionLocked);
}
- (void)resetPosition {
    if (self.resetPositionHandler) self.resetPositionHandler();
}
- (void)hideCard {
    if (self.hideHandler) self.hideHandler();
}
- (void)quitApp {
    [NSApp terminate:nil];
}
@end

@interface AppDashboardChromeView : NSView
@property(nonatomic) NSInteger appearanceMode;
@property(nonatomic) BOOL usesSystemGlass;
@end

@implementation AppDashboardChromeView

- (void)setAppearanceMode:(NSInteger)appearanceMode {
    NSInteger normalized = appearanceMode == AppearanceModeReadable
        ? AppearanceModeReadable
        : (appearanceMode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    if (_appearanceMode == normalized) return;
    _appearanceMode = normalized;
    self.needsDisplay = YES;
}

- (void)setUsesSystemGlass:(BOOL)usesSystemGlass {
    if (_usesSystemGlass == usesSystemGlass) return;
    _usesSystemGlass = usesSystemGlass;
    self.needsDisplay = YES;
}

- (BOOL)isFlipped {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    NSRect bounds = self.bounds;
    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL dark = AppAppearanceIsDark();
    NSGradient *wash = nil;
    if (self.usesSystemGlass) {
        wash = dark
            ? [[NSGradient alloc] initWithColors:@[
                [[NSColor blackColor] colorWithAlphaComponent:(readable ? 0.26 : (clear ? 0.18 : 0.12))],
                [[NSColor colorWithCalibratedRed:0.025 green:0.032 blue:0.044 alpha:1.0] colorWithAlphaComponent:(readable ? 0.20 : (clear ? 0.13 : 0.08))],
                [[NSColor blackColor] colorWithAlphaComponent:(readable ? 0.22 : (clear ? 0.15 : 0.10))]
            ]]
            : [[NSGradient alloc] initWithColors:@[
                [[NSColor whiteColor] colorWithAlphaComponent:(readable ? 0.16 : (clear ? 0.10 : 0.06))],
                [[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.10 : (clear ? 0.06 : 0.035))],
                [[NSColor whiteColor] colorWithAlphaComponent:(readable ? 0.12 : (clear ? 0.075 : 0.045))]
            ]];
    } else {
        wash = dark
            ? [[NSGradient alloc] initWithColors:@[
                [[NSColor colorWithCalibratedRed:0.070 green:0.078 blue:0.092 alpha:1.0] colorWithAlphaComponent:(readable ? 0.36 : (clear ? 0.22 : 0.12))],
                [[NSColor colorWithCalibratedRed:0.035 green:0.040 blue:0.050 alpha:1.0] colorWithAlphaComponent:(readable ? 0.34 : (clear ? 0.20 : 0.11))],
                [[NSColor colorWithCalibratedRed:0.020 green:0.024 blue:0.032 alpha:1.0] colorWithAlphaComponent:(readable ? 0.38 : (clear ? 0.24 : 0.13))]
            ]]
            : [[NSGradient alloc] initWithColors:@[
                [[NSColor controlBackgroundColor] colorWithAlphaComponent:(readable ? 0.050 : (clear ? 0.016 : 0.002))],
                [[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.026 : (clear ? 0.008 : 0.001))],
                [[NSColor underPageBackgroundColor] colorWithAlphaComponent:(readable ? 0.032 : (clear ? 0.010 : 0.001))]
            ]];
    }
    [wash drawInRect:bounds angle:-24];
    [self drawPanel:NSMakeRect(24, 176, 456, 370) radius:20 accent:NSColor.controlAccentColor];
    [self drawPanel:NSMakeRect(24, 20, 456, 132) radius:18 accent:NSColor.systemGreenColor];
    [self drawPanel:NSMakeRect(504, 36, 372, 500) radius:18 accent:NSColor.controlAccentColor];
    [self drawInspectorSeparators];
    [self drawControlChrome];

}

- (void)drawPanel:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent {
    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL dark = AppAppearanceIsDark();
    BOOL leftPanel = NSMinX(rect) < 500;
    CGFloat strokeAlpha = dark
        ? (leftPanel
            ? (readable ? 0.052 : (clear ? 0.024 : 0.012))
            : (readable ? 0.150 : (clear ? 0.082 : 0.046)))
        : (readable ? 0.12 : (clear ? 0.058 : 0.026));
    CGFloat rimStrength = dark
        ? (leftPanel
            ? (readable ? 0.180 : (clear ? 0.105 : 0.070))
            : (readable ? 0.420 : (clear ? 0.300 : 0.220)))
        : (readable ? 0.42 : (clear ? 0.30 : 0.20));

    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
    if (self.usesSystemGlass) {
        CGFloat fillAlpha = dark
            ? (leftPanel
                ? LiquidGlassAlphaForMode(self.appearanceMode, 0.080, 0.052, 0.032)
                : LiquidGlassAlphaForMode(self.appearanceMode, 0.115, 0.075, 0.046))
            : LiquidGlassAlphaForMode(self.appearanceMode, 0.070, 0.043, 0.026);
        NSColor *panelFillColor = dark ? NSColor.blackColor : NSColor.whiteColor;
        [[panelFillColor colorWithAlphaComponent:fillAlpha] setFill];
        [shape fill];
        DrawLiquidGlassOpticalOverlay(rect, radius, accent, self.appearanceMode, dark ? 0.13 : 0.17);
        CGFloat softStrokeAlpha = dark
            ? (leftPanel
                ? LiquidGlassAlphaForMode(self.appearanceMode, 0.020, 0.012, 0.006)
                : LiquidGlassAlphaForMode(self.appearanceMode, 0.048, 0.030, 0.016))
            : LiquidGlassAlphaForMode(self.appearanceMode, 0.058, 0.034, 0.018);
        if (softStrokeAlpha > 0.012) {
            [[[NSColor separatorColor] colorWithAlphaComponent:softStrokeAlpha] setStroke];
            shape.lineWidth = 0.5;
            [shape stroke];
        }
        DrawLiquidGlassRim(rect, radius, accent, dark ? 0.060 : 0.090);
        return;
    }
    DrawFloatingGlassShadow(rect, radius, self.appearanceMode, (dark && leftPanel) ? 0.52 : 0.78);
    if (!(dark && leftPanel)) {
        DrawLiquidGlassOpticalOverlay(rect, radius, accent, self.appearanceMode, 0.72);
    }

    if (!(dark && leftPanel) && strokeAlpha > 0.018) {
        [[[NSColor separatorColor] colorWithAlphaComponent:strokeAlpha] setStroke];
        shape.lineWidth = 0.8;
        [shape stroke];
    }
    DrawLiquidGlassRim(rect, radius, accent, rimStrength);
}

- (void)drawGlassControl:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent emphasized:(BOOL)emphasized {
    if (self.usesSystemGlass) {
        DrawLiquidGlassOpticalOverlay(rect, radius, accent, self.appearanceMode, emphasized ? 0.34 : 0.18);
        DrawLiquidGlassRim(rect, radius, accent, emphasized ? 0.18 : 0.09);
        return;
    }
    if (AppAppearanceIsDark() && NSMinX(rect) < 500) {
        DrawLiquidGlassOpticalOverlay(rect, radius, accent, self.appearanceMode, emphasized ? 0.40 : 0.22);
        DrawLiquidGlassRim(rect, radius, accent, emphasized ? 0.18 : 0.070);
        return;
    }
    DrawLiquidGlassControlWell(rect, radius, accent, self.appearanceMode, emphasized);
}

- (void)drawInspectorSeparators {
    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL dark = AppAppearanceIsDark();
    if (dark) {
        return;
    }
    CGFloat alpha = dark
        ? (readable ? 0.065 : (clear ? 0.040 : 0.024))
        : (readable ? 0.24 : (clear ? 0.13 : 0.070));
    for (NSNumber *value in @[@410, @266, @136]) {
        CGFloat y = value.doubleValue;
        if (dark) {
            NSGradient *softDivider = [[NSGradient alloc] initWithColors:@[
                [[NSColor whiteColor] colorWithAlphaComponent:0.0],
                [[NSColor whiteColor] colorWithAlphaComponent:alpha],
                [[NSColor whiteColor] colorWithAlphaComponent:0.0]
            ]];
            [softDivider drawInRect:NSMakeRect(520, y - 1.2, 340, 2.4) angle:90];
        } else {
            [[[NSColor separatorColor] colorWithAlphaComponent:alpha] setStroke];
            NSBezierPath *line = [NSBezierPath bezierPath];
            line.lineWidth = 0.6;
            [line moveToPoint:NSMakePoint(520, y)];
            [line lineToPoint:NSMakePoint(860, y)];
            [line stroke];
        }
    }
}

- (void)drawControlChrome {
    [self drawGlassControl:NSMakeRect(36, 198, 94, 39) radius:14 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(136, 198, 116, 39) radius:14 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(258, 198, 94, 39) radius:14 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(36, 50, 136, 39) radius:14 accent:NSColor.systemGreenColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(178, 50, 106, 39) radius:14 accent:NSColor.systemRedColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(544, 424, 240, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(788, 424, 74, 36) radius:13 accent:NSColor.controlAccentColor emphasized:YES];
    [self drawGlassControl:NSMakeRect(696, 170, 168, 32) radius:12 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(516, 62, 104, 38) radius:14 accent:NSColor.systemGrayColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(634, 62, 118, 38) radius:14 accent:NSColor.systemGrayColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(758, 62, 106, 38) radius:14 accent:NSColor.systemRedColor emphasized:NO];
}

@end

@interface PreferencesGlassChromeView : NSView
@property(nonatomic) NSInteger appearanceMode;
@end

@implementation PreferencesGlassChromeView

- (void)setAppearanceMode:(NSInteger)appearanceMode {
    NSInteger normalized = appearanceMode == AppearanceModeReadable
        ? AppearanceModeReadable
        : (appearanceMode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    if (_appearanceMode == normalized) return;
    _appearanceMode = normalized;
    self.needsDisplay = YES;
}

- (BOOL)isFlipped {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL dark = AppAppearanceIsDark();
    NSGradient *wash = dark
        ? [[NSGradient alloc] initWithColors:@[
            [[NSColor colorWithCalibratedRed:0.068 green:0.074 blue:0.086 alpha:1.0] colorWithAlphaComponent:(readable ? 0.34 : (clear ? 0.20 : 0.11))],
            [[NSColor colorWithCalibratedRed:0.036 green:0.041 blue:0.050 alpha:1.0] colorWithAlphaComponent:(readable ? 0.32 : (clear ? 0.18 : 0.10))],
            [[NSColor colorWithCalibratedRed:0.020 green:0.024 blue:0.031 alpha:1.0] colorWithAlphaComponent:(readable ? 0.36 : (clear ? 0.22 : 0.12))]
        ]]
        : [[NSGradient alloc] initWithColors:@[
            [[NSColor controlBackgroundColor] colorWithAlphaComponent:(readable ? 0.046 : (clear ? 0.014 : 0.002))],
            [[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.024 : (clear ? 0.008 : 0.001))],
            [[NSColor underPageBackgroundColor] colorWithAlphaComponent:(readable ? 0.030 : (clear ? 0.010 : 0.001))]
        ]];
    [wash drawInRect:self.bounds angle:-24];

    [self drawPanel:NSMakeRect(18, 292, 444, 92) radius:22 accent:NSColor.systemPinkColor];
    [self drawPanel:NSMakeRect(18, 176, 444, 102) radius:20 accent:NSColor.controlAccentColor];
    [self drawPanel:NSMakeRect(18, 34, 444, 128) radius:20 accent:NSColor.systemBlueColor];
    [self drawControls];
}

- (void)drawPanel:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent {
    DrawFloatingGlassShadow(rect, radius, self.appearanceMode, 0.72);
    DrawLiquidGlassOpticalOverlay(rect, radius, accent, self.appearanceMode, 0.70);
    DrawLiquidGlassRim(rect, radius, accent, 0.30);
}

- (void)drawControl:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent emphasized:(BOOL)emphasized {
    DrawLiquidGlassControlWell(rect, radius, accent, self.appearanceMode, emphasized);
}

- (void)drawControls {
    [self drawControl:NSMakeRect(128, 308, 238, 36) radius:13 accent:NSColor.systemPinkColor emphasized:NO];
    [self drawControl:NSMakeRect(368, 308, 56, 36) radius:13 accent:NSColor.systemPinkColor emphasized:YES];
    [self drawControl:NSMakeRect(426, 308, 44, 36) radius:13 accent:NSColor.systemPinkColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 263, 222, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 223, 222, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 183, 222, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 143, 112, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(254, 143, 142, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 113, 120, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(254, 113, 132, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 83, 112, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(254, 83, 112, 32) radius:12 accent:NSColor.systemBlueColor emphasized:NO];
    [self drawControl:NSMakeRect(128, 44, 124, 36) radius:13 accent:NSColor.systemGrayColor emphasized:NO];
    [self drawControl:NSMakeRect(242, 44, 100, 36) radius:13 accent:NSColor.controlAccentColor emphasized:YES];
    [self drawControl:NSMakeRect(374, 44, 82, 36) radius:13 accent:NSColor.systemGrayColor emphasized:NO];
}

@end

@interface StatusGlassSegmentedControl : NSSegmentedControl
@property(nonatomic) NSInteger appearanceMode;
@end

@implementation StatusGlassSegmentedControl

- (BOOL)isFlipped {
    return NO;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;
}

- (void)setAppearanceMode:(NSInteger)appearanceMode {
    NSInteger normalized = appearanceMode == AppearanceModeReadable
        ? AppearanceModeReadable
        : (appearanceMode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    if (_appearanceMode == normalized) return;
    _appearanceMode = normalized;
    self.needsDisplay = YES;
}

- (void)setSelectedSegment:(NSInteger)selectedSegment {
    [super setSelectedSegment:selectedSegment];
    for (NSInteger i = 0; i < self.segmentCount; i++) {
        [self setSelected:(i == selectedSegment) forSegment:i];
    }
    self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    BOOL dark = AppAppearanceIsDark();
    CGFloat alpha = self.enabled ? 1.0 : 0.42;
    NSRect bounds = NSInsetRect(self.bounds, 0.5, 0.5);
    CGFloat radius = MIN(11.0, bounds.size.height / 2.0);

    NSInteger selected = self.selectedSegment;
    if (selected >= 0 && selected < self.segmentCount) {
        CGFloat segmentWidth = bounds.size.width / MAX(1, self.segmentCount);
        NSRect selectedRect = NSMakeRect(NSMinX(bounds) + segmentWidth * selected + 1.0,
                                         NSMinY(bounds) + 1.0,
                                         segmentWidth - 2.0,
                                         bounds.size.height - 2.0);
        NSBezierPath *selectedPath = [NSBezierPath bezierPathWithRoundedRect:selectedRect
                                                                     xRadius:MAX(7.0, radius - 1.0)
                                                                     yRadius:MAX(7.0, radius - 1.0)];
        NSGradient *selectedFill = [[NSGradient alloc] initWithColors:@[
            [[NSColor controlAccentColor] colorWithAlphaComponent:(dark ? 0.76 : 0.70) * alpha],
            [[NSColor controlAccentColor] colorWithAlphaComponent:(dark ? 0.54 : 0.50) * alpha]
        ]];
        [selectedFill drawInBezierPath:selectedPath angle:90];
        NSGradient *selectedSheen = [[NSGradient alloc] initWithColors:@[
            [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.18 : 0.24) * alpha],
            [[NSColor whiteColor] colorWithAlphaComponent:0.0]
        ]];
        [selectedSheen drawInBezierPath:selectedPath angle:90];
    }

    for (NSInteger i = 0; i < self.segmentCount; i++) {
        NSString *label = [self labelForSegment:i] ?: @"";
        BOOL segmentSelected = i == selected;
        NSColor *textColor = segmentSelected
            ? NSColor.alternateSelectedControlTextColor
            : (dark ? [[NSColor whiteColor] colorWithAlphaComponent:0.86 * alpha] : [[NSColor blackColor] colorWithAlphaComponent:0.72 * alpha]);
        NSDictionary *attrs = @{NSFontAttributeName: self.font ?: CachedSystemFont(11, NSFontWeightMedium),
                                NSForegroundColorAttributeName: textColor};
        NSSize size = [label sizeWithAttributes:attrs];
        CGFloat segmentWidth = bounds.size.width / MAX(1, self.segmentCount);
        CGFloat x = NSMinX(bounds) + segmentWidth * i + (segmentWidth - size.width) / 2.0;
        CGFloat y = NSMidY(bounds) - size.height / 2.0 + 0.5;
        [label drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
    }
}

@end

@interface StatusPopoverView : NSView
@property(nonatomic, copy) NSString *name;
@property(nonatomic) NSInteger followers;
@property(nonatomic) BOOL hasFollowers;
@property(nonatomic) NSInteger followerDelta;
@property(nonatomic) BOOL hasFollowerDelta;
@property(nonatomic, copy) NSArray<NSNumber *> *historyValues;
@property(nonatomic, copy) NSString *status;
@property(nonatomic, copy) NSString *updatedText;
@property(nonatomic, copy) NSString *refreshScheduleText;
@property(nonatomic) BOOL menuBarOnly;
@property(nonatomic) BOOL copyEnabled;
@property(nonatomic) BOOL historyEnabled;
@property(nonatomic) BOOL profileEnabled;
@property(nonatomic) BOOL refreshing;
@property(nonatomic) BOOL notificationsEnabled;
@property(nonatomic) BOOL launchAtLoginEnabled;
@property(nonatomic) BOOL launchAtLoginAvailable;
@property(nonatomic) NSTimeInterval refreshInterval;
@property(nonatomic) BOOL autoRefreshEnabled;
@property(nonatomic) NSInteger statusDisplayMode;
@property(nonatomic) NSInteger appearanceMode;
@property(nonatomic, copy) void (^refreshHandler)(void);
@property(nonatomic, copy) void (^copyHandler)(void);
@property(nonatomic, copy) void (^profileHandler)(void);
@property(nonatomic, copy) void (^historyHandler)(void);
@property(nonatomic, copy) void (^notificationHandler)(BOOL enabled);
@property(nonatomic, copy) void (^launchAtLoginHandler)(BOOL enabled);
@property(nonatomic, copy) void (^settingsHandler)(void);
@property(nonatomic, copy) void (^showCardHandler)(void);
@property(nonatomic, copy) void (^refreshIntervalHandler)(NSTimeInterval interval);
@property(nonatomic, copy) void (^autoRefreshHandler)(BOOL enabled);
@property(nonatomic, copy) void (^menuBarOnlyHandler)(BOOL enabled);
@property(nonatomic, copy) void (^statusDisplayHandler)(NSInteger mode);
@property(nonatomic, copy) void (^appearanceHandler)(NSInteger mode);
@property(nonatomic, copy) void (^quitHandler)(void);
@property(nonatomic) BOOL usesSystemGlass;
- (void)invalidateAppearanceDrivenControls;
@end

@implementation StatusPopoverView {
    NSButton *_refreshButton;
    NSButton *_copyButton;
    NSButton *_profileButton;
    NSButton *_historyButton;
    NSButton *_notificationButton;
    NSButton *_settingsButton;
    NSButton *_showCardButton;
    NSButton *_quitButton;
    NSSegmentedControl *_appearanceControl;
    NSSegmentedControl *_statusDisplayControl;
    NSSegmentedControl *_intervalControl;
    NSButton *_autoRefreshButton;
    NSButton *_launchAtLoginButton;
    NSButton *_menuBarOnlyButton;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    (void)event;
    return YES;
}

- (void)styleGlassSegmentedControl:(NSSegmentedControl *)control {
    if (!control) return;
    control.segmentStyle = NSSegmentStyleCapsule;
    control.wantsLayer = YES;
    control.font = [NSFont systemFontOfSize:11 weight:NSFontWeightMedium];
    if (@available(macOS 10.15, *)) {
        control.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

- (void)styleGlassCheckbox:(NSButton *)button {
    if (!button) return;
    button.wantsLayer = YES;
    button.contentTintColor = NSColor.secondaryLabelColor;
    if (@available(macOS 10.15, *)) {
        button.layer.cornerCurve = kCACornerCurveContinuous;
    }
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    self.wantsLayer = YES;
    self.name = [Settings name];
    self.status = @"等待刷新";
    self.updatedText = @"--";
    self.historyValues = @[];

    _refreshButton = [self iconButton:@"arrow.clockwise" tooltip:@"刷新" action:@selector(refreshTapped:)];
    _copyButton = [self iconButton:@"doc.on.doc" tooltip:@"复制粉丝数" action:@selector(copyTapped:)];
    _profileButton = [self iconButton:@"arrow.up.right.square" tooltip:@"打开 B站主页" action:@selector(profileTapped:)];
    _historyButton = [self iconButton:@"tablecells" tooltip:@"复制一周 CSV" action:@selector(historyTapped:)];
    _notificationButton = [self iconButton:@"bell" tooltip:@"粉丝变化提醒" action:@selector(notificationTapped:)];
    _settingsButton = [self iconButton:@"slider.horizontal.3" tooltip:@"设置" action:@selector(settingsTapped:)];
    _showCardButton = [self iconButton:@"macwindow" tooltip:@"显示主窗口" action:@selector(showCardTapped:)];
    _quitButton = [self iconButton:@"power" tooltip:@"退出" action:@selector(quitTapped:)];
    _quitButton.contentTintColor = NSColor.systemRedColor;
    _appearanceControl = [[StatusGlassSegmentedControl alloc] initWithFrame:NSZeroRect];
    _appearanceControl.segmentCount = 3;
    [_appearanceControl setLabel:@"极透" forSegment:0];
    [_appearanceControl setLabel:@"清透" forSegment:1];
    [_appearanceControl setLabel:@"可读" forSegment:2];
    _appearanceControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    _appearanceControl.target = self;
    _appearanceControl.action = @selector(appearanceChanged:);
    _appearanceControl.toolTip = @"玻璃外观";
    [self styleGlassSegmentedControl:_appearanceControl];

    _statusDisplayControl = [[StatusGlassSegmentedControl alloc] initWithFrame:NSZeroRect];
    _statusDisplayControl.segmentCount = 3;
    [_statusDisplayControl setLabel:@"精确" forSegment:0];
    [_statusDisplayControl setLabel:@"紧凑" forSegment:1];
    [_statusDisplayControl setLabel:@"图标" forSegment:2];
    _statusDisplayControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    _statusDisplayControl.target = self;
    _statusDisplayControl.action = @selector(statusDisplayChanged:);
    _statusDisplayControl.toolTip = @"顶栏显示方式";
    [self styleGlassSegmentedControl:_statusDisplayControl];

    _intervalControl = [[StatusGlassSegmentedControl alloc] initWithFrame:NSZeroRect];
    _intervalControl.segmentCount = 3;
    [_intervalControl setLabel:@"1m" forSegment:0];
    [_intervalControl setLabel:@"5m" forSegment:1];
    [_intervalControl setLabel:@"15m" forSegment:2];
    _intervalControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    _intervalControl.target = self;
    _intervalControl.action = @selector(intervalChanged:);
    _intervalControl.toolTip = @"刷新间隔";
    [self styleGlassSegmentedControl:_intervalControl];

    _autoRefreshButton = [NSButton checkboxWithTitle:@"自动" target:self action:@selector(autoRefreshChanged:)];
    _autoRefreshButton.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    _autoRefreshButton.toolTip = @"自动刷新";
    [self styleGlassCheckbox:_autoRefreshButton];

    _launchAtLoginButton = [NSButton checkboxWithTitle:@"登录" target:self action:@selector(launchAtLoginChanged:)];
    _launchAtLoginButton.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    _launchAtLoginButton.toolTip = @"登录后自动启动";
    [self styleGlassCheckbox:_launchAtLoginButton];

    _menuBarOnlyButton = [NSButton checkboxWithTitle:@"仅顶栏" target:self action:@selector(menuBarOnlyChanged:)];
    _menuBarOnlyButton.font = [NSFont systemFontOfSize:11 weight:NSFontWeightSemibold];
    _menuBarOnlyButton.toolTip = @"启动时只显示顶栏小组件";
    [self styleGlassCheckbox:_menuBarOnlyButton];
    [self addSubview:_refreshButton];
    [self addSubview:_copyButton];
    [self addSubview:_profileButton];
    [self addSubview:_historyButton];
    [self addSubview:_notificationButton];
    [self addSubview:_settingsButton];
    [self addSubview:_showCardButton];
    [self addSubview:_quitButton];
    [self addSubview:_appearanceControl];
    [self addSubview:_statusDisplayControl];
    [self addSubview:_intervalControl];
    [self addSubview:_autoRefreshButton];
    [self addSubview:_launchAtLoginButton];
    [self addSubview:_menuBarOnlyButton];
    return self;
}

- (NSButton *)iconButton:(NSString *)symbol tooltip:(NSString *)tooltip action:(SEL)action {
    NSImage *image = TemplateSystemSymbolImage(symbol, tooltip) ?: [self fallbackIconWithText:[tooltip substringToIndex:1]];
    NSButton *button = [NSButton buttonWithImage:image
                                         target:self
                                         action:action];
    button.bezelStyle = NSBezelStyleInline;
    button.bordered = NO;
    button.imagePosition = NSImageOnly;
    button.imageScaling = NSImageScaleProportionallyDown;
    button.contentTintColor = NSColor.secondaryLabelColor;
    button.toolTip = tooltip;
    return button;
}

- (NSImage *)fallbackIconWithText:(NSString *)text {
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(20, 20)];
    [image lockFocus];
    NSDictionary *attrs = @{NSFontAttributeName: [NSFont systemFontOfSize:12 weight:NSFontWeightBold],
                            NSForegroundColorAttributeName: NSColor.labelColor};
    NSSize size = [text sizeWithAttributes:attrs];
    [text drawAtPoint:NSMakePoint((20 - size.width) / 2.0, (20 - size.height) / 2.0) withAttributes:attrs];
    [image unlockFocus];
    return image;
}

- (void)layout {
    [super layout];
    CGFloat width = self.bounds.size.width;
    CGFloat inset = 24;
    _appearanceControl.frame = NSMakeRect(inset, 124, width - inset * 2, 26);
    _statusDisplayControl.frame = NSMakeRect(inset, 93, width - inset * 2, 26);

    CGFloat quickRowWidth = 296;
    CGFloat quickX = MAX(11, (width - quickRowWidth) / 2.0);
    _intervalControl.frame = NSMakeRect(quickX, 61, 104, 26);
    _autoRefreshButton.frame = NSMakeRect(quickX + 112, 62, 52, 24);
    _launchAtLoginButton.frame = NSMakeRect(quickX + 172, 62, 52, 24);
    _menuBarOnlyButton.frame = NSMakeRect(quickX + 232, 62, 64, 24);

    CGFloat y = 14;
    CGFloat size = 30;
    CGFloat gap = 8;
    CGFloat total = size * 8 + gap * 7;
    CGFloat x = (width - total) / 2.0;
    _refreshButton.frame = NSMakeRect(x, y, size, size);
    _copyButton.frame = NSMakeRect(x + (size + gap), y, size, size);
    _profileButton.frame = NSMakeRect(x + (size + gap) * 2, y, size, size);
    _historyButton.frame = NSMakeRect(x + (size + gap) * 3, y, size, size);
    _notificationButton.frame = NSMakeRect(x + (size + gap) * 4, y, size, size);
    _settingsButton.frame = NSMakeRect(x + (size + gap) * 5, y, size, size);
    _showCardButton.frame = NSMakeRect(x + (size + gap) * 6, y, size, size);
    _quitButton.frame = NSMakeRect(x + (size + gap) * 7, y, size, size);
}

- (void)setName:(NSString *)name {
    NSString *next = [name copy] ?: @"";
    if ([_name isEqualToString:next]) return;
    _name = next;
    self.needsDisplay = YES;
}
- (void)setStatus:(NSString *)status {
    NSString *next = [status copy] ?: @"";
    if ([_status isEqualToString:next]) return;
    _status = next;
    self.needsDisplay = YES;
}
- (void)setUpdatedText:(NSString *)updatedText {
    NSString *next = [updatedText copy] ?: @"";
    if ([_updatedText isEqualToString:next]) return;
    _updatedText = next;
    self.needsDisplay = YES;
}
- (void)setRefreshScheduleText:(NSString *)refreshScheduleText {
    NSString *next = [refreshScheduleText copy] ?: @"";
    if ([_refreshScheduleText isEqualToString:next]) return;
    _refreshScheduleText = next;
    self.needsDisplay = YES;
}
- (void)setHistoryValues:(NSArray<NSNumber *> *)historyValues {
    if (_historyValues == historyValues) return;
    NSArray<NSNumber *> *next = [historyValues copy] ?: @[];
    if ([_historyValues isEqualToArray:next]) return;
    _historyValues = next;
    self.needsDisplay = YES;
}
- (void)setFollowers:(NSInteger)followers {
    if (_followers == followers) return;
    _followers = followers;
    self.needsDisplay = YES;
}
- (void)setHasFollowers:(BOOL)hasFollowers {
    if (_hasFollowers == hasFollowers) return;
    _hasFollowers = hasFollowers;
    self.needsDisplay = YES;
}
- (void)setFollowerDelta:(NSInteger)followerDelta {
    if (_followerDelta == followerDelta) return;
    _followerDelta = followerDelta;
    self.needsDisplay = YES;
}
- (void)setHasFollowerDelta:(BOOL)hasFollowerDelta {
    if (_hasFollowerDelta == hasFollowerDelta) return;
    _hasFollowerDelta = hasFollowerDelta;
    self.needsDisplay = YES;
}
- (void)setMenuBarOnly:(BOOL)menuBarOnly {
    BOOL changed = _menuBarOnly != menuBarOnly;
    _menuBarOnly = menuBarOnly;
    SetButtonStateIfChanged(_menuBarOnlyButton, menuBarOnly ? NSControlStateValueOn : NSControlStateValueOff);
    if (changed) self.needsDisplay = YES;
}
- (void)setCopyEnabled:(BOOL)copyEnabled {
    BOOL changed = _copyEnabled != copyEnabled;
    _copyEnabled = copyEnabled;
    SetControlEnabledIfChanged(_copyButton, copyEnabled);
    SetViewAlphaIfChanged(_copyButton, copyEnabled ? 1.0 : 0.35);
    if (changed) self.needsDisplay = YES;
}
- (void)setHistoryEnabled:(BOOL)historyEnabled {
    BOOL changed = _historyEnabled != historyEnabled;
    _historyEnabled = historyEnabled;
    SetControlEnabledIfChanged(_historyButton, historyEnabled);
    SetViewAlphaIfChanged(_historyButton, historyEnabled ? 1.0 : 0.35);
    if (changed) self.needsDisplay = YES;
}
- (void)setProfileEnabled:(BOOL)profileEnabled {
    BOOL changed = _profileEnabled != profileEnabled;
    _profileEnabled = profileEnabled;
    SetControlEnabledIfChanged(_profileButton, profileEnabled);
    SetViewAlphaIfChanged(_profileButton, profileEnabled ? 1.0 : 0.35);
    if (changed) self.needsDisplay = YES;
}
- (void)setRefreshing:(BOOL)refreshing {
    BOOL changed = _refreshing != refreshing;
    _refreshing = refreshing;
    SetControlEnabledIfChanged(_refreshButton, !refreshing);
    SetViewAlphaIfChanged(_refreshButton, refreshing ? 0.35 : 1.0);
    SetViewToolTipIfChanged(_refreshButton, refreshing ? @"正在刷新" : @"刷新");
    if (changed) self.needsDisplay = YES;
}
- (void)setNotificationsEnabled:(BOOL)notificationsEnabled {
    BOOL changed = _notificationsEnabled != notificationsEnabled;
    _notificationsEnabled = notificationsEnabled;
    SetButtonContentTintColorIfChanged(_notificationButton, notificationsEnabled
        ? NSColor.controlAccentColor
        : NSColor.secondaryLabelColor);
    if (changed) self.needsDisplay = YES;
}
- (void)setLaunchAtLoginEnabled:(BOOL)launchAtLoginEnabled {
    BOOL changed = _launchAtLoginEnabled != launchAtLoginEnabled;
    _launchAtLoginEnabled = launchAtLoginEnabled;
    SetButtonStateIfChanged(_launchAtLoginButton, launchAtLoginEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    if (changed) self.needsDisplay = YES;
}
- (void)setLaunchAtLoginAvailable:(BOOL)launchAtLoginAvailable {
    BOOL changed = _launchAtLoginAvailable != launchAtLoginAvailable;
    _launchAtLoginAvailable = launchAtLoginAvailable;
    SetControlEnabledIfChanged(_launchAtLoginButton, launchAtLoginAvailable);
    SetViewAlphaIfChanged(_launchAtLoginButton, launchAtLoginAvailable ? 1.0 : 0.35);
    SetViewToolTipIfChanged(_launchAtLoginButton, launchAtLoginAvailable
        ? @"登录后自动启动"
        : @"登录启动不可用");
    if (changed) self.needsDisplay = YES;
}
- (void)setRefreshInterval:(NSTimeInterval)refreshInterval {
    NSTimeInterval next = refreshInterval >= 60 ? refreshInterval : 60;
    BOOL changed = _refreshInterval != next;
    _refreshInterval = next;
    if (_refreshInterval <= 90) {
        SetSegmentedControlSelectedSegmentIfChanged(_intervalControl, 0);
    } else if (_refreshInterval <= 450) {
        SetSegmentedControlSelectedSegmentIfChanged(_intervalControl, 1);
    } else {
        SetSegmentedControlSelectedSegmentIfChanged(_intervalControl, 2);
    }
    if (changed) self.needsDisplay = YES;
}
- (void)setAutoRefreshEnabled:(BOOL)autoRefreshEnabled {
    BOOL changed = _autoRefreshEnabled != autoRefreshEnabled;
    _autoRefreshEnabled = autoRefreshEnabled;
    SetButtonStateIfChanged(_autoRefreshButton, autoRefreshEnabled ? NSControlStateValueOn : NSControlStateValueOff);
    SetControlEnabledIfChanged(_intervalControl, autoRefreshEnabled);
    SetViewAlphaIfChanged(_intervalControl, autoRefreshEnabled ? 1.0 : 0.42);
    if (changed) self.needsDisplay = YES;
}
- (void)setStatusDisplayMode:(NSInteger)statusDisplayMode {
    NSInteger normalized = statusDisplayMode == StatusDisplayModeIconOnly
        ? StatusDisplayModeIconOnly
        : (statusDisplayMode == StatusDisplayModeCompact ? StatusDisplayModeCompact : StatusDisplayModeExact);
    BOOL changed = _statusDisplayMode != normalized;
    if (statusDisplayMode == StatusDisplayModeIconOnly) {
        _statusDisplayMode = StatusDisplayModeIconOnly;
        SetSegmentedControlSelectedSegmentIfChanged(_statusDisplayControl, 2);
    } else if (statusDisplayMode == StatusDisplayModeCompact) {
        _statusDisplayMode = StatusDisplayModeCompact;
        SetSegmentedControlSelectedSegmentIfChanged(_statusDisplayControl, 1);
    } else {
        _statusDisplayMode = StatusDisplayModeExact;
        SetSegmentedControlSelectedSegmentIfChanged(_statusDisplayControl, 0);
    }
    if (changed) self.needsDisplay = YES;
}
- (void)setAppearanceMode:(NSInteger)appearanceMode {
    NSInteger normalized = appearanceMode == AppearanceModeReadable
        ? AppearanceModeReadable
        : (appearanceMode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    BOOL changed = _appearanceMode != normalized;
    if (appearanceMode == AppearanceModeReadable) {
        _appearanceMode = AppearanceModeReadable;
        SetSegmentedControlSelectedSegmentIfChanged(_appearanceControl, 2);
    } else if (appearanceMode == AppearanceModeClear) {
        _appearanceMode = AppearanceModeClear;
        SetSegmentedControlSelectedSegmentIfChanged(_appearanceControl, 1);
    } else {
        _appearanceMode = AppearanceModeUltraClear;
        SetSegmentedControlSelectedSegmentIfChanged(_appearanceControl, 0);
    }
    for (NSSegmentedControl *control in @[_appearanceControl, _statusDisplayControl, _intervalControl]) {
        if ([control isKindOfClass:StatusGlassSegmentedControl.class]) {
            ((StatusGlassSegmentedControl *)control).appearanceMode = _appearanceMode;
        }
    }
    if (changed) self.needsDisplay = YES;
}

- (void)setUsesSystemGlass:(BOOL)usesSystemGlass {
    if (_usesSystemGlass == usesSystemGlass) return;
    _usesSystemGlass = usesSystemGlass;
    self.needsDisplay = YES;
}

- (void)invalidateAppearanceDrivenControls {
    self.needsDisplay = YES;
    NSView *views[] = {
        _appearanceControl,
        _statusDisplayControl,
        _intervalControl,
        _autoRefreshButton,
        _launchAtLoginButton,
        _menuBarOnlyButton,
        _notificationButton,
        _settingsButton,
        _showCardButton,
        _quitButton
    };
    for (NSUInteger i = 0; i < sizeof(views) / sizeof(views[0]); i++) {
        MarkViewNeedsDisplay(views[i]);
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    NSRect rect = self.usesSystemGlass ? self.bounds : NSInsetRect(self.bounds, 0.75, 0.75);
    if (!self.usesSystemGlass) {
        NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
        BOOL readable = effectiveMode == AppearanceModeReadable;
        DrawFloatingGlassShadow(rect, 20, self.appearanceMode, readable ? 0.34 : 0.22);
        DrawStatusPopoverGlassBackdrop(rect, 20, self.appearanceMode);
        DrawStatusPopoverGlassRim(rect, 20, self.appearanceMode);
    } else {
        BOOL dark = AppAppearanceIsDark();
        CGFloat tintAlpha = dark
            ? LiquidGlassAlphaForMode(self.appearanceMode, 0.40, 0.32, 0.24)
            : LiquidGlassAlphaForMode(self.appearanceMode, 0.105, 0.060, 0.032);
        NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:20 yRadius:20];
        NSColor *tintColor = dark ? NSColor.blackColor : NSColor.whiteColor;
        [[tintColor colorWithAlphaComponent:tintAlpha] setFill];
        [shape fill];
        DrawLiquidGlassOpticalOverlay(NSInsetRect(rect, 1.0, 1.0),
                                      19.0,
                                      NSColor.controlAccentColor,
                                      self.appearanceMode,
                                      dark ? 0.92 : 0.74);
        DrawStatusGlassEdgeHighlight(rect, 20.0, self.appearanceMode);
    }

    [self drawHeader];
    [self drawFollowerNumber];
    CGFloat lift = MAX(0, self.bounds.size.height - 226);
    NSRect trendRect = NSMakeRect(24, 88 + lift, self.bounds.size.width - 116, 18);
    [self drawSparklineInRect:trendRect];
    [self drawTrendSummaryInRect:NSMakeRect(NSMaxX(trendRect) + 8, NSMinY(trendRect), 60, 18)];
    [self drawQuickControlBackplates];
    [self drawActionBackplates];
}

- (void)drawHeader {
    CGFloat headerCenterY = self.bounds.size.height - 29.5;
    [self drawMiniIconAt:NSMakePoint(22, headerCenterY - 8.5)];
    NSDictionary *nameAttrs = @{NSFontAttributeName: CachedSystemFont(13, NSFontWeightBold),
                                NSForegroundColorAttributeName: NSColor.labelColor,
                                NSShadowAttributeName: [self shadowWithAlpha:0.20 blur:2.0 y:-1]};
    NSString *name = self.name.length > 0 ? self.name : @"BILI粉丝数";
    NSSize nameSize = [name sizeWithAttributes:nameAttrs];
    [name drawAtPoint:NSMakePoint(49, headerCenterY - nameSize.height / 2.0) withAttributes:nameAttrs];

    if (self.menuBarOnly || !self.autoRefreshEnabled || self.refreshScheduleText.length > 0) {
        NSDictionary *badgeAttrs = @{NSFontAttributeName: CachedSystemFont(10, NSFontWeightSemibold),
                                     NSForegroundColorAttributeName: NSColor.secondaryLabelColor};
        NSString *badge = !self.autoRefreshEnabled
            ? @"已暂停"
            : (self.refreshScheduleText.length > 0 ? self.refreshScheduleText : @"仅顶栏");
        NSSize size = [badge sizeWithAttributes:badgeAttrs];
        [badge drawAtPoint:NSMakePoint(self.bounds.size.width - size.width - 24, headerCenterY - size.height / 2.0)
            withAttributes:badgeAttrs];
    }
}

- (void)drawMiniIconAt:(NSPoint)point {
    NSRect body = NSMakeRect(point.x, point.y, 20, 17);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:body xRadius:6 yRadius:6];
    NSGradient *fill = [[NSGradient alloc] initWithStartingColor:[[NSColor controlAccentColor] colorWithAlphaComponent:0.92]
                                                     endingColor:[[NSColor controlAccentColor] colorWithAlphaComponent:0.68]];
    [fill drawInBezierPath:shape angle:25];
    [[[NSColor separatorColor] colorWithAlphaComponent:0.28] setStroke];
    shape.lineWidth = 0.75;
    [shape stroke];
    DrawBilibiliBLetter(NSMakeRect(point.x + 4.2, point.y + 2.0, 11.6, 13.0),
                        NSColor.alternateSelectedControlTextColor);
}

- (void)drawFollowerNumber {
    CGFloat lift = MAX(0, self.bounds.size.height - 226);
    NSString *number = self.hasFollowers ? DecimalStringForInteger(self.followers) : @"--";
    NSDictionary *numberAttrs = @{NSFontAttributeName: CachedMonospacedDigitFont(39, NSFontWeightSemibold),
                                  NSForegroundColorAttributeName: NSColor.controlAccentColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.24 blur:3.0 y:-1]};
    NSSize numberSize = [number sizeWithAttributes:numberAttrs];
    [number drawAtPoint:NSMakePoint((self.bounds.size.width - numberSize.width) / 2.0, 122 + lift) withAttributes:numberAttrs];

    if (self.hasFollowerDelta && self.followerDelta != 0) {
        NSString *delta = [NSString stringWithFormat:@"%@%ld", self.followerDelta > 0 ? @"+" : @"", (long)self.followerDelta];
        NSColor *color = self.followerDelta > 0
            ? NSColor.systemGreenColor
            : NSColor.systemRedColor;
        NSDictionary *deltaAttrs = @{NSFontAttributeName: CachedMonospacedDigitFont(13, NSFontWeightBold),
                                     NSForegroundColorAttributeName: color,
                                     NSShadowAttributeName: [self shadowWithAlpha:0.56 blur:3.2 y:-1]};
        [delta drawAtPoint:NSMakePoint((self.bounds.size.width + numberSize.width) / 2.0 + 8, 143 + lift) withAttributes:deltaAttrs];
    }

    NSDictionary *statusAttrs = @{NSFontAttributeName: CachedSystemFont(12, NSFontWeightSemibold),
                                  NSForegroundColorAttributeName: NSColor.secondaryLabelColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.18 blur:2.0 y:-1]};
    NSString *status = self.status.length > 0 ? self.status : @"等待刷新";
    [status drawAtPoint:NSMakePoint(24, 110 + lift) withAttributes:statusAttrs];

    NSDictionary *updatedAttrs = @{NSFontAttributeName: CachedSystemFont(12, NSFontWeightMedium),
                                   NSForegroundColorAttributeName: NSColor.tertiaryLabelColor,
                                   NSShadowAttributeName: [self shadowWithAlpha:0.16 blur:2.0 y:-1]};
    NSString *updated = self.updatedText.length > 0 ? self.updatedText : @"--";
    NSSize updatedSize = [updated sizeWithAttributes:updatedAttrs];
    [updated drawAtPoint:NSMakePoint(self.bounds.size.width - updatedSize.width - 24, 110 + lift) withAttributes:updatedAttrs];
}

- (void)drawSparklineInRect:(NSRect)rect {
    DrawFollowerSparkline(self.historyValues, rect, 3.0, 2.0, 0.70);
}

- (void)drawTrendSummaryInRect:(NSRect)rect {
    if (self.historyValues.count < 2) return;
    NSInteger first = self.historyValues.firstObject.integerValue;
    NSInteger last = self.historyValues.lastObject.integerValue;
    NSInteger delta = last - first;
    NSString *text = CompactSignedDelta(delta);
    NSDictionary *attrs = @{NSFontAttributeName: CachedMonospacedDigitFont(11, NSFontWeightSemibold),
                            NSForegroundColorAttributeName: FollowerDeltaColor(delta),
                            NSShadowAttributeName: [self shadowWithAlpha:0.16 blur:2 y:-1]};
    NSSize size = [text sizeWithAttributes:attrs];
    CGFloat x = NSMaxX(rect) - size.width;
    CGFloat y = NSMidY(rect) - size.height / 2.0 + 1.0;
    [text drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
}

- (void)drawActionBackplates {
    // Icon buttons sit directly on the single popover glass sheet.
}

- (void)drawQuickControlBackplates {
    // Segmented controls and checkboxes now own their visible surfaces; this keeps the popover to one glass layer.
}

- (NSShadow *)shadowWithAlpha:(CGFloat)alpha blur:(CGFloat)blur y:(CGFloat)y {
    return CachedTextShadow(alpha, blur, y);
}

- (void)refreshTapped:(id)sender {
    if (self.refreshing) return;
    if (self.refreshHandler) self.refreshHandler();
}
- (void)copyTapped:(id)sender {
    if (self.copyHandler) self.copyHandler();
}
- (void)profileTapped:(id)sender {
    if (self.profileHandler) self.profileHandler();
}
- (void)historyTapped:(id)sender {
    if (self.historyHandler) self.historyHandler();
}
- (void)notificationTapped:(id)sender {
    if (self.notificationHandler) self.notificationHandler(!self.notificationsEnabled);
}
- (void)launchAtLoginChanged:(id)sender {
    BOOL enabled = _launchAtLoginButton.state == NSControlStateValueOn;
    if (self.launchAtLoginHandler) self.launchAtLoginHandler(enabled);
}
- (void)settingsTapped:(id)sender {
    if (self.settingsHandler) self.settingsHandler();
}
- (void)showCardTapped:(id)sender {
    if (self.showCardHandler) self.showCardHandler();
}
- (void)quitTapped:(id)sender {
    if (self.quitHandler) self.quitHandler();
}
- (void)intervalChanged:(id)sender {
    if (!self.refreshIntervalHandler) return;
    NSInteger segment = _intervalControl.selectedSegment;
    NSTimeInterval interval = segment == 0 ? 60 : (segment == 1 ? 300 : 900);
    self.refreshIntervalHandler(interval);
}
- (void)statusDisplayChanged:(id)sender {
    if (!self.statusDisplayHandler) return;
    NSInteger segment = _statusDisplayControl.selectedSegment;
    NSInteger mode = segment == 2 ? StatusDisplayModeIconOnly : (segment == 1 ? StatusDisplayModeCompact : StatusDisplayModeExact);
    self.statusDisplayHandler(mode);
}
- (void)appearanceChanged:(id)sender {
    if (!self.appearanceHandler) return;
    NSInteger segment = _appearanceControl.selectedSegment;
    NSInteger mode = segment == 2 ? AppearanceModeReadable : (segment == 1 ? AppearanceModeClear : AppearanceModeUltraClear);
    self.appearanceHandler(mode);
}
- (void)autoRefreshChanged:(id)sender {
    BOOL enabled = _autoRefreshButton.state == NSControlStateValueOn;
    if (self.autoRefreshHandler) self.autoRefreshHandler(enabled);
}
- (void)menuBarOnlyChanged:(id)sender {
    BOOL enabled = _menuBarOnlyButton.state == NSControlStateValueOn;
    if (self.menuBarOnlyHandler) self.menuBarOnlyHandler(enabled);
}
@end
