// Shared constants, UI helpers, glass drawing, input parsing, and history helpers.
static NSString * const DefaultName = @"谐门东西";
static NSString * const UnknownTargetName = @"BILI粉丝数";
static long long const DefaultMID = 3546718146661176LL;
static NSTimeInterval const DefaultRefreshInterval = 60;
static NSUInteger const MaxHistoryTargets = 12;
static NSUInteger const MaxHistoryPointsPerTarget = 6000;
static NSUInteger const MaxSparklinePoints = 42;
static NSTimeInterval const OneWeekInterval = 7 * 24 * 60 * 60;
static CGFloat const MainWindowWidth = 900;
static CGFloat const MainWindowHeight = 640;
static NSString * const ShowMainWindowNotificationName = @"com.example.bilifanscard.showMainWindow";
static NSString * const TitlebarChromeOverlayIdentifier = @"com.example.bilifanscard.titlebarChromeOverlay";

typedef NS_ENUM(NSInteger, AppearanceMode) {
    AppearanceModeUltraClear = 0,
    AppearanceModeClear = 1,
    AppearanceModeReadable = 2,
};

typedef NS_ENUM(NSInteger, StatusDisplayMode) {
    StatusDisplayModeExact = 0,
    StatusDisplayModeCompact = 1,
    StatusDisplayModeIconOnly = 2,
};

typedef NS_ENUM(NSInteger, TrendRange) {
    TrendRangeSevenDays = 7,
    TrendRangeThirtyDays = 30,
    TrendRangeNinetyDays = 90,
};

@interface FanResult : NSObject
@property(nonatomic, copy) NSString *name;
@property(nonatomic) long long mid;
@property(nonatomic) NSInteger followers;
@property(nonatomic, strong) NSDate *updatedAt;
@end

@implementation FanResult
@end

@interface VerticallyCenteredTextFieldCell : NSTextFieldCell
@end

@implementation VerticallyCenteredTextFieldCell
- (NSRect)centeredTextRectForBounds:(NSRect)rect {
    NSRect textRect = [super drawingRectForBounds:rect];
    NSSize textSize = [self cellSizeForBounds:rect];
    CGFloat y = NSMinY(rect) + floor((rect.size.height - textSize.height) / 2.0) + 1.0;
    textRect.origin.y = MAX(NSMinY(rect), y);
    textRect.size.height = MIN(rect.size.height, textSize.height);
    return textRect;
}
- (NSRect)drawingRectForBounds:(NSRect)rect {
    return [self centeredTextRectForBounds:rect];
}
- (void)editWithFrame:(NSRect)rect inView:(NSView *)controlView editor:(NSText *)textObj delegate:(id)delegate event:(NSEvent *)event {
    [super editWithFrame:[self centeredTextRectForBounds:rect] inView:controlView editor:textObj delegate:delegate event:event];
}
- (void)selectWithFrame:(NSRect)rect inView:(NSView *)controlView editor:(NSText *)textObj delegate:(id)delegate start:(NSInteger)selStart length:(NSInteger)selLength {
    [super selectWithFrame:[self centeredTextRectForBounds:rect] inView:controlView editor:textObj delegate:delegate start:selStart length:selLength];
}
@end

@interface ChromePassthroughFillView : NSView
@end

@implementation ChromePassthroughFillView
- (NSView *)hitTest:(NSPoint)point {
    (void)point;
    return nil;
}
@end

@interface StatusGlassPanel : NSPanel
@end

@implementation StatusGlassPanel
- (BOOL)canBecomeKeyWindow {
    return NO;
}

- (BOOL)canBecomeMainWindow {
    return NO;
}
@end

static NSString *CleanName(NSString *value, long long mid) {
    (void)mid;
    NSString *trimmed = [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length == 0) {
        return UnknownTargetName;
    }
    return trimmed.length > 24 ? [trimmed substringToIndex:24] : trimmed;
}

static NSString *CompactFollowerCount(long long value) {
    double scaled = (double)value;
    NSString *suffix = @"";
    if (value >= 100000000LL) {
        scaled = value / 100000000.0;
        suffix = @"亿";
    } else if (value >= 10000LL) {
        scaled = value / 10000.0;
        suffix = @"万";
    }
    NSString *text = scaled >= 100 ? [NSString stringWithFormat:@"%.0f", scaled] : [NSString stringWithFormat:@"%.1f", scaled];
    if ([text hasSuffix:@".0"]) {
        text = [text substringToIndex:text.length - 2];
    }
    return [text stringByAppendingString:suffix];
}

static NSString *DecimalStringForInteger(long long value) {
    static NSNumberFormatter *formatter = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        formatter = [NSNumberFormatter new];
        formatter.numberStyle = NSNumberFormatterDecimalStyle;
    });
    return [formatter stringFromNumber:@(value)] ?: [NSString stringWithFormat:@"%lld", value];
}

static NSString *CompactSignedDelta(NSInteger delta) {
    if (delta == 0) {
        return @"持平";
    }
    long long magnitude = llabs((long long)delta);
    NSString *sign = delta > 0 ? @"+" : @"-";
    return [sign stringByAppendingString:CompactFollowerCount(magnitude)];
}

static NSColor *FollowerDeltaColor(NSInteger delta) {
    if (delta > 0) {
        return NSColor.systemGreenColor;
    }
    if (delta < 0) {
        return NSColor.systemRedColor;
    }
    return NSColor.tertiaryLabelColor;
}

static NSColor *SparklineTrendColor(NSInteger delta) {
    if (delta > 0) {
        return NSColor.systemGreenColor;
    }
    if (delta < 0) {
        return NSColor.systemRedColor;
    }
    return NSColor.controlAccentColor;
}

static void DrawFollowerSparkline(NSArray<NSNumber *> *values, NSRect rect, CGFloat verticalInset, CGFloat dotRadius, CGFloat dotAlpha) {
    if (values.count < 2) return;

    NSInteger minValue = NSIntegerMax;
    NSInteger maxValue = NSIntegerMin;
    for (NSNumber *value in values) {
        NSInteger followers = value.integerValue;
        minValue = MIN(minValue, followers);
        maxValue = MAX(maxValue, followers);
    }

    NSInteger delta = values.lastObject.integerValue - values.firstObject.integerValue;
    NSColor *trendColor = SparklineTrendColor(delta);
    CGFloat range = MAX(1.0, (CGFloat)(maxValue - minValue));
    CGFloat step = rect.size.width / (CGFloat)(values.count - 1);
    CGFloat drawableHeight = MAX(1.0, rect.size.height - verticalInset * 2.0);

    NSBezierPath *line = [NSBezierPath bezierPath];
    NSPoint lastPoint = NSZeroPoint;
    for (NSUInteger i = 0; i < values.count; i++) {
        CGFloat x = NSMinX(rect) + step * (CGFloat)i;
        CGFloat normalized = ((CGFloat)values[i].integerValue - (CGFloat)minValue) / range;
        CGFloat y = NSMinY(rect) + verticalInset + normalized * drawableHeight;
        lastPoint = NSMakePoint(x, y);
        if (i == 0) {
            [line moveToPoint:lastPoint];
        } else {
            [line lineToPoint:lastPoint];
        }
    }

    NSBezierPath *area = [line copy];
    [area lineToPoint:NSMakePoint(NSMaxX(rect), NSMinY(rect) + 1)];
    [area lineToPoint:NSMakePoint(NSMinX(rect), NSMinY(rect) + 1)];
    [area closePath];
    [[trendColor colorWithAlphaComponent:0.075] setFill];
    [area fill];

    line.lineWidth = 1.15;
    line.lineJoinStyle = NSLineJoinStyleRound;
    line.lineCapStyle = NSLineCapStyleRound;
    [[trendColor colorWithAlphaComponent:0.50] setStroke];
    [line stroke];

    NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(lastPoint.x - dotRadius, lastPoint.y - dotRadius, dotRadius * 2.0, dotRadius * 2.0)];
    [[trendColor colorWithAlphaComponent:dotAlpha] setFill];
    [dot fill];
}

static NSShadow *CachedTextShadow(CGFloat alpha, CGFloat blur, CGFloat y) {
    static NSMutableDictionary<NSString *, NSShadow *> *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [NSMutableDictionary new];
    });

    NSString *key = [NSString stringWithFormat:@"%.3f|%.3f|%.3f", alpha, blur, y];
    NSShadow *shadow = cache[key];
    if (shadow) return shadow;

    shadow = [NSShadow new];
    shadow.shadowColor = [[NSColor shadowColor] colorWithAlphaComponent:alpha];
    shadow.shadowBlurRadius = blur;
    shadow.shadowOffset = NSMakeSize(0, y);
    cache[key] = shadow;
    return shadow;
}

static NSFont *CachedSystemFont(CGFloat size, NSFontWeight weight) {
    static NSMutableDictionary<NSString *, NSFont *> *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [NSMutableDictionary new];
    });

    NSString *key = [NSString stringWithFormat:@"%.2f|%.3f", size, weight];
    NSFont *font = cache[key];
    if (font) return font;

    font = [NSFont systemFontOfSize:size weight:weight];
    cache[key] = font;
    return font;
}

static NSFont *CachedMonospacedDigitFont(CGFloat size, NSFontWeight weight) {
    static NSMutableDictionary<NSString *, NSFont *> *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [NSMutableDictionary new];
    });

    NSString *key = [NSString stringWithFormat:@"%.2f|%.3f", size, weight];
    NSFont *font = cache[key];
    if (font) return font;

    font = [NSFont monospacedDigitSystemFontOfSize:size weight:weight];
    cache[key] = font;
    return font;
}

static CGFloat ClampUnit(CGFloat value) {
    return MIN(MAX(value, 0.0), 1.0);
}

static BOOL sAppAppearanceIsDarkCacheValid = NO;
static BOOL sAppAppearanceIsDarkCachedValue = NO;

static BOOL ResolveAppAppearanceIsDark(void) {
    NSString *interfaceStyle = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    if (interfaceStyle.length == 0) {
        interfaceStyle = [[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain][@"AppleInterfaceStyle"];
    }
    if ([interfaceStyle isKindOfClass:NSString.class] && interfaceStyle.length > 0) {
        return [interfaceStyle caseInsensitiveCompare:@"Dark"] == NSOrderedSame;
    }
    return NO;
}

static BOOL AppAppearanceIsDark(void) {
    if (!sAppAppearanceIsDarkCacheValid) {
        sAppAppearanceIsDarkCachedValue = ResolveAppAppearanceIsDark();
        sAppAppearanceIsDarkCacheValid = YES;
    }
    return sAppAppearanceIsDarkCachedValue;
}

static void InvalidateAppAppearanceCache(void) {
    sAppAppearanceIsDarkCacheValid = NO;
}

static void SetTextFieldStringIfChanged(NSTextField *field, NSString *value) {
    if (!field) return;
    NSString *next = value ?: @"";
    if (![field.stringValue isEqualToString:next]) {
        field.stringValue = next;
    }
}

static void SetTextFieldColorIfChanged(NSTextField *field, NSColor *color) {
    if (!field || !color) return;
    if (![field.textColor isEqual:color]) {
        field.textColor = color;
    }
}

static void SetButtonTitleIfChanged(NSButton *button, NSString *title) {
    if (!button) return;
    NSString *next = title ?: @"";
    if (![button.title isEqualToString:next]) {
        button.title = next;
    }
}

static void SetMenuItemTitleIfChanged(NSMenuItem *item, NSString *title) {
    if (!item) return;
    NSString *next = title ?: @"";
    if (![item.title isEqualToString:next]) {
        item.title = next;
    }
}

static void SetMenuItemStateIfChanged(NSMenuItem *item, NSControlStateValue state) {
    if (!item) return;
    if (item.state != state) {
        item.state = state;
    }
}

static void SetViewToolTipIfChanged(NSView *view, NSString *toolTip) {
    if (!view) return;
    NSString *current = view.toolTip;
    if ((current || toolTip) && ![current isEqualToString:toolTip]) {
        view.toolTip = toolTip;
    }
}

static void SetControlEnabledIfChanged(NSControl *control, BOOL enabled) {
    if (!control) return;
    if (control.enabled != enabled) {
        control.enabled = enabled;
    }
}

static void SetButtonStateIfChanged(NSButton *button, NSControlStateValue state) {
    if (!button) return;
    if (button.state != state) {
        button.state = state;
    }
}

static void SetViewAlphaIfChanged(NSView *view, CGFloat alpha) {
    if (!view) return;
    if (fabs(view.alphaValue - alpha) > 0.001) {
        view.alphaValue = alpha;
    }
}

static void SetViewHiddenIfChanged(NSView *view, BOOL hidden) {
    if (!view) return;
    if (view.hidden != hidden) {
        view.hidden = hidden;
    }
}

static void SetSegmentedControlSelectedSegmentIfChanged(NSSegmentedControl *control, NSInteger selectedSegment) {
    if (!control) return;
    if (control.selectedSegment != selectedSegment) {
        control.selectedSegment = selectedSegment;
    }
}

static void SetButtonContentTintColorIfChanged(NSButton *button, NSColor *color) {
    if (!button || !color) return;
    if (![button.contentTintColor isEqual:color]) {
        button.contentTintColor = color;
    }
}

static NSImage *TemplateSystemSymbolImage(NSString *symbolName, NSString *accessibilityDescription) {
    if (symbolName.length == 0) return nil;
    static NSMutableDictionary<NSString *, NSImage *> *cache = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        cache = [NSMutableDictionary new];
    });
    NSString *key = [NSString stringWithFormat:@"%@\n%@", symbolName, accessibilityDescription ?: @""];
    NSImage *cached = cache[key];
    if (cached) return cached;
    NSImage *image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:accessibilityDescription];
    if (!image) return nil;
    image.template = YES;
    cache[key] = image;
    return image;
}

static void MarkViewNeedsDisplay(NSView *view) {
    if (view) {
        view.needsDisplay = YES;
    }
}

static NSColor *WindowChromeBackgroundColor(void) {
    return AppAppearanceIsDark()
        ? [NSColor colorWithCalibratedRed:0.105 green:0.100 blue:0.102 alpha:1.0]
        : [NSColor colorWithCalibratedRed:0.965 green:0.972 blue:0.982 alpha:1.0];
}

static NSColor *MainWindowTitlebarUnderlayColor(void) {
    return NSColor.clearColor;
}

static NSAppearance *WindowChromeAppearance(void) {
    return [NSAppearance appearanceNamed:AppAppearanceIsDark() ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

static CGColorRef LayerColorForNSColor(NSColor *color) {
    NSColor *rgb = [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace] ?: color;
    return rgb.CGColor;
}

static BOOL ApplyChromeFillToView(NSView *view, NSAppearance *appearance, CGColorRef backgroundColor) {
    if (!view) return NO;
    BOOL changed = NO;
    if (view.appearance != appearance) {
        view.appearance = appearance;
        changed = YES;
    }
    if (!view.wantsLayer) {
        view.wantsLayer = YES;
        changed = YES;
    }
    if ([view isKindOfClass:NSVisualEffectView.class]) {
        NSVisualEffectView *effectView = (NSVisualEffectView *)view;
        if (effectView.material != NSVisualEffectMaterialWindowBackground) {
            effectView.material = NSVisualEffectMaterialWindowBackground;
            changed = YES;
        }
        if (effectView.blendingMode != NSVisualEffectBlendingModeWithinWindow) {
            effectView.blendingMode = NSVisualEffectBlendingModeWithinWindow;
            changed = YES;
        }
        if (effectView.state != NSVisualEffectStateActive) {
            effectView.state = NSVisualEffectStateActive;
            changed = YES;
        }
    }
    if (!view.layer.backgroundColor || !CGColorEqualToColor(view.layer.backgroundColor, backgroundColor)) {
        view.layer.backgroundColor = backgroundColor;
        changed = YES;
    }
    if (changed) {
        view.needsDisplay = YES;
    }
    return changed;
}

static BOOL ViewIsStandardWindowButton(NSView *view, NSView *closeButton, NSView *miniaturizeButton, NSView *zoomButton) {
    return view == closeButton || view == miniaturizeButton || view == zoomButton;
}

static BOOL ApplyChromeFillToFrameTree(NSView *view,
                                       NSView *contentView,
                                       NSView *closeButton,
                                       NSView *miniaturizeButton,
                                       NSView *zoomButton,
                                       NSAppearance *appearance,
                                       CGColorRef backgroundColor) {
    if (!view || view == contentView || ViewIsStandardWindowButton(view, closeButton, miniaturizeButton, zoomButton)) {
        return NO;
    }
    BOOL changed = ApplyChromeFillToView(view, appearance, backgroundColor);
    for (NSView *subview in [view.subviews copy]) {
        changed = ApplyChromeFillToFrameTree(subview,
                                             contentView,
                                             closeButton,
                                             miniaturizeButton,
                                             zoomButton,
                                             appearance,
                                             backgroundColor) || changed;
    }
    return changed;
}

static ChromePassthroughFillView *TitlebarChromeOverlayForView(NSView *titlebarView) {
    if (!titlebarView) return nil;
    for (NSView *subview in [titlebarView.subviews copy]) {
        if ([subview.identifier isEqualToString:TitlebarChromeOverlayIdentifier] &&
            [subview isKindOfClass:ChromePassthroughFillView.class]) {
            if (subview.superview == titlebarView) {
                [subview removeFromSuperview];
                [titlebarView addSubview:subview positioned:NSWindowBelow relativeTo:nil];
            }
            return (ChromePassthroughFillView *)subview;
        }
    }

    ChromePassthroughFillView *overlay = [[ChromePassthroughFillView alloc] initWithFrame:titlebarView.bounds];
    overlay.identifier = TitlebarChromeOverlayIdentifier;
    overlay.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    overlay.wantsLayer = YES;
    [titlebarView addSubview:overlay positioned:NSWindowBelow relativeTo:nil];
    return overlay;
}

static BOOL ApplyTitlebarChromeOverlay(NSView *titlebarView, NSAppearance *appearance, CGColorRef backgroundColor) {
    ChromePassthroughFillView *overlay = TitlebarChromeOverlayForView(titlebarView);
    if (!overlay) return NO;
    overlay.frame = titlebarView.bounds;
    BOOL changed = ApplyChromeFillToView(overlay, appearance, backgroundColor);
    if (!overlay.layer.backgroundColor || !CGColorEqualToColor(overlay.layer.backgroundColor, backgroundColor)) {
        overlay.layer.backgroundColor = backgroundColor;
        changed = YES;
    }
    return changed;
}

static BOOL ApplyMainWindowTitlebarUnderlay(NSView *titlebarView, NSView *frameView, NSAppearance *appearance) {
    (void)frameView;
    ChromePassthroughFillView *overlay = TitlebarChromeOverlayForView(titlebarView);
    if (!overlay) return NO;

    BOOL changed = NO;
    overlay.frame = titlebarView.bounds;
    if (!overlay.wantsLayer) {
        overlay.wantsLayer = YES;
        changed = YES;
    }
    if (overlay.appearance != appearance) {
        overlay.appearance = appearance;
        changed = YES;
    }
    overlay.layer.masksToBounds = NO;
    overlay.layer.cornerRadius = 0.0;

    CGColorRef underlayColor = LayerColorForNSColor(MainWindowTitlebarUnderlayColor());
    if (!overlay.layer.backgroundColor || !CGColorEqualToColor(overlay.layer.backgroundColor, underlayColor)) {
        overlay.layer.backgroundColor = underlayColor;
        changed = YES;
    }
    if (changed) {
        overlay.needsDisplay = YES;
    }
    return changed;
}

static void ApplyWindowChrome(NSWindow *window) {
    if (!window) return;
    BOOL isMainWindow = [window.identifier isEqualToString:@"BILIFansCardMainWindow"];
    NSAppearance *appearance = WindowChromeAppearance();
    NSColor *background = isMainWindow ? NSColor.clearColor : WindowChromeBackgroundColor();
    CGColorRef backgroundColor = LayerColorForNSColor(background);
    BOOL chromeChanged = NO;
    if (NSApp.appearance != appearance) {
        NSApp.appearance = appearance;
    }
    if (window.appearance != appearance) {
        window.appearance = appearance;
        chromeChanged = YES;
    }
    if (![window.backgroundColor isEqual:background]) {
        window.backgroundColor = background;
        chromeChanged = YES;
    }
    if (window.opaque) {
        window.opaque = NO;
        chromeChanged = YES;
    }
    NSWindowTitleVisibility desiredTitleVisibility = NSWindowTitleHidden;
    if (window.titleVisibility != desiredTitleVisibility) {
        window.titleVisibility = desiredTitleVisibility;
        chromeChanged = YES;
    }
    BOOL wantsTransparentTitlebar = YES;
    if (window.titlebarAppearsTransparent != wantsTransparentTitlebar) {
        window.titlebarAppearsTransparent = wantsTransparentTitlebar;
        chromeChanged = YES;
    }
    BOOL wantsFullSizeContent = isMainWindow;
    if (wantsFullSizeContent && (window.styleMask & NSWindowStyleMaskFullSizeContentView) == 0) {
        window.styleMask = window.styleMask | NSWindowStyleMaskFullSizeContentView;
        chromeChanged = YES;
    } else if (!wantsFullSizeContent && (window.styleMask & NSWindowStyleMaskFullSizeContentView) != 0) {
        window.styleMask = window.styleMask & ~NSWindowStyleMaskFullSizeContentView;
        chromeChanged = YES;
    }
    if (@available(macOS 11.0, *)) {
        if (window.titlebarSeparatorStyle != NSTitlebarSeparatorStyleNone) {
            window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
            chromeChanged = YES;
        }
    }

    NSView *contentView = window.contentView;
    NSButton *closeButton = [window standardWindowButton:NSWindowCloseButton];
    NSButton *miniaturizeButton = [window standardWindowButton:NSWindowMiniaturizeButton];
    NSButton *zoomButton = [window standardWindowButton:NSWindowZoomButton];
    if (contentView) {
        if (contentView.appearance != appearance) {
            contentView.appearance = appearance;
            chromeChanged = YES;
        }
        if (!contentView.wantsLayer) {
            contentView.wantsLayer = YES;
            chromeChanged = YES;
        }
        CGColorRef clearColor = NSColor.clearColor.CGColor;
        if (!contentView.layer.backgroundColor || !CGColorEqualToColor(contentView.layer.backgroundColor, clearColor)) {
            contentView.layer.backgroundColor = clearColor;
            chromeChanged = YES;
        }
    }

    if (isMainWindow) {
        NSView *titlebarView = closeButton.superview;
        chromeChanged = ApplyMainWindowTitlebarUnderlay(titlebarView, contentView.superview, appearance) || chromeChanged;
        NSButton *standardButtons[] = { closeButton, miniaturizeButton, zoomButton };
        for (NSUInteger i = 0; i < sizeof(standardButtons) / sizeof(standardButtons[0]); i++) {
            NSButton *button = standardButtons[i];
            if (!button) continue;
            if (button.hidden) {
                button.hidden = NO;
                chromeChanged = YES;
            }
            if (fabs(button.alphaValue - 1.0) > 0.001) {
                button.alphaValue = 1.0;
                chromeChanged = YES;
            }
        }
        if (chromeChanged) {
            [contentView.superview displayIfNeeded];
        }
        return;
    }

    NSView *frameView = contentView.superview;
    if (frameView) {
        chromeChanged = ApplyChromeFillToFrameTree(frameView,
                                                   contentView,
                                                   closeButton,
                                                   miniaturizeButton,
                                                   zoomButton,
                                                   appearance,
                                                   backgroundColor) || chromeChanged;
    }

    NSView *titlebarView = closeButton.superview;
    chromeChanged = ApplyTitlebarChromeOverlay(titlebarView, appearance, backgroundColor) || chromeChanged;
    NSButton *standardButtons[] = { closeButton, miniaturizeButton, zoomButton };
    for (NSUInteger i = 0; i < sizeof(standardButtons) / sizeof(standardButtons[0]); i++) {
        NSButton *button = standardButtons[i];
        if (!button) continue;
        if (button.hidden) {
            button.hidden = NO;
            chromeChanged = YES;
        }
        if (fabs(button.alphaValue - 1.0) > 0.001) {
            button.alphaValue = 1.0;
            chromeChanged = YES;
        }
        if (button.superview) {
            [button.superview addSubview:button positioned:NSWindowAbove relativeTo:nil];
        }
    }
    while (titlebarView && titlebarView != contentView) {
        chromeChanged = ApplyChromeFillToView(titlebarView, appearance, backgroundColor) || chromeChanged;
        if (titlebarView == frameView) {
            break;
        }
        titlebarView = titlebarView.superview;
    }
    if (chromeChanged) {
        [frameView displayIfNeeded];
    }
}

static void ApplyWindowChromeAfterSystemTitlebarUpdate(NSWindow *window) {
    if (!window) return;
    __weak NSWindow *weakWindow = window;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        ApplyWindowChrome(weakWindow);
    });
}

static BOOL SystemPrefersReducedTransparency(void) {
    return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldReduceTransparency;
}

static NSInteger EffectiveAppearanceMode(NSInteger mode) {
    if (SystemPrefersReducedTransparency()) {
        return AppearanceModeReadable;
    }
    if (mode == AppearanceModeReadable || mode == AppearanceModeClear) {
        return mode;
    }
    return AppearanceModeUltraClear;
}

static void DrawLiquidGlassRim(NSRect rect, CGFloat radius, NSColor *accent, CGFloat strength) {
    CGFloat clamped = ClampUnit(strength);
    BOOL dark = AppAppearanceIsDark();
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];

    [NSGraphicsContext saveGraphicsState];
    [shape addClip];

    NSGradient *topSheen = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.118 : 0.28) * clamped],
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.044 : 0.104) * clamped],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topSheen drawInRect:rect angle:90];

    NSGradient *lowerDepth = [[NSGradient alloc] initWithColors:@[
        [[NSColor blackColor] colorWithAlphaComponent:0.0],
        [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.016 : 0.018) * clamped],
        [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.038 : 0.044) * clamped]
    ]];
    [lowerDepth drawInRect:rect angle:-90];

    CGFloat edgeWidth = MIN(30.0, rect.size.width * 0.14);
    NSGradient *leftRefraction = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:0.0],
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.095 : 0.130) * clamped],
        [accent colorWithAlphaComponent:(dark ? 0.052 : 0.030) * clamped]
    ]];
    [leftRefraction drawInRect:NSMakeRect(NSMinX(rect), NSMinY(rect), edgeWidth, rect.size.height) angle:0];

    NSGradient *rightRefraction = [[NSGradient alloc] initWithColors:@[
        [accent colorWithAlphaComponent:(dark ? 0.060 : 0.040) * clamped],
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.082 : 0.118) * clamped],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [rightRefraction drawInRect:NSMakeRect(NSMaxX(rect) - edgeWidth, NSMinY(rect), edgeWidth, rect.size.height) angle:0];

    NSBezierPath *upperLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(rect) - rect.size.width * 0.18,
                                                                                NSMaxY(rect) - rect.size.height * 0.40,
                                                                                rect.size.width * 0.58,
                                                                                rect.size.height * 0.38)];
    NSGradient *upperGlow = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.128 * clamped]
                                                          endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [upperGlow drawInBezierPath:upperLens relativeCenterPosition:NSMakePoint(-0.22, 0.10)];

    NSRect lensRect = NSMakeRect(NSMaxX(rect) - rect.size.width * 0.42,
                                 NSMinY(rect) - rect.size.height * 0.20,
                                 rect.size.width * 0.52,
                                 rect.size.height * 0.54);
    NSBezierPath *lens = [NSBezierPath bezierPathWithOvalInRect:lensRect];
    NSGradient *accentLens = [[NSGradient alloc] initWithStartingColor:[accent colorWithAlphaComponent:0.066 * clamped]
                                                           endingColor:[accent colorWithAlphaComponent:0.0]];
    [accentLens drawInBezierPath:lens relativeCenterPosition:NSMakePoint(0.22, -0.14)];

    [NSGraphicsContext restoreGraphicsState];

    NSBezierPath *outer = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 0.4, 0.4) xRadius:radius yRadius:radius];
    [[[NSColor separatorColor] colorWithAlphaComponent:(dark ? 0.072 : 0.082) * clamped] setStroke];
    outer.lineWidth = 0.8;
    [outer stroke];

    NSBezierPath *inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 1.1, 1.1) xRadius:MAX(2, radius - 1.1) yRadius:MAX(2, radius - 1.1)];
    [[[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.078 : 0.24) * clamped] setStroke];
    inner.lineWidth = 0.75;
    [inner stroke];

    NSBezierPath *depth = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 2.0, 2.0) xRadius:MAX(2, radius - 2.0) yRadius:MAX(2, radius - 2.0)];
    [[[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.038 : 0.036) * clamped] setStroke];
    depth.lineWidth = 0.55;
    [depth stroke];
}

static CGFloat LiquidGlassAlphaForMode(NSInteger mode, CGFloat readable, CGFloat clear, CGFloat ultraClear) {
    mode = EffectiveAppearanceMode(mode);
    if (mode == AppearanceModeReadable) {
        return readable;
    }
    if (mode == AppearanceModeClear) {
        return clear;
    }
    return ultraClear;
}

static NSFont *RoundedBFont(CGFloat size) {
    NSFont *base = [NSFont systemFontOfSize:size weight:NSFontWeightHeavy];
    if (@available(macOS 10.15, *)) {
        NSFontDescriptor *rounded = [base.fontDescriptor fontDescriptorWithDesign:NSFontDescriptorSystemDesignRounded];
        NSFont *font = rounded ? [NSFont fontWithDescriptor:rounded size:size] : nil;
        if (font) {
            return font;
        }
    }
    return base;
}

static void DrawBilibiliBLetter(NSRect rect, NSColor *color) {
    if (rect.size.width <= 0 || rect.size.height <= 0) {
        return;
    }
    NSString *letter = @"B";
    NSFont *font = RoundedBFont(rect.size.height * 1.02);
    NSDictionary *attrs = @{NSFontAttributeName: font,
                            NSForegroundColorAttributeName: color};
    NSSize size = [letter sizeWithAttributes:attrs];
    CGFloat x = NSMidX(rect) - size.width / 2.0 - rect.size.width * 0.015;
    CGFloat y = NSMidY(rect) - size.height / 2.0 - rect.size.height * 0.035;
    [letter drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
}

static void DrawFloatingGlassShadow(NSRect rect, CGFloat radius, NSInteger appearanceMode, CGFloat strength) {
    CGFloat scale = MAX(0.0, strength);
    CGFloat ambientAlpha = LiquidGlassAlphaForMode(appearanceMode, 0.20, 0.15, 0.11) * scale;
    CGFloat closeAlpha = LiquidGlassAlphaForMode(appearanceMode, 0.12, 0.09, 0.065) * scale;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 1.0, 1.0)
                                                          xRadius:MAX(2, radius - 1.0)
                                                          yRadius:MAX(2, radius - 1.0)];

    [NSGraphicsContext saveGraphicsState];
    NSShadow *ambient = [NSShadow new];
    ambient.shadowColor = [[NSColor shadowColor] colorWithAlphaComponent:ambientAlpha];
    ambient.shadowBlurRadius = 24.0 * scale;
    ambient.shadowOffset = NSMakeSize(0, -9.0 * scale);
    [ambient set];
    [[[NSColor blackColor] colorWithAlphaComponent:0.006 * scale] setFill];
    [shape fill];
    [NSGraphicsContext restoreGraphicsState];

    [NSGraphicsContext saveGraphicsState];
    NSShadow *close = [NSShadow new];
    close.shadowColor = [[NSColor shadowColor] colorWithAlphaComponent:closeAlpha];
    close.shadowBlurRadius = 8.0 * scale;
    close.shadowOffset = NSMakeSize(0, -2.0 * scale);
    [close set];
    [[[NSColor blackColor] colorWithAlphaComponent:0.005 * scale] setFill];
    [shape fill];
    [NSGraphicsContext restoreGraphicsState];
}

static void DrawLiquidGlassOpticalOverlay(NSRect rect, CGFloat radius, NSColor *accent, NSInteger appearanceMode, CGFloat intensity) {
    CGFloat scale = MAX(0.0, intensity);
    BOOL dark = AppAppearanceIsDark();
    NSInteger effectiveMode = EffectiveAppearanceMode(appearanceMode);
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL clear = effectiveMode == AppearanceModeClear;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];

    [NSGraphicsContext saveGraphicsState];
    [shape addClip];

    CGFloat topAlpha = (dark
        ? (readable ? 0.070 : (clear ? 0.046 : 0.028))
        : (readable ? 0.125 : (clear ? 0.080 : 0.046))) * scale;
    CGFloat sideAlpha = (dark
        ? (readable ? 0.050 : (clear ? 0.032 : 0.020))
        : (readable ? 0.086 : (clear ? 0.054 : 0.032))) * scale;
    CGFloat accentAlpha = (dark
        ? (readable ? 0.036 : (clear ? 0.022 : 0.014))
        : (readable ? 0.040 : (clear ? 0.024 : 0.014))) * scale;
    CGFloat depthAlpha = (dark
        ? (readable ? 0.040 : (clear ? 0.026 : 0.016))
        : (readable ? 0.020 : (clear ? 0.012 : 0.006))) * scale;

    NSGradient *topSheen = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:topAlpha],
        [[NSColor whiteColor] colorWithAlphaComponent:topAlpha * 0.26],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topSheen drawInRect:rect angle:90];

    NSBezierPath *leftLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(rect) - rect.size.width * 0.18,
                                                                                NSMaxY(rect) - rect.size.height * 0.54,
                                                                                rect.size.width * 0.72,
                                                                                rect.size.height * 0.58)];
    NSGradient *leftGlow = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:sideAlpha]
                                                         endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [leftGlow drawInBezierPath:leftLens relativeCenterPosition:NSMakePoint(-0.24, 0.16)];

    NSBezierPath *accentLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMaxX(rect) - rect.size.width * 0.48,
                                                                                  NSMinY(rect) - rect.size.height * 0.20,
                                                                                  rect.size.width * 0.62,
                                                                                  rect.size.height * 0.62)];
    NSGradient *accentGlow = [[NSGradient alloc] initWithStartingColor:[accent colorWithAlphaComponent:accentAlpha]
                                                           endingColor:[accent colorWithAlphaComponent:0.0]];
    [accentGlow drawInBezierPath:accentLens relativeCenterPosition:NSMakePoint(0.22, -0.12)];

    NSGradient *depth = [[NSGradient alloc] initWithStartingColor:[[NSColor blackColor] colorWithAlphaComponent:0.0]
                                                     endingColor:[[NSColor blackColor] colorWithAlphaComponent:depthAlpha]];
    [depth drawInRect:rect angle:-90];

    [NSGraphicsContext restoreGraphicsState];
}

static void DrawLiquidGlassControlWell(NSRect rect, CGFloat radius, NSColor *accent, NSInteger appearanceMode, BOOL emphasized) {
    BOOL dark = AppAppearanceIsDark();
    NSInteger effectiveMode = EffectiveAppearanceMode(appearanceMode);
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL clear = effectiveMode == AppearanceModeClear;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
    CGFloat fillAlpha = dark
        ? (emphasized ? (readable ? 0.070 : (clear ? 0.050 : 0.034)) : (readable ? 0.030 : (clear ? 0.018 : 0.010)))
        : (emphasized ? (readable ? 0.100 : (clear ? 0.068 : 0.044)) : (readable ? 0.052 : (clear ? 0.032 : 0.018)));
    [[(emphasized ? accent : NSColor.whiteColor) colorWithAlphaComponent:fillAlpha] setFill];
    [shape fill];
    DrawLiquidGlassOpticalOverlay(rect, radius, accent, appearanceMode, emphasized ? 0.62 : 0.36);
    DrawLiquidGlassRim(rect, radius, accent, emphasized ? (dark ? 0.46 : 0.44) : (dark ? 0.18 : 0.28));
}

static void DrawStatusGlassEdgeHighlight(NSRect rect, CGFloat radius, NSInteger appearanceMode) {
    BOOL dark = AppAppearanceIsDark();
    CGFloat strength = LiquidGlassAlphaForMode(appearanceMode, 1.0, 0.84, 0.68);
    NSRect outerRect = NSInsetRect(rect, 0.70, 0.70);
    NSBezierPath *outer = [NSBezierPath bezierPathWithRoundedRect:outerRect
                                                          xRadius:MAX(2.0, radius - 0.70)
                                                          yRadius:MAX(2.0, radius - 0.70)];

    [[[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.30 : 0.16) * strength] setStroke];
    outer.lineWidth = 1.55;
    [outer stroke];

    [[[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.34 : 0.44) * strength] setStroke];
    outer.lineWidth = 0.85;
    [outer stroke];

    NSBezierPath *inner = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(rect, 1.55, 1.55)
                                                          xRadius:MAX(2.0, radius - 1.55)
                                                          yRadius:MAX(2.0, radius - 1.55)];
    [[[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.125 : 0.24) * strength] setStroke];
    inner.lineWidth = 0.75;
    [inner stroke];

    [NSGraphicsContext saveGraphicsState];
    [outer addClip];
    NSGradient *topGlint = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.18 : 0.28) * strength],
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.045 : 0.080) * strength],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topGlint drawInRect:NSMakeRect(NSMinX(rect), NSMaxY(rect) - 54.0, NSWidth(rect), 54.0) angle:90];
    NSGradient *leftGlint = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.10 : 0.16) * strength]
                                                          endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [leftGlint drawInRect:NSMakeRect(NSMinX(rect), NSMinY(rect), 72.0, NSHeight(rect)) angle:0];
    [NSGraphicsContext restoreGraphicsState];
}

static void DrawStatusPopoverGlassBackdrop(NSRect rect, CGFloat radius, NSInteger appearanceMode) {
    BOOL dark = AppAppearanceIsDark();
    NSInteger effectiveMode = EffectiveAppearanceMode(appearanceMode);
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL clear = effectiveMode == AppearanceModeClear;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];

    [NSGraphicsContext saveGraphicsState];
    [shape addClip];

    CGFloat baseAlpha = dark
        ? (readable ? 0.30 : (clear ? 0.18 : 0.12))
        : (readable ? 0.22 : (clear ? 0.13 : 0.080));
    NSColor *baseColor = dark
        ? [NSColor colorWithCalibratedRed:0.055 green:0.065 blue:0.078 alpha:baseAlpha]
        : [[NSColor whiteColor] colorWithAlphaComponent:baseAlpha];
    [baseColor setFill];
    [shape fill];

    CGFloat topAlpha = dark
        ? (readable ? 0.075 : (clear ? 0.048 : 0.030))
        : (readable ? 0.150 : (clear ? 0.092 : 0.058));
    CGFloat sideAlpha = dark
        ? (readable ? 0.036 : (clear ? 0.024 : 0.016))
        : (readable ? 0.082 : (clear ? 0.050 : 0.032));
    CGFloat depthAlpha = dark
        ? (readable ? 0.042 : (clear ? 0.026 : 0.016))
        : (readable ? 0.018 : (clear ? 0.010 : 0.006));

    NSGradient *topBloom = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:topAlpha],
        [[NSColor whiteColor] colorWithAlphaComponent:topAlpha * 0.28],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topBloom drawInRect:rect angle:90];

    NSBezierPath *leftLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(rect) - rect.size.width * 0.26,
                                                                                NSMaxY(rect) - rect.size.height * 0.64,
                                                                                rect.size.width * 0.72,
                                                                                rect.size.height * 0.78)];
    NSGradient *leftGlow = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:sideAlpha]
                                                         endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [leftGlow drawInBezierPath:leftLens relativeCenterPosition:NSMakePoint(-0.20, 0.16)];

    NSGradient *floorShade = [[NSGradient alloc] initWithStartingColor:[[NSColor blackColor] colorWithAlphaComponent:0.0]
                                                           endingColor:[[NSColor blackColor] colorWithAlphaComponent:depthAlpha]];
    [floorShade drawInRect:rect angle:-90];

    [NSGraphicsContext restoreGraphicsState];
}

static void DrawStatusPopoverGlassRim(NSRect rect, CGFloat radius, NSInteger appearanceMode) {
    NSInteger effectiveMode = EffectiveAppearanceMode(appearanceMode);
    BOOL dark = AppAppearanceIsDark();
    BOOL readable = effectiveMode == AppearanceModeReadable;
    BOOL clear = effectiveMode == AppearanceModeClear;
    CGFloat highlightAlpha = dark
        ? (readable ? 0.18 : (clear ? 0.125 : 0.075))
        : (readable ? 0.20 : (clear ? 0.140 : 0.080));
    CGFloat liftAlpha = dark
        ? (readable ? 0.070 : (clear ? 0.045 : 0.026))
        : (readable ? 0.095 : (clear ? 0.060 : 0.034));

    NSRect innerRect = NSInsetRect(rect, 2.0, 2.0);
    NSBezierPath *clip = [NSBezierPath bezierPathWithRoundedRect:innerRect
                                                         xRadius:MAX(2, radius - 2.0)
                                                         yRadius:MAX(2, radius - 2.0)];
    [NSGraphicsContext saveGraphicsState];
    [clip addClip];

    NSGradient *topSheen = [[NSGradient alloc] initWithColors:@[
        [[NSColor whiteColor] colorWithAlphaComponent:highlightAlpha],
        [[NSColor whiteColor] colorWithAlphaComponent:highlightAlpha * 0.18],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topSheen drawInRect:NSMakeRect(NSMinX(innerRect) + 4,
                                    NSMaxY(innerRect) - 13,
                                    innerRect.size.width - 8,
                                    12)
                   angle:-90];

    NSBezierPath *leftLift = [NSBezierPath bezierPath];
    [leftLift moveToPoint:NSMakePoint(NSMinX(innerRect) + 1.0, NSMaxY(innerRect) - radius - 2.0)];
    [leftLift lineToPoint:NSMakePoint(NSMinX(innerRect) + 1.0, NSMidY(innerRect) + 8.0)];
    [[[NSColor whiteColor] colorWithAlphaComponent:liftAlpha] setStroke];
    leftLift.lineWidth = 0.65;
    leftLift.lineCapStyle = NSLineCapStyleRound;
    [leftLift stroke];

    [NSGraphicsContext restoreGraphicsState];
}

static NSString *RefreshErrorText(NSError *error) {
    if (!error) {
        return @"网络异常";
    }
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorTimedOut:
                return @"请求超时";
            case NSURLErrorNotConnectedToInternet:
                return @"未联网";
            case NSURLErrorNetworkConnectionLost:
                return @"网络中断";
            case NSURLErrorCannotFindHost:
            case NSURLErrorCannotConnectToHost:
            case NSURLErrorDNSLookupFailed:
                return @"连接失败";
            default:
                return @"网络异常";
        }
    }
    NSString *message = [error.localizedDescription stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (message.length == 0) {
        return @"刷新异常";
    }
    return message.length > 18 ? [message substringToIndex:18] : message;
}

static NSString *StatusWithRefreshError(NSString *prefix, NSError *error) {
    NSString *reason = RefreshErrorText(error);
    if (reason.length == 0) {
        return prefix ?: @"刷新失败";
    }
    return [NSString stringWithFormat:@"%@ · %@", prefix ?: @"刷新失败", reason];
}

static NSError *BiliAPIError(NSDictionary *root, NSString *fallbackDescription, NSInteger fallbackCode) {
    id codeValue = root[@"code"];
    NSInteger code = [codeValue respondsToSelector:@selector(integerValue)] ? [codeValue integerValue] : fallbackCode;
    NSString *message = [root[@"message"] isKindOfClass:NSString.class] ? root[@"message"] : nil;
    if (message.length == 0) {
        message = [root[@"msg"] isKindOfClass:NSString.class] ? root[@"msg"] : nil;
    }
    NSString *cleanMessage = [message stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *description = cleanMessage.length > 0 ? cleanMessage : (fallbackDescription ?: @"接口返回异常");
    if (code != 0 && cleanMessage.length > 0) {
        description = [NSString stringWithFormat:@"%@（%ld）", cleanMessage, (long)code];
    }
    return [NSError errorWithDomain:@"BILIFansCard"
                               code:code != 0 ? code : fallbackCode
                           userInfo:@{NSLocalizedDescriptionKey: description}];
}

static long long MIDFromDigitString(NSString *value) {
    NSString *trimmed = [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (trimmed.length == 0 || trimmed.length > 18) {
        return 0;
    }
    NSCharacterSet *nonDigits = [[NSCharacterSet decimalDigitCharacterSet] invertedSet];
    if ([trimmed rangeOfCharacterFromSet:nonDigits].location != NSNotFound) {
        return 0;
    }
    long long mid = trimmed.longLongValue;
    return mid > 0 ? mid : 0;
}

static BOOL BilibiliHostIsTrusted(NSString *host) {
    NSString *normalized = host.lowercaseString ?: @"";
    return [normalized isEqualToString:@"bilibili.com"] || [normalized hasSuffix:@".bilibili.com"];
}

static long long MIDFromQueryItems(NSArray<NSURLQueryItem *> *queryItems) {
    NSSet<NSString *> *candidateNames = [NSSet setWithArray:@[@"mid", @"uid", @"vmid", @"author_id"]];
    for (NSURLQueryItem *item in queryItems) {
        NSString *name = item.name.lowercaseString ?: @"";
        if (![candidateNames containsObject:name]) continue;
        long long mid = MIDFromDigitString(item.value ?: @"");
        if (mid > 0) return mid;
    }
    return 0;
}

static long long BilibiliMIDFromInput(NSString *input) {
    NSString *trimmed = [input stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    long long directMID = MIDFromDigitString(trimmed);
    if (directMID > 0) {
        return directMID;
    }
    BOOL likelyWebLink = [trimmed rangeOfString:@"bilibili.com" options:NSCaseInsensitiveSearch].location != NSNotFound;
    BOOL likelyAppLink = [trimmed rangeOfString:@"bilibili://" options:NSCaseInsensitiveSearch].location == 0;
    if (!likelyWebLink && !likelyAppLink) {
        return 0;
    }

    NSString *urlText = [trimmed rangeOfString:@"://"].location == NSNotFound
        ? [@"https://" stringByAppendingString:trimmed]
        : trimmed;
    NSURLComponents *components = [NSURLComponents componentsWithString:urlText];
    NSString *host = components.host.lowercaseString ?: @"";
    NSString *scheme = components.scheme.lowercaseString ?: @"";
    BOOL appLink = [scheme isEqualToString:@"bilibili"];
    if (!appLink && !BilibiliHostIsTrusted(host)) {
        return 0;
    }

    long long queryMID = MIDFromQueryItems(components.queryItems ?: @[]);
    if (queryMID > 0) return queryMID;

    NSArray<NSString *> *parts = [components.path componentsSeparatedByString:@"/"];
    BOOL spaceHost = [host isEqualToString:@"space.bilibili.com"] || [host hasSuffix:@".space.bilibili.com"];
    for (NSUInteger i = 0; i < parts.count; i++) {
        NSString *part = parts[i];
        if (part.length == 0) continue;
        if (spaceHost) {
            long long mid = MIDFromDigitString(part);
            if (mid > 0) return mid;
        }
        if ([part isEqualToString:@"space"] && i + 1 < parts.count) {
            long long mid = MIDFromDigitString(parts[i + 1]);
            if (mid > 0) return mid;
        }
    }
    if (appLink && ([host isEqualToString:@"space"] || [host isEqualToString:@"user"])) {
        for (NSString *part in parts) {
            long long mid = MIDFromDigitString(part);
            if (mid > 0) return mid;
        }
    }
    return 0;
}

static BOOL HistoryPointIsUsable(id point) {
    if (![point isKindOfClass:NSDictionary.class]) {
        return NO;
    }
    NSDictionary *entry = point;
    id followers = entry[@"followers"];
    if (!followers || ![followers respondsToSelector:@selector(integerValue)]) {
        return NO;
    }
    return [entry[@"updated_at"] doubleValue] > 0;
}

static NSDateFormatter *ThreadLocalDateFormatterWithLocale(NSString *key, NSString *format, NSString *localeIdentifier) {
    NSMutableDictionary *threadDictionary = NSThread.currentThread.threadDictionary;
    NSDateFormatter *formatter = threadDictionary[key];
    if (!formatter) {
        formatter = [NSDateFormatter new];
        formatter.locale = [NSLocale localeWithLocaleIdentifier:localeIdentifier ?: @"en_US_POSIX"];
        formatter.dateFormat = format;
        threadDictionary[key] = formatter;
    }
    return formatter;
}

static NSDateFormatter *ThreadLocalDateFormatter(NSString *key, NSString *format) {
    return ThreadLocalDateFormatterWithLocale(key, format, @"en_US_POSIX");
}

static NSArray<NSDictionary *> *SortedUsableHistoryPoints(NSArray<NSDictionary *> *history) {
    NSMutableArray<NSDictionary *> *usable = [NSMutableArray new];
    for (id point in history ?: @[]) {
        if (HistoryPointIsUsable(point)) {
            [usable addObject:point];
        }
    }
    [usable sortUsingComparator:^NSComparisonResult(NSDictionary *left, NSDictionary *right) {
        NSTimeInterval leftTime = [left[@"updated_at"] doubleValue];
        NSTimeInterval rightTime = [right[@"updated_at"] doubleValue];
        if (leftTime < rightTime) return NSOrderedAscending;
        if (leftTime > rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];
    return usable.copy;
}

static NSUInteger FirstHistoryPointIndexAtOrAfterCutoffFromSortedPoints(NSArray<NSDictionary *> *points, NSTimeInterval cutoff) {
    NSUInteger low = 0;
    NSUInteger high = points.count;
    while (low < high) {
        NSUInteger mid = low + (high - low) / 2;
        NSTimeInterval updatedAt = [points[mid][@"updated_at"] doubleValue];
        if (updatedAt < cutoff) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return low;
}

static NSArray<NSDictionary *> *TrendHistoryIncludingBaselineFromSortedPoints(NSArray<NSDictionary *> *points, NSTimeInterval cutoff) {
    NSArray<NSDictionary *> *sortedPoints = points ?: @[];
    NSUInteger firstIndex = FirstHistoryPointIndexAtOrAfterCutoffFromSortedPoints(sortedPoints, cutoff);
    if (firstIndex >= sortedPoints.count) {
        return @[];
    }
    NSArray<NSDictionary *> *inRange = [sortedPoints subarrayWithRange:NSMakeRange(firstIndex, sortedPoints.count - firstIndex)];
    if (firstIndex > 0 && [inRange.firstObject[@"updated_at"] doubleValue] > cutoff) {
        NSMutableArray<NSDictionary *> *withBaseline = [NSMutableArray arrayWithCapacity:inRange.count + 1];
        [withBaseline addObject:sortedPoints[firstIndex - 1]];
        [withBaseline addObjectsFromArray:inRange];
        return withBaseline.copy;
    }
    return inRange;
}

__attribute__((unused)) static NSArray<NSDictionary *> *TrendHistoryIncludingBaseline(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    return TrendHistoryIncludingBaselineFromSortedPoints(SortedUsableHistoryPoints(history), cutoff);
}

static NSUInteger HistoryPointCountAtOrAfterCutoffFromSortedPoints(NSArray<NSDictionary *> *points, NSTimeInterval cutoff) {
    NSUInteger firstIndex = FirstHistoryPointIndexAtOrAfterCutoffFromSortedPoints(points, cutoff);
    return points.count - firstIndex;
}

__attribute__((unused)) static NSUInteger HistoryPointCountAtOrAfterCutoff(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    return HistoryPointCountAtOrAfterCutoffFromSortedPoints(SortedUsableHistoryPoints(history), cutoff);
}

static NSUInteger HistoryPointCountAtOrAfterCutoffFromSortedUsablePoints(NSArray<NSDictionary *> *points, NSTimeInterval cutoff) {
    return HistoryPointCountAtOrAfterCutoffFromSortedPoints(points ?: @[], cutoff);
}

static NSArray<NSDictionary *> *HistoryPointsAtOrAfterCutoffFromSortedPoints(NSArray<NSDictionary *> *points, NSTimeInterval cutoff) {
    NSArray<NSDictionary *> *sortedPoints = points ?: @[];
    NSUInteger firstIndex = FirstHistoryPointIndexAtOrAfterCutoffFromSortedPoints(sortedPoints, cutoff);
    if (firstIndex >= sortedPoints.count) {
        return @[];
    }
    return [sortedPoints subarrayWithRange:NSMakeRange(firstIndex, sortedPoints.count - firstIndex)];
}

__attribute__((unused)) static NSArray<NSDictionary *> *HistoryPointsAtOrAfterCutoff(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    return HistoryPointsAtOrAfterCutoffFromSortedPoints(SortedUsableHistoryPoints(history), cutoff);
}

static NSString *CSVStringForHistoryPoints(NSArray<NSDictionary *> *points, BOOL includeScopeColumn, NSTimeInterval cutoff) {
    NSDateFormatter *csvFormatter = ThreadLocalDateFormatter(@"BILIFansCSVDateFormatter", @"yyyy-MM-dd HH:mm:ss");

    NSMutableString *csv = [NSMutableString stringWithString:includeScopeColumn ? @"time,uid,followers,scope\n" : @"time,uid,followers\n"];
    for (NSDictionary *point in points ?: @[]) {
        if (!HistoryPointIsUsable(point)) continue;
        NSTimeInterval updatedAt = [point[@"updated_at"] doubleValue];
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:updatedAt];
        if (includeScopeColumn) {
            NSString *scope = updatedAt < cutoff ? @"baseline" : @"range";
            [csv appendFormat:@"%@,%lld,%ld,%@\n",
             [csvFormatter stringFromDate:date],
             [point[@"mid"] longLongValue],
             (long)[point[@"followers"] integerValue],
             scope];
        } else {
            [csv appendFormat:@"%@,%lld,%ld\n",
             [csvFormatter stringFromDate:date],
             [point[@"mid"] longLongValue],
             (long)[point[@"followers"] integerValue]];
        }
    }
    return csv.copy;
}

static NSString *HistoryCSVPeriodIdentifierForDate(NSDate *date) {
    NSDateFormatter *formatter = ThreadLocalDateFormatter(@"BILIFansCSVPeriodFormatter", @"yyyy-MM");
    return [formatter stringFromDate:date ?: NSDate.date];
}

static NSURL *HistoryCSVRootDirectoryURL(BOOL create) {
    NSString *overrideRoot = NSProcessInfo.processInfo.environment[@"BILIFANSCARD_HISTORY_CSV_ROOT"];
    NSURL *rootURL = nil;
    if (overrideRoot.length > 0) {
        rootURL = [NSURL fileURLWithPath:overrideRoot isDirectory:YES];
    } else {
        NSURL *supportURL = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                                 inDomains:NSUserDomainMask].firstObject;
        if (!supportURL) return nil;
        NSString *bundleID = NSBundle.mainBundle.bundleIdentifier;
        if (bundleID.length == 0) bundleID = @"com.example.bilifanscard";
        rootURL = [[supportURL URLByAppendingPathComponent:bundleID isDirectory:YES]
            URLByAppendingPathComponent:@"HistoryCSV" isDirectory:YES];
    }
    if (create) {
        NSError *error = nil;
        [NSFileManager.defaultManager createDirectoryAtURL:rootURL
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:&error];
        if (error) {
            NSLog(@"BILI粉丝数：创建历史 CSV 目录失败：%@", error.localizedDescription);
            return nil;
        }
    }
    return rootURL;
}

static NSURL *HistoryCSVDirectoryURLForMID(long long mid, BOOL create) {
    if (mid <= 0) return nil;
    NSURL *rootURL = HistoryCSVRootDirectoryURL(create);
    if (!rootURL) return nil;
    NSURL *directoryURL = [rootURL URLByAppendingPathComponent:[NSString stringWithFormat:@"%lld", mid]
                                                   isDirectory:YES];
    if (create) {
        NSError *error = nil;
        [NSFileManager.defaultManager createDirectoryAtURL:directoryURL
                               withIntermediateDirectories:YES
                                                attributes:nil
                                                     error:&error];
        if (error) {
            NSLog(@"BILI粉丝数：创建 UID 历史 CSV 目录失败：%@", error.localizedDescription);
            return nil;
        }
    }
    return directoryURL;
}

static NSURL *HistoryCSVFileURLForMIDAndDate(long long mid, NSDate *date, BOOL create) {
    NSURL *directoryURL = HistoryCSVDirectoryURLForMID(mid, create);
    if (!directoryURL) return nil;
    NSString *filename = [[HistoryCSVPeriodIdentifierForDate(date) stringByAppendingString:@".csv"] copy];
    return [directoryURL URLByAppendingPathComponent:filename isDirectory:NO];
}

static NSString *CSVLineForHistoryPoint(NSDictionary *point) {
    if (!HistoryPointIsUsable(point)) return @"";
    NSDateFormatter *csvFormatter = ThreadLocalDateFormatter(@"BILIFansCSVDateFormatter", @"yyyy-MM-dd HH:mm:ss");
    NSTimeInterval updatedAt = [point[@"updated_at"] doubleValue];
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:updatedAt];
    return [NSString stringWithFormat:@"%@,%lld,%ld\n",
            [csvFormatter stringFromDate:date],
            [point[@"mid"] longLongValue],
            (long)[point[@"followers"] integerValue]];
}

static NSString *AppVersionString(void) {
    static NSString *version = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSString *bundleVersion = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
        version = [bundleVersion.length > 0 ? bundleVersion : @"3" copy];
    });
    return version;
}

static NSString *AppUserAgentString(void) {
    static NSString *userAgent = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        userAgent = [[NSString stringWithFormat:@"Mozilla/5.0 macOS BILIFansCard/%@", AppVersionString()] copy];
    });
    return userAgent;
}

static void FlushUserSettings(void) {
    (void)CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}
