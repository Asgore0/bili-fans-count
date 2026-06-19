#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <UserNotifications/UserNotifications.h>
#import <ServiceManagement/ServiceManagement.h>
#import <math.h>
#import <stdlib.h>

static NSString * const DefaultName = @"谐门东西";
static NSString * const UnknownTargetName = @"BILI粉丝数";
static long long const DefaultMID = 3546718146661176LL;
static NSTimeInterval const DefaultRefreshInterval = 60;
static NSUInteger const MaxHistoryTargets = 12;
static NSUInteger const MaxHistoryPointsPerTarget = 6000;
static NSUInteger const MaxSparklinePoints = 42;
static NSTimeInterval const OneWeekInterval = 7 * 24 * 60 * 60;
static CGFloat const MainWindowWidth = 900;
static CGFloat const MainWindowHeight = 620;
static NSString * const ShowMainWindowNotificationName = @"com.example.bilifanscard.showMainWindow";

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

static CGFloat ClampUnit(CGFloat value) {
    return MIN(MAX(value, 0.0), 1.0);
}

static BOOL AppAppearanceIsDark(void) {
    NSAppearanceName match = [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua,
        NSAppearanceNameDarkAqua
    ]];
    return [match isEqualToString:NSAppearanceNameDarkAqua];
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
        [[NSColor whiteColor] colorWithAlphaComponent:(dark ? 0.030 : 0.082) * clamped],
        [[NSColor whiteColor] colorWithAlphaComponent:0.0]
    ]];
    [topSheen drawInRect:NSMakeRect(NSMinX(rect), NSMaxY(rect) - rect.size.height * 0.46, rect.size.width, rect.size.height * 0.46) angle:90];

    NSGradient *lowerDepth = [[NSGradient alloc] initWithColors:@[
        [[NSColor blackColor] colorWithAlphaComponent:0.0],
        [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.024 : 0.024) * clamped],
        [[NSColor blackColor] colorWithAlphaComponent:(dark ? 0.046 : 0.052) * clamped]
    ]];
    [lowerDepth drawInRect:NSMakeRect(NSMinX(rect), NSMinY(rect), rect.size.width, rect.size.height * 0.58) angle:-90];

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

static void DrawCompactBMark(NSRect rect, NSColor *color, CGFloat lineWidth) {
    if (rect.size.width <= 0 || rect.size.height <= 0) {
        return;
    }
    CGFloat x = NSMinX(rect);
    CGFloat y = NSMinY(rect);
    CGFloat w = rect.size.width;
    CGFloat h = rect.size.height;
    CGFloat stemX = x + w * 0.28;
    CGFloat topY = y + h * 0.88;
    CGFloat midY = y + h * 0.51;
    CGFloat bottomY = y + h * 0.12;
    CGFloat rightX = x + w * 0.84;

    NSBezierPath *mark = [NSBezierPath bezierPath];
    mark.lineWidth = lineWidth;
    mark.lineCapStyle = NSLineCapStyleRound;
    mark.lineJoinStyle = NSLineJoinStyleRound;
    [mark moveToPoint:NSMakePoint(stemX, bottomY)];
    [mark lineToPoint:NSMakePoint(stemX, topY)];
    [mark moveToPoint:NSMakePoint(stemX, topY)];
    [mark curveToPoint:NSMakePoint(stemX, midY + h * 0.03)
         controlPoint1:NSMakePoint(rightX, topY)
         controlPoint2:NSMakePoint(rightX, midY + h * 0.08)];
    [mark moveToPoint:NSMakePoint(stemX, midY - h * 0.02)];
    [mark curveToPoint:NSMakePoint(stemX, bottomY)
         controlPoint1:NSMakePoint(x + w * 0.92, midY - h * 0.08)
         controlPoint2:NSMakePoint(x + w * 0.92, bottomY)];
    [color setStroke];
    [mark stroke];
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

static void DrawLiquidGlassBackdrop(NSRect rect, CGFloat radius, NSColor *accent, NSInteger appearanceMode, CGFloat intensity) {
    CGFloat scale = MAX(0.0, intensity);
    BOOL dark = AppAppearanceIsDark();
    CGFloat surfaceAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.170, 0.085, 0.032)
        : LiquidGlassAlphaForMode(appearanceMode, 0.116, 0.040, 0.008)) * scale;
    CGFloat middleAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.118, 0.052, 0.018)
        : LiquidGlassAlphaForMode(appearanceMode, 0.068, 0.020, 0.003)) * scale;
    CGFloat baseAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.170, 0.080, 0.030)
        : LiquidGlassAlphaForMode(appearanceMode, 0.044, 0.009, 0.001)) * scale;
    CGFloat sheenAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.070, 0.042, 0.020)
        : LiquidGlassAlphaForMode(appearanceMode, 0.090, 0.052, 0.026)) * scale;
    CGFloat accentAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.094, 0.046, 0.018)
        : LiquidGlassAlphaForMode(appearanceMode, 0.058, 0.026, 0.011)) * scale;
    CGFloat depthAlpha = (dark
        ? LiquidGlassAlphaForMode(appearanceMode, 0.082, 0.044, 0.020)
        : LiquidGlassAlphaForMode(appearanceMode, 0.030, 0.014, 0.006)) * scale;
    NSColor *surfaceColor = dark
        ? [NSColor colorWithCalibratedRed:0.30 green:0.34 blue:0.40 alpha:surfaceAlpha]
        : [[NSColor controlBackgroundColor] colorWithAlphaComponent:surfaceAlpha];
    NSColor *middleColor = dark
        ? [NSColor colorWithCalibratedRed:0.12 green:0.14 blue:0.18 alpha:middleAlpha]
        : [[NSColor windowBackgroundColor] colorWithAlphaComponent:middleAlpha];
    NSColor *baseColor = dark
        ? [NSColor colorWithCalibratedRed:0.02 green:0.025 blue:0.035 alpha:baseAlpha]
        : [[NSColor underPageBackgroundColor] colorWithAlphaComponent:baseAlpha];

    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:radius yRadius:radius];
    [NSGraphicsContext saveGraphicsState];
    [shape addClip];

    NSGradient *wash = [[NSGradient alloc] initWithColors:@[
        surfaceColor,
        middleColor,
        baseColor
    ]];
    [wash drawInRect:rect angle:-28];

    NSBezierPath *topLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMinX(rect) - rect.size.width * 0.16,
                                                                               NSMaxY(rect) - rect.size.height * 0.58,
                                                                               rect.size.width * 0.72,
                                                                               rect.size.height * 0.54)];
    NSGradient *topGlow = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:sheenAlpha]
                                                        endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [topGlow drawInBezierPath:topLens relativeCenterPosition:NSMakePoint(-0.28, 0.16)];

    NSBezierPath *accentLens = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(NSMaxX(rect) - rect.size.width * 0.45,
                                                                                  NSMinY(rect) - rect.size.height * 0.18,
                                                                                  rect.size.width * 0.58,
                                                                                  rect.size.height * 0.58)];
    NSGradient *accentGlow = [[NSGradient alloc] initWithStartingColor:[accent colorWithAlphaComponent:accentAlpha]
                                                           endingColor:[accent colorWithAlphaComponent:0.0]];
    [accentGlow drawInBezierPath:accentLens relativeCenterPosition:NSMakePoint(0.22, -0.14)];

    NSGradient *depth = [[NSGradient alloc] initWithStartingColor:[[NSColor blackColor] colorWithAlphaComponent:0.0]
                                                     endingColor:[[NSColor blackColor] colorWithAlphaComponent:depthAlpha]];
    [depth drawInRect:NSMakeRect(NSMinX(rect), NSMinY(rect), rect.size.width, rect.size.height * 0.58) angle:-90];

    [NSGraphicsContext restoreGraphicsState];
}

static void DrawLiquidGlassControlWell(NSRect rect, CGFloat radius, NSColor *accent, NSInteger appearanceMode, BOOL emphasized) {
    BOOL dark = AppAppearanceIsDark();
    DrawLiquidGlassBackdrop(rect, radius, accent, appearanceMode, emphasized ? (dark ? 0.82 : 0.78) : (dark ? 0.42 : 0.54));
    DrawLiquidGlassRim(rect, radius, accent, emphasized ? (dark ? 0.46 : 0.44) : (dark ? 0.18 : 0.28));
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

static NSArray<NSDictionary *> *TrendHistoryIncludingBaseline(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    NSArray<NSDictionary *> *points = SortedUsableHistoryPoints(history);
    NSDictionary *baseline = nil;
    NSMutableArray<NSDictionary *> *inRange = [NSMutableArray new];
    for (NSDictionary *point in points) {
        NSTimeInterval updatedAt = [point[@"updated_at"] doubleValue];
        if (updatedAt < cutoff) {
            baseline = point;
        } else {
            [inRange addObject:point];
        }
    }
    if (inRange.count == 0) {
        return @[];
    }
    if (baseline && [inRange.firstObject[@"updated_at"] doubleValue] > cutoff) {
        [inRange insertObject:baseline atIndex:0];
    }
    return inRange.copy;
}

static NSUInteger HistoryPointCountAtOrAfterCutoff(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    NSUInteger count = 0;
    for (id point in history ?: @[]) {
        if (HistoryPointIsUsable(point) && [point[@"updated_at"] doubleValue] >= cutoff) {
            count++;
        }
    }
    return count;
}

static NSArray<NSDictionary *> *HistoryPointsAtOrAfterCutoff(NSArray<NSDictionary *> *history, NSTimeInterval cutoff) {
    NSArray<NSDictionary *> *points = SortedUsableHistoryPoints(history);
    NSMutableArray<NSDictionary *> *filtered = [NSMutableArray new];
    for (NSDictionary *point in points) {
        if ([point[@"updated_at"] doubleValue] >= cutoff) {
            [filtered addObject:point];
        }
    }
    return filtered.copy;
}

static NSString *CSVStringForHistoryPoints(NSArray<NSDictionary *> *points, BOOL includeScopeColumn, NSTimeInterval cutoff) {
    NSDateFormatter *csvFormatter = [NSDateFormatter new];
    csvFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    csvFormatter.dateFormat = @"yyyy-MM-dd HH:mm:ss";

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
    NSDateFormatter *formatter = [NSDateFormatter new];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"yyyy-MM";
    return [formatter stringFromDate:date ?: NSDate.date];
}

static NSURL *HistoryCSVRootDirectoryURL(BOOL create) {
    NSURL *supportURL = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                             inDomains:NSUserDomainMask].firstObject;
    if (!supportURL) return nil;
    NSString *bundleID = NSBundle.mainBundle.bundleIdentifier;
    if (bundleID.length == 0) bundleID = @"com.example.bilifanscard";
    NSURL *rootURL = [[supportURL URLByAppendingPathComponent:bundleID isDirectory:YES]
        URLByAppendingPathComponent:@"HistoryCSV" isDirectory:YES];
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
    NSDateFormatter *csvFormatter = [NSDateFormatter new];
    csvFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    csvFormatter.dateFormat = @"yyyy-MM-dd HH:mm:ss";
    NSTimeInterval updatedAt = [point[@"updated_at"] doubleValue];
    NSDate *date = [NSDate dateWithTimeIntervalSince1970:updatedAt];
    return [NSString stringWithFormat:@"%@,%lld,%ld\n",
            [csvFormatter stringFromDate:date],
            [point[@"mid"] longLongValue],
            (long)[point[@"followers"] integerValue]];
}

static NSString *AppVersionString(void) {
    NSString *version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    return version.length > 0 ? version : @"3";
}

static void FlushUserSettings(void) {
    (void)CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

@interface Settings : NSObject
+ (long long)mid;
+ (void)setMid:(long long)mid;
+ (NSString *)name;
+ (void)setName:(NSString *)name;
+ (FanResult *)latestResultForCurrentTarget;
+ (void)saveLatestResult:(FanResult *)result;
+ (void)clearLatestResult;
+ (NSArray<FanResult *> *)recentResults;
+ (NSArray<NSDictionary *> *)historyForCurrentTarget;
+ (NSArray<NSDictionary *> *)normalizedHistoryGroup:(NSArray<NSDictionary *> *)sortedGroup referenceDate:(NSDate *)referenceDate;
+ (void)appendSegmentedHistoryCSVPoint:(NSDictionary *)point;
+ (void)backfillSegmentedHistoryCSVIfNeededForMID:(long long)mid history:(NSArray<NSDictionary *> *)history;
+ (void)saveHistoryPoint:(FanResult *)result;
+ (void)clearSegmentedHistoryCSVForMID:(long long)mid;
+ (void)clearHistoryForCurrentTarget;
+ (NSInteger)appearanceMode;
+ (void)setAppearanceMode:(NSInteger)mode;
+ (NSInteger)statusDisplayMode;
+ (void)setStatusDisplayMode:(NSInteger)mode;
+ (NSInteger)trendRange;
+ (void)setTrendRange:(NSInteger)range;
+ (NSTimeInterval)refreshInterval;
+ (void)setRefreshInterval:(NSTimeInterval)interval;
+ (BOOL)autoRefreshEnabled;
+ (void)setAutoRefreshEnabled:(BOOL)enabled;
+ (BOOL)changeNotificationsEnabled;
+ (void)setChangeNotificationsEnabled:(BOOL)enabled;
+ (BOOL)alwaysOnTop;
+ (void)setAlwaysOnTop:(BOOL)enabled;
+ (BOOL)positionLocked;
+ (void)setPositionLocked:(BOOL)locked;
+ (BOOL)launchHidden;
+ (void)setLaunchHidden:(BOOL)hidden;
+ (NSPoint)windowOriginWithDefaultFrame:(NSRect)frame;
+ (void)saveWindowFrame:(NSRect)frame;
@end

@implementation Settings
static long long sCachedHistoryMID = 0;
static NSArray<NSDictionary *> *sCachedHistoryForMID = nil;

+ (void)invalidateHistoryCache {
    sCachedHistoryMID = 0;
    sCachedHistoryForMID = nil;
}

+ (long long)mid {
    long long value = [[NSUserDefaults standardUserDefaults] objectForKey:@"target_mid"]
        ? [[NSUserDefaults standardUserDefaults] integerForKey:@"target_mid"]
        : DefaultMID;
    return value > 0 ? value : DefaultMID;
}
+ (void)setMid:(long long)mid {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"target_mid"];
    long long previous = [self mid];
    BOOL storedMatches = [stored respondsToSelector:@selector(longLongValue)] && [stored longLongValue] == mid;
    if (previous == mid && storedMatches) {
        return;
    }
    [defaults setObject:@(mid) forKey:@"target_mid"];
    [self invalidateHistoryCache];
    FlushUserSettings();
}
+ (NSString *)name {
    NSString *value = [[NSUserDefaults standardUserDefaults] stringForKey:@"target_name"];
    return value.length > 0 ? value : DefaultName;
}
+ (void)setName:(NSString *)name {
    NSString *current = [[NSUserDefaults standardUserDefaults] stringForKey:@"target_name"];
    NSString *next = [name copy];
    if ((current || next) && ![current isEqualToString:next]) {
        if (next.length > 0) {
            [[NSUserDefaults standardUserDefaults] setObject:next forKey:@"target_name"];
        } else {
            [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"target_name"];
        }
        FlushUserSettings();
    }
}
+ (FanResult *)latestResultFromDictionary:(NSDictionary *)stored expectedMid:(long long)expectedMid fallbackName:(NSString *)fallbackName {
    if (![stored isKindOfClass:NSDictionary.class]) {
        return nil;
    }
    long long mid = [stored[@"mid"] longLongValue];
    if (mid <= 0 || mid != expectedMid) {
        return nil;
    }
    NSTimeInterval updatedAt = [stored[@"updated_at"] doubleValue];
    if (updatedAt <= 0) {
        return nil;
    }
    id followers = stored[@"followers"];
    if (!followers || ![followers respondsToSelector:@selector(integerValue)]) {
        return nil;
    }

    FanResult *result = [FanResult new];
    result.mid = mid;
    result.name = CleanName([stored[@"name"] isKindOfClass:NSString.class] ? stored[@"name"] : fallbackName, mid);
    result.followers = [followers integerValue];
    result.updatedAt = [NSDate dateWithTimeIntervalSince1970:updatedAt];
    return result;
}

+ (FanResult *)latestResultForCurrentTarget {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    long long currentMID = [self mid];
    NSDictionary *byMID = [defaults dictionaryForKey:@"latest_results_by_mid"];
    NSString *key = [NSString stringWithFormat:@"%lld", currentMID];
    FanResult *storedResult = [self latestResultFromDictionary:byMID[key]
                                                   expectedMid:currentMID
                                                 fallbackName:[self name]];
    if (storedResult) {
        return storedResult;
    }

    if (![defaults objectForKey:@"latest_followers"] || ![defaults objectForKey:@"latest_updated_at"]) {
        return nil;
    }
    NSDictionary *legacy = @{@"mid": [defaults objectForKey:@"latest_mid"] ?: @(0),
                             @"name": [defaults stringForKey:@"latest_name"] ?: [self name],
                             @"followers": [defaults objectForKey:@"latest_followers"] ?: @(0),
                             @"updated_at": @([defaults doubleForKey:@"latest_updated_at"])};
    return [self latestResultFromDictionary:legacy
                                expectedMid:currentMID
                              fallbackName:[self name]];
}

+ (void)saveLatestResult:(FanResult *)result {
    if (!result) return;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:@(result.mid) forKey:@"latest_mid"];
    [defaults setObject:result.name ?: [self name] forKey:@"latest_name"];
    [defaults setInteger:result.followers forKey:@"latest_followers"];
    [defaults setDouble:result.updatedAt.timeIntervalSince1970 forKey:@"latest_updated_at"];

    NSMutableDictionary *byMID = [[defaults dictionaryForKey:@"latest_results_by_mid"] mutableCopy] ?: [NSMutableDictionary new];
    NSString *key = [NSString stringWithFormat:@"%lld", result.mid];
    byMID[key] = @{@"mid": @(result.mid),
                   @"name": result.name ?: [self name],
                   @"followers": @(result.followers),
                   @"updated_at": @(result.updatedAt.timeIntervalSince1970)};

    NSArray<NSString *> *recentKeys = [byMID.allKeys sortedArrayUsingComparator:^NSComparisonResult(NSString *left, NSString *right) {
        NSDictionary *leftResult = [byMID[left] isKindOfClass:NSDictionary.class] ? byMID[left] : nil;
        NSDictionary *rightResult = [byMID[right] isKindOfClass:NSDictionary.class] ? byMID[right] : nil;
        NSTimeInterval leftTime = [leftResult[@"updated_at"] doubleValue];
        NSTimeInterval rightTime = [rightResult[@"updated_at"] doubleValue];
        if (leftTime > rightTime) return NSOrderedAscending;
        if (leftTime < rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];
    for (NSUInteger i = MaxHistoryTargets; i < recentKeys.count; i++) {
        [byMID removeObjectForKey:recentKeys[i]];
    }
    [defaults setObject:byMID.copy forKey:@"latest_results_by_mid"];
    FlushUserSettings();
}
+ (void)clearLatestResult {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    long long currentMID = [self mid];
    NSMutableDictionary *byMID = [[defaults dictionaryForKey:@"latest_results_by_mid"] mutableCopy];
    if (byMID) {
        [byMID removeObjectForKey:[NSString stringWithFormat:@"%lld", currentMID]];
        [defaults setObject:byMID.copy forKey:@"latest_results_by_mid"];
    }
    long long legacyMID = [[defaults objectForKey:@"latest_mid"] longLongValue];
    if (legacyMID == currentMID) {
        [defaults removeObjectForKey:@"latest_mid"];
        [defaults removeObjectForKey:@"latest_name"];
        [defaults removeObjectForKey:@"latest_followers"];
        [defaults removeObjectForKey:@"latest_updated_at"];
    }
    FlushUserSettings();
}
+ (NSArray<FanResult *> *)recentResults {
    NSDictionary *byMID = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"latest_results_by_mid"];
    if (![byMID isKindOfClass:NSDictionary.class] || byMID.count == 0) {
        FanResult *current = [self latestResultForCurrentTarget];
        return current ? @[current] : @[];
    }

    NSMutableArray<FanResult *> *results = [NSMutableArray new];
    for (id key in byMID) {
        NSDictionary *stored = [byMID[key] isKindOfClass:NSDictionary.class] ? byMID[key] : nil;
        long long mid = [stored[@"mid"] longLongValue];
        FanResult *result = [self latestResultFromDictionary:stored
                                                 expectedMid:mid
                                               fallbackName:UnknownTargetName];
        if (result) {
            [results addObject:result];
        }
    }
    FanResult *current = [self latestResultForCurrentTarget];
    if (current) {
        BOOL alreadyIncluded = NO;
        for (FanResult *result in results) {
            if (result.mid == current.mid) {
                alreadyIncluded = YES;
                break;
            }
        }
        if (!alreadyIncluded) {
            [results addObject:current];
        }
    }
    [results sortUsingComparator:^NSComparisonResult(FanResult *left, FanResult *right) {
        NSTimeInterval leftTime = left.updatedAt.timeIntervalSince1970;
        NSTimeInterval rightTime = right.updatedAt.timeIntervalSince1970;
        if (leftTime > rightTime) return NSOrderedAscending;
        if (leftTime < rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];
    if (results.count > MaxHistoryTargets) {
        return [results subarrayWithRange:NSMakeRange(0, MaxHistoryTargets)];
    }
    return results.copy;
}
+ (NSArray<NSDictionary *> *)historyForCurrentTarget {
    long long mid = [self mid];
    if (sCachedHistoryForMID && sCachedHistoryMID == mid) {
        return sCachedHistoryForMID;
    }
    NSArray *stored = [[NSUserDefaults standardUserDefaults] arrayForKey:@"history_points"];
    if (![stored isKindOfClass:NSArray.class]) {
        [self invalidateHistoryCache];
        return @[];
    }
    NSMutableArray<NSDictionary *> *filtered = [NSMutableArray new];
    for (NSDictionary *point in stored) {
        if (![point isKindOfClass:NSDictionary.class]) continue;
        if ([point[@"mid"] longLongValue] == mid && point[@"followers"] && point[@"updated_at"]) {
            [filtered addObject:point];
        }
    }
    [filtered sortUsingComparator:^NSComparisonResult(NSDictionary *left, NSDictionary *right) {
        NSTimeInterval leftTime = [left[@"updated_at"] doubleValue];
        NSTimeInterval rightTime = [right[@"updated_at"] doubleValue];
        if (leftTime < rightTime) return NSOrderedAscending;
        if (leftTime > rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];
    sCachedHistoryMID = mid;
    sCachedHistoryForMID = filtered.copy;
    return sCachedHistoryForMID;
}
+ (NSTimeInterval)historyBucketIntervalForAge:(NSTimeInterval)age {
    NSTimeInterval day = 24 * 60 * 60;
    if (age <= 2 * day) {
        return 0;
    }
    if (age <= 30 * day) {
        return 60 * 60;
    }
    if (age <= 180 * day) {
        return 6 * 60 * 60;
    }
    return day;
}
+ (NSArray<NSDictionary *> *)normalizedHistoryGroup:(NSArray<NSDictionary *> *)sortedGroup referenceDate:(NSDate *)referenceDate {
    if (sortedGroup.count == 0) {
        return @[];
    }
    NSTimeInterval referenceTime = (referenceDate ?: NSDate.date).timeIntervalSince1970;
    NSMutableArray<NSDictionary *> *recent = [NSMutableArray new];
    NSMutableDictionary<NSString *, NSDictionary *> *bucketed = [NSMutableDictionary new];

    for (NSDictionary *point in sortedGroup) {
        if (![point isKindOfClass:NSDictionary.class]) continue;
        NSTimeInterval updatedAt = [point[@"updated_at"] doubleValue];
        if (updatedAt <= 0) {
            continue;
        }
        NSTimeInterval age = MAX(0, referenceTime - updatedAt);
        NSTimeInterval interval = [self historyBucketIntervalForAge:age];
        if (interval <= 0) {
            [recent addObject:point];
            continue;
        }
        NSTimeInterval bucket = floor(updatedAt / interval) * interval;
        NSString *key = [NSString stringWithFormat:@"%.0f", bucket];
        bucketed[key] = point;
    }

    NSMutableArray<NSDictionary *> *combined = [NSMutableArray arrayWithArray:recent];
    [combined addObjectsFromArray:bucketed.allValues];
    [combined sortUsingComparator:^NSComparisonResult(NSDictionary *left, NSDictionary *right) {
        NSTimeInterval leftTime = [left[@"updated_at"] doubleValue];
        NSTimeInterval rightTime = [right[@"updated_at"] doubleValue];
        if (leftTime < rightTime) return NSOrderedAscending;
        if (leftTime > rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];
    if (combined.count > MaxHistoryPointsPerTarget) {
        return [combined subarrayWithRange:NSMakeRange(combined.count - MaxHistoryPointsPerTarget, MaxHistoryPointsPerTarget)];
    }
    return combined.copy;
}
+ (void)backfillSegmentedHistoryCSVIfNeededForMID:(long long)mid history:(NSArray<NSDictionary *> *)history {
    if (mid <= 0 || history.count == 0) return;

    NSFileManager *fileManager = NSFileManager.defaultManager;
    NSURL *directoryURL = HistoryCSVDirectoryURLForMID(mid, NO);
    BOOL isDirectory = NO;
    BOOL hasCSV = NO;
    if (directoryURL && [fileManager fileExistsAtPath:directoryURL.path isDirectory:&isDirectory] && isDirectory) {
        NSError *readError = nil;
        NSArray<NSURL *> *files = [fileManager contentsOfDirectoryAtURL:directoryURL
                                             includingPropertiesForKeys:nil
                                                                options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                  error:&readError];
        if (readError) {
            NSLog(@"BILI粉丝数：读取历史 CSV 目录失败：%@", readError.localizedDescription);
        }
        for (NSURL *fileURL in files ?: @[]) {
            if ([fileURL.pathExtension.lowercaseString isEqualToString:@"csv"]) {
                hasCSV = YES;
                break;
            }
        }
    }
    if (hasCSV) return;

    NSMutableDictionary<NSString *, NSMutableString *> *payloads = [NSMutableDictionary new];
    for (NSDictionary *point in SortedUsableHistoryPoints(history)) {
        if ([point[@"mid"] longLongValue] != mid) continue;
        NSDate *date = [NSDate dateWithTimeIntervalSince1970:[point[@"updated_at"] doubleValue]];
        NSString *period = HistoryCSVPeriodIdentifierForDate(date);
        NSMutableString *payload = payloads[period];
        if (!payload) {
            payload = [NSMutableString stringWithString:@"time,uid,followers\n"];
            payloads[period] = payload;
        }
        [payload appendString:CSVLineForHistoryPoint(point)];
    }

    if (payloads.count == 0) return;
    NSURL *targetDirectoryURL = HistoryCSVDirectoryURLForMID(mid, YES);
    if (!targetDirectoryURL) return;
    for (NSString *period in payloads) {
        NSURL *fileURL = [targetDirectoryURL URLByAppendingPathComponent:[period stringByAppendingString:@".csv"]
                                                              isDirectory:NO];
        NSError *writeError = nil;
        if (![payloads[period] writeToURL:fileURL
                               atomically:YES
                                 encoding:NSUTF8StringEncoding
                                    error:&writeError]) {
            NSLog(@"BILI粉丝数：回填历史 CSV 失败：%@", writeError.localizedDescription);
        }
    }
}
+ (void)appendSegmentedHistoryCSVPoint:(NSDictionary *)point {
    if (!HistoryPointIsUsable(point)) return;
    long long mid = [point[@"mid"] longLongValue];
    if (mid <= 0) return;

    NSDate *date = [NSDate dateWithTimeIntervalSince1970:[point[@"updated_at"] doubleValue]];
    NSURL *fileURL = HistoryCSVFileURLForMIDAndDate(mid, date, YES);
    if (!fileURL) return;

    NSFileManager *fileManager = NSFileManager.defaultManager;
    BOOL fileExists = [fileManager fileExistsAtPath:fileURL.path];
    NSString *line = CSVLineForHistoryPoint(point);
    if (line.length == 0) return;
    NSString *payload = fileExists ? line : [@"time,uid,followers\n" stringByAppendingString:line];
    NSData *data = [payload dataUsingEncoding:NSUTF8StringEncoding];
    if (!data) return;

    if (!fileExists) {
        NSError *writeError = nil;
        if (![data writeToURL:fileURL options:NSDataWritingAtomic error:&writeError]) {
            NSLog(@"BILI粉丝数：写入历史 CSV 失败：%@", writeError.localizedDescription);
        }
        return;
    }

    NSError *openError = nil;
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:fileURL error:&openError];
    if (!handle) {
        NSLog(@"BILI粉丝数：打开历史 CSV 失败：%@", openError.localizedDescription);
        return;
    }
    @try {
        [handle seekToEndOfFile];
        [handle writeData:data];
    } @catch (NSException *exception) {
        NSLog(@"BILI粉丝数：追加历史 CSV 失败：%@", exception.reason);
    } @finally {
        if ([handle respondsToSelector:@selector(closeAndReturnError:)]) {
            NSError *closeError = nil;
            [handle closeAndReturnError:&closeError];
            if (closeError) NSLog(@"BILI粉丝数：关闭历史 CSV 失败：%@", closeError.localizedDescription);
        } else {
            [handle closeFile];
        }
    }
}
+ (void)saveHistoryPoint:(FanResult *)result {
    if (!result) return;
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *stored = [defaults arrayForKey:@"history_points"] ?: @[];
    NSMutableArray<NSDictionary *> *points = [NSMutableArray arrayWithArray:stored];

    NSDictionary *last = nil;
    for (NSDictionary *point in [points reverseObjectEnumerator]) {
        if ([point[@"mid"] longLongValue] == result.mid) {
            last = point;
            break;
        }
    }
    NSTimeInterval now = result.updatedAt.timeIntervalSince1970;
    if (last && [last[@"followers"] integerValue] == result.followers && now - [last[@"updated_at"] doubleValue] < 300) {
        return;
    }

    NSDictionary *newPoint = @{@"mid": @(result.mid),
                               @"followers": @(result.followers),
                               @"updated_at": @(now)};
    [points addObject:newPoint];
    [self appendSegmentedHistoryCSVPoint:newPoint];

    NSMutableDictionary<NSNumber *, NSMutableArray<NSDictionary *> *> *grouped = [NSMutableDictionary new];
    for (NSDictionary *point in points) {
        NSNumber *mid = @([point[@"mid"] longLongValue]);
        if (mid.longLongValue <= 0) continue;
        if (!grouped[mid]) grouped[mid] = [NSMutableArray new];
        [grouped[mid] addObject:point];
    }

    NSMutableDictionary<NSNumber *, NSArray<NSDictionary *> *> *normalizedGroups = [NSMutableDictionary new];
    for (NSNumber *mid in grouped) {
        NSArray<NSDictionary *> *sortedGroup = [grouped[mid] sortedArrayUsingComparator:^NSComparisonResult(NSDictionary *left, NSDictionary *right) {
            NSTimeInterval leftTime = [left[@"updated_at"] doubleValue];
            NSTimeInterval rightTime = [right[@"updated_at"] doubleValue];
            if (leftTime < rightTime) return NSOrderedAscending;
            if (leftTime > rightTime) return NSOrderedDescending;
            return NSOrderedSame;
        }];
        normalizedGroups[mid] = [self normalizedHistoryGroup:sortedGroup referenceDate:result.updatedAt];
    }

    NSArray<NSNumber *> *recentMIDs = [normalizedGroups.allKeys sortedArrayUsingComparator:^NSComparisonResult(NSNumber *left, NSNumber *right) {
        NSTimeInterval leftTime = [normalizedGroups[left].lastObject[@"updated_at"] doubleValue];
        NSTimeInterval rightTime = [normalizedGroups[right].lastObject[@"updated_at"] doubleValue];
        if (leftTime > rightTime) return NSOrderedAscending;
        if (leftTime < rightTime) return NSOrderedDescending;
        return NSOrderedSame;
    }];

    NSMutableArray<NSDictionary *> *trimmed = [NSMutableArray new];
    NSUInteger keptTargets = 0;
    for (NSNumber *mid in recentMIDs) {
        if (keptTargets >= MaxHistoryTargets) break;
        [trimmed addObjectsFromArray:normalizedGroups[mid]];
        keptTargets++;
    }
    [defaults setObject:trimmed forKey:@"history_points"];
    [self invalidateHistoryCache];
    FlushUserSettings();
}
+ (void)clearSegmentedHistoryCSVForMID:(long long)mid {
    NSURL *directoryURL = HistoryCSVDirectoryURLForMID(mid, NO);
    if (!directoryURL || ![NSFileManager.defaultManager fileExistsAtPath:directoryURL.path]) return;
    NSError *error = nil;
    if (![NSFileManager.defaultManager removeItemAtURL:directoryURL error:&error]) {
        NSLog(@"BILI粉丝数：清理历史 CSV 失败：%@", error.localizedDescription);
    }
}
+ (void)clearHistoryForCurrentTarget {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *stored = [defaults arrayForKey:@"history_points"] ?: @[];
    long long mid = [self mid];
    NSMutableArray<NSDictionary *> *kept = [NSMutableArray new];
    for (NSDictionary *point in stored) {
        if (![point isKindOfClass:NSDictionary.class]) continue;
        if ([point[@"mid"] longLongValue] != mid) {
            [kept addObject:point];
        }
    }
    [defaults setObject:kept forKey:@"history_points"];
    [self clearSegmentedHistoryCSVForMID:mid];
    [self invalidateHistoryCache];
    FlushUserSettings();
}
+ (NSInteger)appearanceMode {
    NSInteger mode = [[NSUserDefaults standardUserDefaults] integerForKey:@"appearance_mode"];
    if (mode == AppearanceModeClear || mode == AppearanceModeReadable) {
        return mode;
    }
    return AppearanceModeUltraClear;
}
+ (void)setAppearanceMode:(NSInteger)mode {
    NSInteger normalized = mode == AppearanceModeReadable ? AppearanceModeReadable : (mode == AppearanceModeClear ? AppearanceModeClear : AppearanceModeUltraClear);
    if ([self appearanceMode] == normalized) {
        return;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:normalized forKey:@"appearance_mode"];
    FlushUserSettings();
}
+ (NSInteger)statusDisplayMode {
    NSInteger mode = [[NSUserDefaults standardUserDefaults] integerForKey:@"status_display_mode"];
    if (mode == StatusDisplayModeCompact || mode == StatusDisplayModeIconOnly) {
        return mode;
    }
    return StatusDisplayModeExact;
}
+ (void)setStatusDisplayMode:(NSInteger)mode {
    NSInteger normalized = mode == StatusDisplayModeIconOnly
        ? StatusDisplayModeIconOnly
        : (mode == StatusDisplayModeCompact ? StatusDisplayModeCompact : StatusDisplayModeExact);
    if ([self statusDisplayMode] == normalized) {
        return;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:normalized forKey:@"status_display_mode"];
    FlushUserSettings();
}
+ (NSInteger)trendRange {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if (![defaults objectForKey:@"trend_range"]) {
        return TrendRangeThirtyDays;
    }
    NSInteger range = [defaults integerForKey:@"trend_range"];
    if (range == TrendRangeSevenDays || range == TrendRangeThirtyDays || range == TrendRangeNinetyDays) {
        return range;
    }
    if (range == 0) {
        return TrendRangeThirtyDays;
    }
    return TrendRangeNinetyDays;
}
+ (void)setTrendRange:(NSInteger)range {
    NSInteger normalized = range == TrendRangeNinetyDays
        ? TrendRangeNinetyDays
        : (range == TrendRangeSevenDays ? TrendRangeSevenDays : TrendRangeThirtyDays);
    if ([self trendRange] == normalized) {
        return;
    }
    [[NSUserDefaults standardUserDefaults] setInteger:normalized forKey:@"trend_range"];
    FlushUserSettings();
}
+ (NSTimeInterval)refreshInterval {
    NSTimeInterval interval = [[NSUserDefaults standardUserDefaults] doubleForKey:@"refresh_interval"];
    return interval >= 60 ? interval : DefaultRefreshInterval;
}
+ (void)setRefreshInterval:(NSTimeInterval)interval {
    NSTimeInterval normalized = interval >= 60 ? interval : DefaultRefreshInterval;
    if (fabs([self refreshInterval] - normalized) < 0.5) {
        return;
    }
    [[NSUserDefaults standardUserDefaults] setDouble:normalized forKey:@"refresh_interval"];
    FlushUserSettings();
}
+ (BOOL)autoRefreshEnabled {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return [defaults objectForKey:@"auto_refresh_enabled"] ? [defaults boolForKey:@"auto_refresh_enabled"] : YES;
}
+ (void)setAutoRefreshEnabled:(BOOL)enabled {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"auto_refresh_enabled"];
    if ((stored && [defaults boolForKey:@"auto_refresh_enabled"] == enabled) || (!stored && enabled)) {
        return;
    }
    [defaults setBool:enabled forKey:@"auto_refresh_enabled"];
    FlushUserSettings();
}
+ (BOOL)changeNotificationsEnabled {
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"change_notifications_enabled"];
}
+ (void)setChangeNotificationsEnabled:(BOOL)enabled {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"change_notifications_enabled"];
    if ((stored && [defaults boolForKey:@"change_notifications_enabled"] == enabled) || (!stored && !enabled)) {
        return;
    }
    [defaults setBool:enabled forKey:@"change_notifications_enabled"];
    FlushUserSettings();
}
+ (BOOL)alwaysOnTop {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    return [defaults objectForKey:@"always_on_top"] ? [defaults boolForKey:@"always_on_top"] : NO;
}
+ (void)setAlwaysOnTop:(BOOL)enabled {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"always_on_top"];
    if ((stored && [defaults boolForKey:@"always_on_top"] == enabled) || (!stored && !enabled)) {
        return;
    }
    [defaults setBool:enabled forKey:@"always_on_top"];
    FlushUserSettings();
}
+ (BOOL)positionLocked {
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"position_locked"];
}
+ (void)setPositionLocked:(BOOL)locked {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"position_locked"];
    if ((stored && [defaults boolForKey:@"position_locked"] == locked) || (!stored && !locked)) {
        return;
    }
    [defaults setBool:locked forKey:@"position_locked"];
    FlushUserSettings();
}
+ (BOOL)launchHidden {
    return [[NSUserDefaults standardUserDefaults] boolForKey:@"launch_hidden"];
}
+ (void)setLaunchHidden:(BOOL)hidden {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id stored = [defaults objectForKey:@"launch_hidden"];
    if ((stored && [defaults boolForKey:@"launch_hidden"] == hidden) || (!stored && !hidden)) {
        return;
    }
    [defaults setBool:hidden forKey:@"launch_hidden"];
    FlushUserSettings();
}
+ (NSPoint)windowOriginWithDefaultFrame:(NSRect)frame {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:@"window_x"] && [defaults objectForKey:@"window_y"]) {
        NSRect visible = NSScreen.mainScreen.visibleFrame;
        CGFloat x = [defaults doubleForKey:@"window_x"];
        CGFloat y = [defaults doubleForKey:@"window_y"];
        if (!NSIntersectsRect(NSMakeRect(x, y, frame.size.width, frame.size.height), visible)) {
            return frame.origin;
        }
        CGFloat maxX = MAX(NSMinX(visible), NSMaxX(visible) - frame.size.width);
        CGFloat maxY = MAX(NSMinY(visible), NSMaxY(visible) - frame.size.height);
        x = MIN(MAX(x, NSMinX(visible)), maxX);
        y = MIN(MAX(y, NSMinY(visible)), maxY);
        return NSMakePoint(x, y);
    }
    return frame.origin;
}
+ (void)saveWindowFrame:(NSRect)frame {
    if (NSIsEmptyRect(frame)
        || !isfinite(frame.origin.x)
        || !isfinite(frame.origin.y)
        || !isfinite(frame.size.width)
        || !isfinite(frame.size.height)) {
        return;
    }
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:@"window_x"]
        && [defaults objectForKey:@"window_y"]
        && fabs([defaults doubleForKey:@"window_x"] - frame.origin.x) < 0.5
        && fabs([defaults doubleForKey:@"window_y"] - frame.origin.y) < 0.5) {
        return;
    }
    [defaults setDouble:frame.origin.x forKey:@"window_x"];
    [defaults setDouble:frame.origin.y forKey:@"window_y"];
    FlushUserSettings();
}
@end

@interface BiliFansClient : NSObject
+ (void)fetchMID:(long long)mid fallbackName:(NSString *)fallbackName completion:(void (^)(FanResult *result, NSError *error))completion;
@end

@implementation BiliFansClient

+ (BOOL)shouldTryFollowersFallbackAfterCardError:(NSError *)error {
    if (!error) {
        return YES;
    }
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        return NO;
    }
    return YES;
}

+ (void)fetchMID:(long long)mid fallbackName:(NSString *)fallbackName completion:(void (^)(FanResult *, NSError *))completion {
    [self fetchCardMID:mid fallbackName:fallbackName completion:^(FanResult *result, NSError *error) {
        if (result) {
            completion(result, nil);
            return;
        }
        if (![self shouldTryFollowersFallbackAfterCardError:error]) {
            completion(nil, error);
            return;
        }
        [self fetchFollowersMID:mid fallbackName:fallbackName completion:completion];
    }];
}

+ (void)fetchCardMID:(long long)mid fallbackName:(NSString *)fallbackName completion:(void (^)(FanResult *, NSError *))completion {
    NSString *url = [NSString stringWithFormat:@"https://api.bilibili.com/x/web-interface/card?mid=%lld&photo=false", mid];
    [self get:url completion:^(NSDictionary *root, NSError *error) {
        if (error) {
            completion(nil, error);
            return;
        }
        if (![root[@"code"] isEqual:@0]) {
            completion(nil, BiliAPIError(root, @"资料接口返回异常", 1));
            return;
        }
        NSDictionary *data = [root[@"data"] isKindOfClass:NSDictionary.class] ? root[@"data"] : nil;
        NSDictionary *card = [data[@"card"] isKindOfClass:NSDictionary.class] ? data[@"card"] : nil;
        NSNumber *fans = [card[@"fans"] isKindOfClass:NSNumber.class] ? card[@"fans"] : nil;
        if (!fans && [data[@"follower"] isKindOfClass:NSNumber.class]) {
            fans = data[@"follower"];
        }
        if (!fans) {
            completion(nil, nil);
            return;
        }
        FanResult *result = [FanResult new];
        result.mid = mid;
        result.name = CleanName([card[@"name"] isKindOfClass:NSString.class] ? card[@"name"] : fallbackName, mid);
        result.followers = fans.integerValue;
        result.updatedAt = [NSDate date];
        completion(result, nil);
    }];
}

+ (void)fetchFollowersMID:(long long)mid fallbackName:(NSString *)fallbackName completion:(void (^)(FanResult *, NSError *))completion {
    NSString *url = [NSString stringWithFormat:@"https://api.bilibili.com/x/relation/stat?vmid=%lld", mid];
    [self get:url completion:^(NSDictionary *root, NSError *error) {
        if (error) {
            completion(nil, error);
            return;
        }
        if (![root[@"code"] isEqual:@0]) {
            completion(nil, BiliAPIError(root, @"关注者接口返回异常", 2));
            return;
        }
        NSDictionary *data = [root[@"data"] isKindOfClass:NSDictionary.class] ? root[@"data"] : nil;
        NSNumber *followers = [data[@"follower"] isKindOfClass:NSNumber.class] ? data[@"follower"] : nil;
        if (!followers) {
            completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:3 userInfo:@{NSLocalizedDescriptionKey: @"关注者数据缺失"}]);
            return;
        }
        FanResult *result = [FanResult new];
        result.mid = mid;
        result.name = CleanName(fallbackName, mid);
        result.followers = followers.integerValue;
        result.updatedAt = [NSDate date];
        completion(result, nil);
    }];
}

+ (void)get:(NSString *)address completion:(void (^)(NSDictionary *, NSError *))completion {
    NSURL *url = [NSURL URLWithString:address];
    if (!url) {
        completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:4 userInfo:@{NSLocalizedDescriptionKey: @"请求地址无效"}]);
        return;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:10];
    [request setValue:[NSString stringWithFormat:@"Mozilla/5.0 macOS BILIFansCard/%@", AppVersionString()] forHTTPHeaderField:@"User-Agent"];
    [request setValue:@"https://www.bilibili.com/" forHTTPHeaderField:@"Referer"];
    [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (error) {
            completion(nil, error);
            return;
        }
        if (![response isKindOfClass:NSHTTPURLResponse.class]) {
            completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:5 userInfo:@{NSLocalizedDescriptionKey: @"响应类型异常"}]);
            return;
        }
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        if (http.statusCode < 200 || http.statusCode >= 300) {
            completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:http.statusCode userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"HTTP %ld", (long)http.statusCode]}]);
            return;
        }
        if (data.length == 0) {
            completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:6 userInfo:@{NSLocalizedDescriptionKey: @"响应为空"}]);
            return;
        }
        id parsed = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
        if (![parsed isKindOfClass:NSDictionary.class]) {
            completion(nil, error ?: [NSError errorWithDomain:@"BILIFansCard" code:7 userInfo:@{NSLocalizedDescriptionKey: @"JSON 格式异常"}]);
            return;
        }
        completion(parsed, nil);
    }];
    [task resume];
}
@end

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
        _refreshButton.hidden = YES;
        _closeButton.hidden = YES;
        return;
    }
    _refreshButton.hidden = NO;
    _closeButton.hidden = NO;
    _refreshButton.enabled = !self.refreshing;
    _refreshButton.alphaValue = self.refreshing ? 0.26 : (_hovering ? 0.92 : 0.36);
    _closeButton.alphaValue = _hovering ? 0.98 : 0.64;
}

- (void)setName:(NSString *)name {
    _name = [name copy];
    self.needsDisplay = YES;
}
- (void)setStatus:(NSString *)status {
    _status = [status copy];
    self.needsDisplay = YES;
}
- (void)setUpdatedText:(NSString *)updatedText {
    _updatedText = [updatedText copy];
    self.needsDisplay = YES;
}
- (void)setAppearanceMode:(NSInteger)appearanceMode {
    if (appearanceMode == AppearanceModeClear || appearanceMode == AppearanceModeReadable) {
        _appearanceMode = appearanceMode;
    } else {
        _appearanceMode = AppearanceModeUltraClear;
    }
    self.needsDisplay = YES;
}
- (void)setFollowerDelta:(NSInteger)followerDelta {
    _followerDelta = followerDelta;
    self.needsDisplay = YES;
}
- (void)setHasFollowerDelta:(BOOL)hasFollowerDelta {
    _hasFollowerDelta = hasFollowerDelta;
    self.needsDisplay = YES;
}
- (void)setHistoryValues:(NSArray<NSNumber *> *)historyValues {
    _historyValues = [historyValues copy] ?: @[];
    self.needsDisplay = YES;
}
- (void)setMid:(long long)mid {
    _mid = mid;
}
- (void)setShowsInlineControls:(BOOL)showsInlineControls {
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
    item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
    return item;
}

- (BOOL)validateMenuItem:(NSMenuItem *)item {
    if (item.action == @selector(refresh)) {
        item.title = self.refreshing ? @"正在刷新" : @"刷新";
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
    if (!AppAppearanceIsDark()) {
        DrawLiquidGlassBackdrop(rect, 24, NSColor.controlAccentColor, self.appearanceMode, 1.0);
        return;
    }

    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL clear = effectiveMode == AppearanceModeClear;
    BOOL readable = effectiveMode == AppearanceModeReadable;
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:24 yRadius:24];
    [NSGraphicsContext saveGraphicsState];
    [shape addClip];

    NSGradient *wash = [[NSGradient alloc] initWithColors:@[
        [NSColor colorWithCalibratedRed:0.25 green:0.27 blue:0.31 alpha:(readable ? 0.30 : (clear ? 0.18 : 0.10))],
        [NSColor colorWithCalibratedRed:0.16 green:0.18 blue:0.22 alpha:(readable ? 0.24 : (clear ? 0.14 : 0.08))],
        [NSColor colorWithCalibratedRed:0.10 green:0.12 blue:0.16 alpha:(readable ? 0.22 : (clear ? 0.12 : 0.07))]
    ]];
    [wash drawInRect:rect angle:-18];

    NSRect upperGlowRect = NSMakeRect(NSMinX(rect) - rect.size.width * 0.12,
                                      NSMaxY(rect) - rect.size.height * 0.58,
                                      rect.size.width * 0.74,
                                      rect.size.height * 0.58);
    NSBezierPath *upperGlow = [NSBezierPath bezierPathWithOvalInRect:upperGlowRect];
    NSGradient *upperSheen = [[NSGradient alloc] initWithStartingColor:[[NSColor whiteColor] colorWithAlphaComponent:(readable ? 0.070 : (clear ? 0.046 : 0.030))]
                                                           endingColor:[[NSColor whiteColor] colorWithAlphaComponent:0.0]];
    [upperSheen drawInBezierPath:upperGlow relativeCenterPosition:NSMakePoint(-0.25, 0.18)];

    NSRect accentRect = NSMakeRect(NSMaxX(rect) - rect.size.width * 0.52,
                                   NSMinY(rect) - rect.size.height * 0.18,
                                   rect.size.width * 0.68,
                                   rect.size.height * 0.62);
    NSBezierPath *accentGlow = [NSBezierPath bezierPathWithOvalInRect:accentRect];
    NSGradient *accentSheen = [[NSGradient alloc] initWithStartingColor:[NSColor.controlAccentColor colorWithAlphaComponent:(readable ? 0.055 : (clear ? 0.034 : 0.020))]
                                                            endingColor:[NSColor.controlAccentColor colorWithAlphaComponent:0.0]];
    [accentSheen drawInBezierPath:accentGlow relativeCenterPosition:NSMakePoint(0.20, -0.12)];

    [NSGraphicsContext restoreGraphicsState];
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
    if (self.historyValues.count < 2) return;

    NSInteger minValue = NSIntegerMax;
    NSInteger maxValue = NSIntegerMin;
    for (NSNumber *value in self.historyValues) {
        NSInteger followers = value.integerValue;
        minValue = MIN(minValue, followers);
        maxValue = MAX(maxValue, followers);
    }
    NSInteger delta = self.historyValues.lastObject.integerValue - self.historyValues.firstObject.integerValue;
    NSColor *trendColor = delta > 0 ? NSColor.systemGreenColor : (delta < 0 ? NSColor.systemRedColor : NSColor.controlAccentColor);
    CGFloat range = MAX(1.0, (CGFloat)(maxValue - minValue));
    CGFloat step = rect.size.width / (CGFloat)(self.historyValues.count - 1);

    NSBezierPath *line = [NSBezierPath bezierPath];
    NSPoint lastPoint = NSZeroPoint;
    for (NSUInteger i = 0; i < self.historyValues.count; i++) {
        CGFloat x = NSMinX(rect) + step * (CGFloat)i;
        CGFloat normalized = ((CGFloat)self.historyValues[i].integerValue - (CGFloat)minValue) / range;
        CGFloat y = NSMinY(rect) + 4 + normalized * (rect.size.height - 8);
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

    NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(lastPoint.x - 2.2, lastPoint.y - 2.2, 4.4, 4.4)];
    [[trendColor colorWithAlphaComponent:0.72] setFill];
    [dot fill];
}

- (void)drawTrendSummaryInRect:(NSRect)rect {
    if (self.historyValues.count < 2) return;
    NSInteger first = self.historyValues.firstObject.integerValue;
    NSInteger last = self.historyValues.lastObject.integerValue;
    NSInteger delta = last - first;
    NSString *text = CompactSignedDelta(delta);
    NSDictionary *attrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightSemibold],
                            NSForegroundColorAttributeName: FollowerDeltaColor(delta),
                            NSShadowAttributeName: [self shadowWithAlpha:0.14 blur:1.5 y:-1]};
    NSSize size = [text sizeWithAttributes:attrs];
    CGFloat x = NSMaxX(rect) - size.width;
    CGFloat y = NSMidY(rect) - size.height / 2.0 + 1.0;
    [text drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
}

- (void)drawLabels {
    NSDictionary *nameAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:18 weight:NSFontWeightBold],
                                NSForegroundColorAttributeName: NSColor.labelColor,
                                NSShadowAttributeName: [self shadowWithAlpha:0.22 blur:2.0 y:-1]};
    [self.name drawAtPoint:NSMakePoint(24, self.bounds.size.height - 45) withAttributes:nameAttrs];

    NSString *number = self.hasFollowers ? DecimalStringForInteger(self.followers) : @"--";
    NSDictionary *numberAttrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:56 weight:NSFontWeightSemibold],
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
        NSDictionary *deltaAttrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:15 weight:NSFontWeightSemibold],
                                     NSForegroundColorAttributeName: deltaColor,
                                     NSShadowAttributeName: [self shadowWithAlpha:0.24 blur:2.5 y:-1]};
        [delta drawAtPoint:NSMakePoint(numberX + numberSize.width + 8, 96) withAttributes:deltaAttrs];
    }

    NSDictionary *statusAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightBold],
                                  NSForegroundColorAttributeName: NSColor.secondaryLabelColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.20 blur:2.0 y:-1]};
    [self.status drawAtPoint:NSMakePoint(24, 52) withAttributes:statusAttrs];

    NSDictionary *updatedAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightMedium],
                                   NSForegroundColorAttributeName: NSColor.tertiaryLabelColor,
                                   NSShadowAttributeName: [self shadowWithAlpha:0.18 blur:2.0 y:-1]};
    NSSize updatedSize = [self.updatedText sizeWithAttributes:updatedAttrs];
    [self.updatedText drawAtPoint:NSMakePoint(self.bounds.size.width - updatedSize.width - 24, 52) withAttributes:updatedAttrs];
}

- (NSShadow *)shadowWithAlpha:(CGFloat)alpha blur:(CGFloat)blur y:(CGFloat)y {
    NSShadow *shadow = [NSShadow new];
    shadow.shadowColor = [[NSColor shadowColor] colorWithAlphaComponent:alpha];
    shadow.shadowBlurRadius = blur;
    shadow.shadowOffset = NSMakeSize(0, y);
    return shadow;
}

- (NSImage *)refreshImage {
    NSImage *image = [NSImage imageWithSystemSymbolName:@"arrow.clockwise" accessibilityDescription:@"刷新"];
    if (image) {
        image.template = YES;
        return image;
    }
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
    NSImage *image = [NSImage imageWithSystemSymbolName:@"power.circle.fill" accessibilityDescription:@"退出"];
    if (image) {
        image.template = YES;
        return image;
    }
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
    NSGradient *wash = dark
        ? [[NSGradient alloc] initWithColors:@[
            [[NSColor colorWithCalibratedRed:0.070 green:0.078 blue:0.092 alpha:1.0] colorWithAlphaComponent:(readable ? 0.70 : (clear ? 0.46 : 0.30))],
            [[NSColor colorWithCalibratedRed:0.035 green:0.040 blue:0.050 alpha:1.0] colorWithAlphaComponent:(readable ? 0.68 : (clear ? 0.44 : 0.28))],
            [[NSColor colorWithCalibratedRed:0.020 green:0.024 blue:0.032 alpha:1.0] colorWithAlphaComponent:(readable ? 0.74 : (clear ? 0.48 : 0.32))]
        ]]
        : [[NSGradient alloc] initWithColors:@[
            [[NSColor controlBackgroundColor] colorWithAlphaComponent:(readable ? 0.075 : (clear ? 0.026 : 0.004))],
            [[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.042 : (clear ? 0.014 : 0.002))],
            [[NSColor underPageBackgroundColor] colorWithAlphaComponent:(readable ? 0.050 : (clear ? 0.016 : 0.003))]
        ]];
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
    DrawFloatingGlassShadow(rect, radius, self.appearanceMode, (dark && leftPanel) ? 0.76 : 1.0);
    DrawLiquidGlassBackdrop(rect, radius, accent, self.appearanceMode, (dark && leftPanel) ? 0.56 : 0.82);

    if (!(dark && leftPanel) && strokeAlpha > 0.018) {
        [[[NSColor separatorColor] colorWithAlphaComponent:strokeAlpha] setStroke];
        shape.lineWidth = 0.8;
        [shape stroke];
    }
    DrawLiquidGlassRim(rect, radius, accent, rimStrength);
}

- (void)drawGlassControl:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent emphasized:(BOOL)emphasized {
    if (AppAppearanceIsDark() && NSMinX(rect) < 500) {
        DrawLiquidGlassBackdrop(rect, radius, accent, self.appearanceMode, emphasized ? 0.42 : 0.22);
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
    [self drawGlassControl:NSMakeRect(304, 86, 164, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(304, 26, 164, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(544, 424, 240, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(788, 424, 74, 36) radius:13 accent:NSColor.controlAccentColor emphasized:YES];
    [self drawGlassControl:NSMakeRect(516, 316, 348, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(516, 280, 168, 32) radius:12 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(696, 280, 168, 32) radius:12 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(516, 206, 348, 36) radius:13 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(516, 170, 168, 32) radius:12 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(696, 170, 168, 32) radius:12 accent:NSColor.controlAccentColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(516, 100, 112, 32) radius:12 accent:NSColor.systemGrayColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(634, 100, 112, 32) radius:12 accent:NSColor.systemGrayColor emphasized:NO];
    [self drawGlassControl:NSMakeRect(752, 100, 112, 32) radius:12 accent:NSColor.systemGrayColor emphasized:NO];
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
    NSGradient *wash = [[NSGradient alloc] initWithColors:@[
        [[NSColor controlBackgroundColor] colorWithAlphaComponent:(readable ? 0.074 : (clear ? 0.026 : 0.004))],
        [[NSColor windowBackgroundColor] colorWithAlphaComponent:(readable ? 0.040 : (clear ? 0.014 : 0.002))],
        [[NSColor underPageBackgroundColor] colorWithAlphaComponent:(readable ? 0.050 : (clear ? 0.016 : 0.003))]
    ]];
    [wash drawInRect:self.bounds angle:-24];

    [self drawPanel:NSMakeRect(18, 292, 444, 92) radius:22 accent:NSColor.systemPinkColor];
    [self drawPanel:NSMakeRect(18, 176, 444, 102) radius:20 accent:NSColor.controlAccentColor];
    [self drawPanel:NSMakeRect(18, 34, 444, 128) radius:20 accent:NSColor.systemBlueColor];
    [self drawControls];
}

- (void)drawPanel:(NSRect)rect radius:(CGFloat)radius accent:(NSColor *)accent {
    DrawLiquidGlassBackdrop(rect, radius, accent, self.appearanceMode, 0.78);
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
    _appearanceControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    _appearanceControl.segmentCount = 3;
    [_appearanceControl setLabel:@"极透" forSegment:0];
    [_appearanceControl setLabel:@"清透" forSegment:1];
    [_appearanceControl setLabel:@"可读" forSegment:2];
    _appearanceControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    _appearanceControl.target = self;
    _appearanceControl.action = @selector(appearanceChanged:);
    _appearanceControl.toolTip = @"玻璃外观";
    [self styleGlassSegmentedControl:_appearanceControl];

    _statusDisplayControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    _statusDisplayControl.segmentCount = 3;
    [_statusDisplayControl setLabel:@"精确" forSegment:0];
    [_statusDisplayControl setLabel:@"紧凑" forSegment:1];
    [_statusDisplayControl setLabel:@"图标" forSegment:2];
    _statusDisplayControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    _statusDisplayControl.target = self;
    _statusDisplayControl.action = @selector(statusDisplayChanged:);
    _statusDisplayControl.toolTip = @"顶栏显示方式";
    [self styleGlassSegmentedControl:_statusDisplayControl];

    _intervalControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
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
    NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:tooltip] ?: [self fallbackIconWithText:[tooltip substringToIndex:1]];
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
    _menuBarOnlyButton.state = menuBarOnly ? NSControlStateValueOn : NSControlStateValueOff;
    if (changed) self.needsDisplay = YES;
}
- (void)setCopyEnabled:(BOOL)copyEnabled {
    BOOL changed = _copyEnabled != copyEnabled;
    _copyEnabled = copyEnabled;
    _copyButton.enabled = copyEnabled;
    _copyButton.alphaValue = copyEnabled ? 1.0 : 0.35;
    if (changed) self.needsDisplay = YES;
}
- (void)setHistoryEnabled:(BOOL)historyEnabled {
    BOOL changed = _historyEnabled != historyEnabled;
    _historyEnabled = historyEnabled;
    _historyButton.enabled = historyEnabled;
    _historyButton.alphaValue = historyEnabled ? 1.0 : 0.35;
    if (changed) self.needsDisplay = YES;
}
- (void)setProfileEnabled:(BOOL)profileEnabled {
    BOOL changed = _profileEnabled != profileEnabled;
    _profileEnabled = profileEnabled;
    _profileButton.enabled = profileEnabled;
    _profileButton.alphaValue = profileEnabled ? 1.0 : 0.35;
    if (changed) self.needsDisplay = YES;
}
- (void)setRefreshing:(BOOL)refreshing {
    BOOL changed = _refreshing != refreshing;
    _refreshing = refreshing;
    _refreshButton.enabled = !refreshing;
    _refreshButton.alphaValue = refreshing ? 0.35 : 1.0;
    _refreshButton.toolTip = refreshing ? @"正在刷新" : @"刷新";
    if (changed) self.needsDisplay = YES;
}
- (void)setNotificationsEnabled:(BOOL)notificationsEnabled {
    BOOL changed = _notificationsEnabled != notificationsEnabled;
    _notificationsEnabled = notificationsEnabled;
    _notificationButton.contentTintColor = notificationsEnabled
        ? NSColor.controlAccentColor
        : NSColor.secondaryLabelColor;
    if (changed) self.needsDisplay = YES;
}
- (void)setLaunchAtLoginEnabled:(BOOL)launchAtLoginEnabled {
    BOOL changed = _launchAtLoginEnabled != launchAtLoginEnabled;
    _launchAtLoginEnabled = launchAtLoginEnabled;
    _launchAtLoginButton.state = launchAtLoginEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    if (changed) self.needsDisplay = YES;
}
- (void)setLaunchAtLoginAvailable:(BOOL)launchAtLoginAvailable {
    BOOL changed = _launchAtLoginAvailable != launchAtLoginAvailable;
    _launchAtLoginAvailable = launchAtLoginAvailable;
    _launchAtLoginButton.enabled = launchAtLoginAvailable;
    _launchAtLoginButton.alphaValue = launchAtLoginAvailable ? 1.0 : 0.35;
    _launchAtLoginButton.toolTip = launchAtLoginAvailable
        ? @"登录后自动启动"
        : @"登录启动不可用";
    if (changed) self.needsDisplay = YES;
}
- (void)setRefreshInterval:(NSTimeInterval)refreshInterval {
    NSTimeInterval next = refreshInterval >= 60 ? refreshInterval : 60;
    BOOL changed = _refreshInterval != next;
    _refreshInterval = next;
    if (_refreshInterval <= 90) {
        _intervalControl.selectedSegment = 0;
    } else if (_refreshInterval <= 450) {
        _intervalControl.selectedSegment = 1;
    } else {
        _intervalControl.selectedSegment = 2;
    }
    if (changed) self.needsDisplay = YES;
}
- (void)setAutoRefreshEnabled:(BOOL)autoRefreshEnabled {
    BOOL changed = _autoRefreshEnabled != autoRefreshEnabled;
    _autoRefreshEnabled = autoRefreshEnabled;
    _autoRefreshButton.state = autoRefreshEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    _intervalControl.enabled = autoRefreshEnabled;
    _intervalControl.alphaValue = autoRefreshEnabled ? 1.0 : 0.42;
    if (changed) self.needsDisplay = YES;
}
- (void)setStatusDisplayMode:(NSInteger)statusDisplayMode {
    NSInteger normalized = statusDisplayMode == StatusDisplayModeIconOnly
        ? StatusDisplayModeIconOnly
        : (statusDisplayMode == StatusDisplayModeCompact ? StatusDisplayModeCompact : StatusDisplayModeExact);
    BOOL changed = _statusDisplayMode != normalized;
    if (statusDisplayMode == StatusDisplayModeIconOnly) {
        _statusDisplayMode = StatusDisplayModeIconOnly;
        _statusDisplayControl.selectedSegment = 2;
    } else if (statusDisplayMode == StatusDisplayModeCompact) {
        _statusDisplayMode = StatusDisplayModeCompact;
        _statusDisplayControl.selectedSegment = 1;
    } else {
        _statusDisplayMode = StatusDisplayModeExact;
        _statusDisplayControl.selectedSegment = 0;
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
        _appearanceControl.selectedSegment = 2;
    } else if (appearanceMode == AppearanceModeClear) {
        _appearanceMode = AppearanceModeClear;
        _appearanceControl.selectedSegment = 1;
    } else {
        _appearanceMode = AppearanceModeUltraClear;
        _appearanceControl.selectedSegment = 0;
    }
    if (changed) self.needsDisplay = YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    NSRect rect = NSInsetRect(self.bounds, 3, 3);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:20 yRadius:20];
    NSInteger effectiveMode = EffectiveAppearanceMode(self.appearanceMode);
    BOOL readable = effectiveMode == AppearanceModeReadable;
    [NSGraphicsContext saveGraphicsState];
    [shape addClip];
    DrawLiquidGlassBackdrop(rect, 20, NSColor.controlAccentColor, self.appearanceMode, readable ? 0.92 : 0.86);
    [NSGraphicsContext restoreGraphicsState];

    DrawLiquidGlassRim(rect, 20, NSColor.controlAccentColor, readable ? 0.68 : (effectiveMode == AppearanceModeClear ? 0.54 : 0.46));

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
    NSDictionary *nameAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:13 weight:NSFontWeightBold],
                                NSForegroundColorAttributeName: NSColor.labelColor,
                                NSShadowAttributeName: [self shadowWithAlpha:0.20 blur:2.0 y:-1]};
    NSString *name = self.name.length > 0 ? self.name : @"BILI粉丝数";
    NSSize nameSize = [name sizeWithAttributes:nameAttrs];
    [name drawAtPoint:NSMakePoint(49, headerCenterY - nameSize.height / 2.0) withAttributes:nameAttrs];

    if (self.menuBarOnly || !self.autoRefreshEnabled || self.refreshScheduleText.length > 0) {
        NSDictionary *badgeAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:10 weight:NSFontWeightSemibold],
                                     NSForegroundColorAttributeName: NSColor.secondaryLabelColor};
        NSString *badge = !self.autoRefreshEnabled
            ? @"已暂停"
            : (self.refreshScheduleText.length > 0 ? self.refreshScheduleText : @"仅顶栏");
        NSSize size = [badge sizeWithAttributes:badgeAttrs];
        NSRect badgeRect = NSMakeRect(self.bounds.size.width - size.width - 30, headerCenterY - 10.0, size.width + 14, 20);
        DrawLiquidGlassControlWell(badgeRect, 10, NSColor.controlAccentColor, self.appearanceMode, NO);
        [badge drawAtPoint:NSMakePoint(NSMinX(badgeRect) + 7, headerCenterY - size.height / 2.0) withAttributes:badgeAttrs];
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
    DrawCompactBMark(NSMakeRect(point.x + 5.2, point.y + 3.2, 9.2, 10.8),
                     NSColor.alternateSelectedControlTextColor,
                     1.75);
}

- (void)drawFollowerNumber {
    CGFloat lift = MAX(0, self.bounds.size.height - 226);
    NSString *number = self.hasFollowers ? DecimalStringForInteger(self.followers) : @"--";
    NSDictionary *numberAttrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:39 weight:NSFontWeightSemibold],
                                  NSForegroundColorAttributeName: NSColor.controlAccentColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.24 blur:3.0 y:-1]};
    NSSize numberSize = [number sizeWithAttributes:numberAttrs];
    [number drawAtPoint:NSMakePoint((self.bounds.size.width - numberSize.width) / 2.0, 122 + lift) withAttributes:numberAttrs];

    if (self.hasFollowerDelta && self.followerDelta != 0) {
        NSString *delta = [NSString stringWithFormat:@"%@%ld", self.followerDelta > 0 ? @"+" : @"", (long)self.followerDelta];
        NSColor *color = self.followerDelta > 0
            ? NSColor.systemGreenColor
            : NSColor.systemRedColor;
        NSDictionary *deltaAttrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:13 weight:NSFontWeightBold],
                                     NSForegroundColorAttributeName: color,
                                     NSShadowAttributeName: [self shadowWithAlpha:0.56 blur:3.2 y:-1]};
        [delta drawAtPoint:NSMakePoint((self.bounds.size.width + numberSize.width) / 2.0 + 8, 143 + lift) withAttributes:deltaAttrs];
    }

    NSDictionary *statusAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:12 weight:NSFontWeightSemibold],
                                  NSForegroundColorAttributeName: NSColor.secondaryLabelColor,
                                  NSShadowAttributeName: [self shadowWithAlpha:0.18 blur:2.0 y:-1]};
    NSString *status = self.status.length > 0 ? self.status : @"等待刷新";
    [status drawAtPoint:NSMakePoint(24, 110 + lift) withAttributes:statusAttrs];

    NSDictionary *updatedAttrs = @{NSFontAttributeName: [NSFont systemFontOfSize:12 weight:NSFontWeightMedium],
                                   NSForegroundColorAttributeName: NSColor.tertiaryLabelColor,
                                   NSShadowAttributeName: [self shadowWithAlpha:0.16 blur:2.0 y:-1]};
    NSString *updated = self.updatedText.length > 0 ? self.updatedText : @"--";
    NSSize updatedSize = [updated sizeWithAttributes:updatedAttrs];
    [updated drawAtPoint:NSMakePoint(self.bounds.size.width - updatedSize.width - 24, 110 + lift) withAttributes:updatedAttrs];
}

- (void)drawSparklineInRect:(NSRect)rect {
    if (self.historyValues.count < 2) return;
    NSInteger minValue = NSIntegerMax;
    NSInteger maxValue = NSIntegerMin;
    for (NSNumber *value in self.historyValues) {
        NSInteger followers = value.integerValue;
        minValue = MIN(minValue, followers);
        maxValue = MAX(maxValue, followers);
    }
    NSInteger delta = self.historyValues.lastObject.integerValue - self.historyValues.firstObject.integerValue;
    NSColor *trendColor = delta > 0 ? NSColor.systemGreenColor : (delta < 0 ? NSColor.systemRedColor : NSColor.controlAccentColor);
    CGFloat range = MAX(1.0, (CGFloat)(maxValue - minValue));
    CGFloat step = rect.size.width / (CGFloat)(self.historyValues.count - 1);
    NSBezierPath *line = [NSBezierPath bezierPath];
    NSPoint lastPoint = NSZeroPoint;
    for (NSUInteger i = 0; i < self.historyValues.count; i++) {
        CGFloat x = NSMinX(rect) + step * (CGFloat)i;
        CGFloat normalized = ((CGFloat)self.historyValues[i].integerValue - (CGFloat)minValue) / range;
        CGFloat y = NSMinY(rect) + 3 + normalized * (rect.size.height - 6);
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
    line.lineCapStyle = NSLineCapStyleRound;
    line.lineJoinStyle = NSLineJoinStyleRound;
    [[trendColor colorWithAlphaComponent:0.50] setStroke];
    [line stroke];

    NSBezierPath *dot = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(lastPoint.x - 2.0, lastPoint.y - 2.0, 4.0, 4.0)];
    [[trendColor colorWithAlphaComponent:0.70] setFill];
    [dot fill];
}

- (void)drawTrendSummaryInRect:(NSRect)rect {
    if (self.historyValues.count < 2) return;
    NSInteger first = self.historyValues.firstObject.integerValue;
    NSInteger last = self.historyValues.lastObject.integerValue;
    NSInteger delta = last - first;
    NSString *text = CompactSignedDelta(delta);
    NSDictionary *attrs = @{NSFontAttributeName: [NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightSemibold],
                            NSForegroundColorAttributeName: FollowerDeltaColor(delta),
                            NSShadowAttributeName: [self shadowWithAlpha:0.16 blur:2 y:-1]};
    NSSize size = [text sizeWithAttributes:attrs];
    CGFloat x = NSMaxX(rect) - size.width;
    CGFloat y = NSMidY(rect) - size.height / 2.0 + 1.0;
    [text drawAtPoint:NSMakePoint(x, y) withAttributes:attrs];
}

- (void)drawActionBackplates {
    NSArray<NSButton *> *buttons = @[_refreshButton, _copyButton, _profileButton, _historyButton, _notificationButton, _settingsButton, _showCardButton, _quitButton];
    BOOL dark = AppAppearanceIsDark();
    for (NSButton *button in buttons) {
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(button.frame, 1, 1) xRadius:12 yRadius:12];
        BOOL activeNotification = button == _notificationButton && self.notificationsEnabled;
        BOOL destructive = button == _quitButton;
        CGFloat fillAlpha = dark
            ? (activeNotification || destructive ? 0.120 : (button.enabled ? 0.030 : 0.014))
            : (activeNotification || destructive ? 0.105 : (button.enabled ? 0.045 : 0.024));
        CGFloat strokeAlpha = dark
            ? (activeNotification || destructive ? 0.145 : (button.enabled ? 0.070 : 0.032))
            : (activeNotification || destructive ? 0.17 : (button.enabled ? 0.115 : 0.052));
        NSColor *fillColor = activeNotification
            ? NSColor.controlAccentColor
            : (destructive ? NSColor.systemRedColor : (dark ? [NSColor colorWithCalibratedRed:0.06 green:0.07 blue:0.09 alpha:1.0] : NSColor.controlBackgroundColor));
        [[fillColor colorWithAlphaComponent:fillAlpha] setFill];
        [path fill];
        [[[NSColor separatorColor] colorWithAlphaComponent:strokeAlpha] setStroke];
        path.lineWidth = 0.7;
        [path stroke];
        if (button.enabled) {
            DrawLiquidGlassControlWell(NSInsetRect(button.frame, 1.5, 1.5),
                                       12,
                                       activeNotification ? NSColor.controlAccentColor : (destructive ? NSColor.systemRedColor : NSColor.controlAccentColor),
                                       self.appearanceMode,
                                       activeNotification || destructive);
        }
    }
}

- (void)drawQuickControlBackplates {
    BOOL dark = AppAppearanceIsDark();
    NSArray<NSValue *> *baseRects = @[
        [NSValue valueWithRect:NSInsetRect(_appearanceControl.frame, -4, -4)],
        [NSValue valueWithRect:NSInsetRect(_statusDisplayControl.frame, -4, -4)],
        [NSValue valueWithRect:NSInsetRect(_intervalControl.frame, -4, -4)],
        [NSValue valueWithRect:NSInsetRect(_autoRefreshButton.frame, -4, -3)],
        [NSValue valueWithRect:NSInsetRect(_launchAtLoginButton.frame, -4, -3)],
        [NSValue valueWithRect:NSInsetRect(_menuBarOnlyButton.frame, -4, -3)]
    ];
    for (NSValue *value in baseRects) {
        DrawLiquidGlassControlWell(value.rectValue, 11, NSColor.controlAccentColor, self.appearanceMode, NO);
    }

    NSMutableArray<NSValue *> *activeRects = [NSMutableArray new];
    if (self.autoRefreshEnabled) {
        [activeRects addObject:[NSValue valueWithRect:NSInsetRect(_autoRefreshButton.frame, -4, -3)]];
    }
    if (self.launchAtLoginEnabled) {
        [activeRects addObject:[NSValue valueWithRect:NSInsetRect(_launchAtLoginButton.frame, -4, -3)]];
    }
    if (self.menuBarOnly) {
        [activeRects addObject:[NSValue valueWithRect:NSInsetRect(_menuBarOnlyButton.frame, -4, -3)]];
    }
    for (NSValue *value in activeRects) {
        NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:value.rectValue xRadius:11 yRadius:11];
        [[[NSColor controlAccentColor] colorWithAlphaComponent:dark ? 0.120 : 0.085] setFill];
        [path fill];
        DrawLiquidGlassRim(value.rectValue, 11, NSColor.controlAccentColor, dark ? 0.58 : 0.50);
    }
}

- (NSShadow *)shadowWithAlpha:(CGFloat)alpha blur:(CGFloat)blur y:(CGFloat)y {
    NSShadow *shadow = [NSShadow new];
    shadow.shadowColor = [[NSColor shadowColor] colorWithAlphaComponent:alpha];
    shadow.shadowBlurRadius = blur;
    shadow.shadowOffset = NSMakeSize(0, y);
    return shadow;
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

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate, NSMenuItemValidation, NSPopoverDelegate, NSMenuDelegate>
- (BOOL)prepareForSingleInstanceLaunch;
- (void)handleShowMainWindowNotification:(NSNotification *)notification;
- (BOOL)bundlePathIsMountedDiskImage:(NSString *)path;
- (BOOL)bundlePathIsInstalledApplication:(NSString *)path;
- (void)showCardWindowTemporarily;
- (void)saveWindowFrameIfAvailable;
- (void)copyHistoryCSV;
- (void)copyTrendCSV;
- (void)copyHistoryPoints:(NSArray<NSDictionary *> *)points status:(NSString *)status;
- (void)confirmAndClearCurrentHistory;
- (void)populateRecentTargetsMenu:(NSMenu *)menu;
@end

@implementation AppDelegate {
    NSWindow *_window;
    NSView *_rootView;
    AppDashboardChromeView *_chromeView;
    NSVisualEffectView *_glassEffectView;
    LiquidCardView *_card;
    NSStatusItem *_statusItem;
    NSPopover *_statusPopover;
    NSVisualEffectView *_statusPopoverEffectView;
    StatusPopoverView *_popoverView;
    NSDateFormatter *_formatter;
    NSTimer *_timer;
    NSTimer *_displayTimer;
    NSTimer *_windowFrameSaveTimer;
    NSTimer *_transientStatusTimer;
    NSDate *_nextAutoRefreshAt;
    NSImage *_statusIconActive;
    NSImage *_statusIconInactive;
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
    NSUInteger _historyPointCount;
    NSUInteger _weekPointCount;
    NSUInteger _trendPointCount;
    NSInteger _historyDelta;
    BOOL _historyHasDelta;
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
    [NSWorkspace.sharedWorkspace.notificationCenter addObserver:self
                                                       selector:@selector(accessibilityDisplayOptionsDidChange:)
                                                           name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                                         object:nil];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [self setupMainMenu];
    NSRect visible = NSScreen.mainScreen.visibleFrame;
    NSRect frame = NSMakeRect(NSMidX(visible) - MainWindowWidth / 2.0, NSMidY(visible) - MainWindowHeight / 2.0, MainWindowWidth, MainWindowHeight);
    frame.origin = [Settings windowOriginWithDefaultFrame:frame];

    _rootView = [[NSVisualEffectView alloc] initWithFrame:NSMakeRect(0, 0, MainWindowWidth, MainWindowHeight)];
    _rootView.wantsLayer = YES;
    ((NSVisualEffectView *)_rootView).material = NSVisualEffectMaterialWindowBackground;
    ((NSVisualEffectView *)_rootView).blendingMode = NSVisualEffectBlendingModeBehindWindow;
    ((NSVisualEffectView *)_rootView).state = NSVisualEffectStateActive;

    _chromeView = [[AppDashboardChromeView alloc] initWithFrame:_rootView.bounds];
    _chromeView.appearanceMode = [Settings appearanceMode];
    _chromeView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [_rootView addSubview:_chromeView];

    NSRect cardFrame = NSMakeRect(40, 334, 424, 180);
    _glassEffectView = [[NSVisualEffectView alloc] initWithFrame:cardFrame];
    _glassEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    _glassEffectView.state = NSVisualEffectStateActive;
    _glassEffectView.wantsLayer = YES;
    _glassEffectView.layer.cornerRadius = 24;
    if (@available(macOS 10.15, *)) {
        _glassEffectView.layer.cornerCurve = kCACornerCurveContinuous;
    }
    _glassEffectView.layer.masksToBounds = YES;
    [_rootView addSubview:_glassEffectView];

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
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
    _window.title = @"BILI粉丝数";
    _window.titleVisibility = NSWindowTitleHidden;
    _window.opaque = NO;
    _window.backgroundColor = NSColor.windowBackgroundColor;
    _window.titlebarAppearsTransparent = NO;
    _window.hasShadow = YES;
    _window.releasedWhenClosed = NO;
    _window.delegate = self;
    _window.contentView = _rootView;
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

    _formatter = [NSDateFormatter new];
    _formatter.dateFormat = @"HH:mm";
    [self showCachedResultIfAvailable];
    [self startAutoRefresh];
    if ([Settings autoRefreshEnabled]) {
        [self refresh:NO];
    } else if (!_card.hasFollowers) {
        [self setStableCardStatus:@"自动刷新已暂停"];
        [self syncStatusPopover];
    }
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
        NSImage *image = [NSImage imageWithSystemSymbolName:symbolName accessibilityDescription:title];
        if (image) {
            image.template = YES;
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
        item.state = result.mid == currentMID ? NSControlStateValueOn : NSControlStateValueOff;
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
    [self applyVisualEffectMaterials];
    _chromeView.needsDisplay = YES;
    _preferencesChromeView.needsDisplay = YES;
    _card.needsDisplay = YES;
    _popoverView.needsDisplay = YES;
}

- (void)windowDidMove:(NSNotification *)notification {
    [self scheduleWindowFrameSave];
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

- (NSVisualEffectMaterial)popoverMaterialForAppearance:(NSInteger)mode {
    mode = EffectiveAppearanceMode(mode);
    if (mode == AppearanceModeReadable) {
        return NSVisualEffectMaterialWindowBackground;
    }
    if (mode == AppearanceModeClear) {
        return NSVisualEffectMaterialPopover;
    }
    return NSVisualEffectMaterialMenu;
}

- (void)applyVisualEffectMaterials {
    NSInteger mode = [Settings appearanceMode];
    NSInteger effectiveMode = EffectiveAppearanceMode(mode);
    if (_glassEffectView) {
        _glassEffectView.material = [self cardMaterialForAppearance:mode];
        _glassEffectView.state = NSVisualEffectStateActive;
        _glassEffectView.alphaValue = effectiveMode == AppearanceModeUltraClear ? 0.58 : (effectiveMode == AppearanceModeClear ? 0.74 : 0.94);
    }
    if (_statusPopoverEffectView) {
        _statusPopoverEffectView.material = [self popoverMaterialForAppearance:mode];
        _statusPopoverEffectView.state = NSVisualEffectStateActive;
        _statusPopoverEffectView.alphaValue = 1.0;
    }
    if (_preferencesEffectView) {
        _preferencesEffectView.material = [self cardMaterialForAppearance:mode];
        _preferencesEffectView.state = NSVisualEffectStateActive;
        _preferencesEffectView.alphaValue = 1.0;
    }
}

- (void)setupStatusItem {
    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    _statusItem.button.image = [self cachedStatusBarWidgetIconActive:NO];
    _statusItem.button.imagePosition = NSImageLeft;
    _statusItem.button.imageScaling = NSImageScaleProportionallyDown;
    _statusItem.button.font = [NSFont monospacedDigitSystemFontOfSize:12 weight:NSFontWeightSemibold];
    _statusItem.button.title = @"";
    _statusItem.button.toolTip = @"BILI粉丝数";
    _statusItem.button.target = self;
    _statusItem.button.action = @selector(statusItemClicked:);
    [_statusItem.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
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
    _popoverView.appearanceHandler = ^(NSInteger mode) { [weakSelf setAppearanceMode:mode]; };
    _popoverView.quitHandler = ^{ [weakSelf statusQuit:nil]; };

    NSViewController *controller = [NSViewController new];
    NSVisualEffectView *root = [[NSVisualEffectView alloc] initWithFrame:_popoverView.bounds];
    root.material = [self popoverMaterialForAppearance:[Settings appearanceMode]];
    root.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    root.state = NSVisualEffectStateActive;
    root.wantsLayer = YES;
    root.layer.cornerRadius = 20;
    if (@available(macOS 10.15, *)) {
        root.layer.cornerCurve = kCACornerCurveContinuous;
    }
    root.layer.masksToBounds = YES;
    root.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _statusPopoverEffectView = root;
    _popoverView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [root addSubview:_popoverView];
    controller.view = root;

    _statusPopover = [NSPopover new];
    _statusPopover.contentSize = _popoverView.bounds.size;
    _statusPopover.behavior = NSPopoverBehaviorSemitransient;
    _statusPopover.animates = YES;
    _statusPopover.delegate = self;
    _statusPopover.contentViewController = controller;
    [self applyVisualEffectMaterials];
    [self syncStatusPopover];
}

- (void)syncStatusPopover {
    if (!_popoverView) return;
    _card.refreshing = _isRefreshing;
    _popoverView.name = _card.name;
    _popoverView.followers = _card.followers;
    _popoverView.hasFollowers = _card.hasFollowers;
    _popoverView.followerDelta = _card.followerDelta;
    _popoverView.hasFollowerDelta = _card.hasFollowerDelta;
    _popoverView.historyValues = _card.historyValues;
    _popoverView.status = _card.status;
    _popoverView.updatedText = _card.updatedText;
    _popoverView.refreshScheduleText = [self refreshScheduleText];
    _popoverView.menuBarOnly = [Settings launchHidden];
    _popoverView.copyEnabled = _card.hasFollowers;
    _popoverView.refreshing = _isRefreshing;
    _popoverView.profileEnabled = _card.mid > 0;
    _popoverView.historyEnabled = _weekPointCount > 0;
    _popoverView.notificationsEnabled = [Settings changeNotificationsEnabled];
    _popoverView.launchAtLoginAvailable = [self launchAtLoginSupported];
    _popoverView.launchAtLoginEnabled = [self launchAtLoginEnabled];
    _popoverView.refreshInterval = [Settings refreshInterval];
    _popoverView.autoRefreshEnabled = [Settings autoRefreshEnabled];
    _popoverView.statusDisplayMode = [Settings statusDisplayMode];
    _popoverView.appearanceMode = [Settings appearanceMode];
    [self syncMainPageControlsIfVisible];
    [self syncPreferencesWindowControlsIfVisible];
}

- (void)syncStatusPopoverIfVisible {
    if (_statusPopover.shown) {
        [self syncStatusPopover];
    }
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
        return;
    }
    [self toggleStatusPopover];
}

- (void)toggleStatusPopover {
    if (_statusPopover.shown) {
        [self closeStatusPopover];
        return;
    }
    [self syncStatusPopover];
    [_statusPopover showRelativeToRect:_statusItem.button.bounds ofView:_statusItem.button preferredEdge:NSRectEdgeMinY];
    [self syncMainStatusWidgetButton];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)closeStatusPopover {
    if (_statusPopover.shown) {
        [_statusPopover close];
    }
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (void)popoverDidClose:(NSNotification *)notification {
    [self syncMainStatusWidgetButton];
    [self syncDisplayRefreshTickerForVisibleSurfaces];
}

- (NSImage *)statusBarWidgetIconActive:(BOOL)active {
    NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(21, 18)];
    image.template = YES;
    [image lockFocus];

    NSRect body = NSMakeRect(3.0, 3.0, 15.0, 12.0);
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:body xRadius:4.2 yRadius:4.2];
    shape.lineWidth = active ? 1.65 : 1.35;
    [[NSColor.blackColor colorWithAlphaComponent:active ? 0.96 : 0.76] setStroke];
    [shape stroke];

    DrawCompactBMark(NSMakeRect(6.2, 5.1, 8.4, 8.4),
                     [NSColor.blackColor colorWithAlphaComponent:active ? 0.96 : 0.76],
                     active ? 1.32 : 1.16);

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
    return DecimalStringForInteger(_card.followers);
}

- (NSString *)compactValue:(double)value suffix:(NSString *)suffix {
    NSString *text = value >= 100 ? [NSString stringWithFormat:@"%.0f", value] : [NSString stringWithFormat:@"%.1f", value];
    if ([text hasSuffix:@".0"]) {
        text = [text substringToIndex:text.length - 2];
    }
    return [text stringByAppendingString:suffix];
}

- (NSString *)compactFollowersText {
    if (_card.followers >= 100000000) {
        return [self compactValue:_card.followers / 100000000.0 suffix:@"亿"];
    }
    if (_card.followers >= 10000) {
        return [self compactValue:_card.followers / 10000.0 suffix:@"万"];
    }
    return [self exactFollowersText];
}

- (NSString *)currentTrendSummaryText {
    if (_historyHasDelta) {
        return [NSString stringWithFormat:@"%@趋势 %@（%lu点）",
                [self trendRangeTitle],
                CompactSignedDelta(_historyDelta),
                (unsigned long)_trendPointCount];
    }
    if (_historyPointCount > 0) {
        return [NSString stringWithFormat:@"%@趋势积累中", [self trendRangeTitle]];
    }
    return @"暂无趋势";
}

- (void)updateStatusItemTitle {
    [self updateStatusItemTitleSyncVisibleSurfaces:YES];
}

- (void)updateStatusItemTitleSyncVisibleSurfaces:(BOOL)syncVisibleSurfaces {
    if (!_statusItem.button) return;
    BOOL iconActive = _isRefreshing || (_card.hasFollowers && [Settings autoRefreshEnabled]);
    if (!_hasLastStatusIconState || _lastStatusIconActive != iconActive) {
        _statusItem.button.image = [self cachedStatusBarWidgetIconActive:iconActive];
        _lastStatusIconActive = iconActive;
        _hasLastStatusIconState = YES;
    }

    NSInteger displayMode = [Settings statusDisplayMode];
    CGFloat length = 28;
    NSString *title = @"";
    NSString *tooltip = nil;
    if (!_card.hasFollowers) {
        NSString *mode = [self refreshScheduleText];
        NSString *name = _card.name.length > 0 ? _card.name : ([Settings name].length > 0 ? [Settings name] : @"BILI粉丝数");
        long long mid = _card.mid > 0 ? _card.mid : [Settings mid];
        NSString *target = mid > 0
            ? [NSString stringWithFormat:@"%@ · UID %lld", name, mid]
            : name;
        tooltip = mode.length > 0
            ? [NSString stringWithFormat:@"BILI粉丝数 · %@ · %@ · 左键小组件 · 右键菜单", target, mode]
            : [NSString stringWithFormat:@"BILI粉丝数 · %@ · 左键小组件 · 右键菜单", target];
        [self applyStatusItemLength:length title:title tooltip:tooltip];
        if (syncVisibleSurfaces) {
            [self syncStatusPopoverIfVisible];
            [self syncMainPageControlsIfVisible];
        }
        return;
    }
    NSString *followers = [self exactFollowersText];
    NSString *name = _card.name.length > 0 ? _card.name : @"BILI粉丝数";
    NSString *updated = _card.updatedText.length > 0 ? _card.updatedText : @"等待刷新";
    if (displayMode == StatusDisplayModeIconOnly) {
        length = 28;
        title = @"";
    } else {
        NSString *titleFollowers = displayMode == StatusDisplayModeCompact ? [self compactFollowersText] : followers;
        length = NSVariableStatusItemLength;
        title = [NSString stringWithFormat:@" %@", titleFollowers];
    }
    NSString *mode = [Settings autoRefreshEnabled] ? [self refreshScheduleText] : @"已暂停";
    tooltip = [NSString stringWithFormat:@"%@ · %@ · %@ · %@ · %@ · 左键小组件 · 右键菜单",
               name,
               followers,
               updated,
               [self currentTrendSummaryText],
               mode.length > 0 ? mode : @"自动刷新"];
    [self applyStatusItemLength:length title:title tooltip:tooltip];
    if (syncVisibleSurfaces) {
        [self syncStatusPopoverIfVisible];
        [self syncMainPageControlsIfVisible];
    }
}

- (void)updateTimeSensitiveStatusText {
    [self updateStatusItemTitleSyncVisibleSurfaces:NO];

    NSString *scheduleText = [self refreshScheduleText];
    if (_statusPopover.shown) {
        _popoverView.refreshScheduleText = scheduleText;
    }
    if (_window.isVisible && _mainScheduleLabel) {
        _mainScheduleLabel.stringValue = [NSString stringWithFormat:@"自动刷新：%@", scheduleText.length > 0 ? scheduleText : @"等待计时"];
        _mainScheduleLabel.textColor = [Settings autoRefreshEnabled] ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;
    }
}

- (void)applyStatusItemLength:(CGFloat)length title:(NSString *)title tooltip:(NSString *)tooltip {
    if (!_hasLastStatusLength || _lastStatusLength != length) {
        _statusItem.length = length;
        _lastStatusLength = length;
        _hasLastStatusLength = YES;
    }
    if (![_lastStatusTitle isEqualToString:title]) {
        _statusItem.button.title = title;
        _lastStatusTitle = [title copy];
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
        item.title = _window.isVisible ? @"隐藏主窗口" : @"显示主窗口";
        return YES;
    }
    if (action == @selector(showCardWindow)) {
        return !_window.isVisible;
    }
    if (action == @selector(hideCardWindow)) {
        return _window.isVisible;
    }
    if (action == @selector(statusRefresh:)) {
        item.title = _isRefreshing ? @"正在刷新" : @"刷新";
        return !_isRefreshing;
    }
    if (action == @selector(toggleMenuBarOnly:)) {
        item.state = [Settings launchHidden] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusToggleAlwaysOnTop:)) {
        item.state = [Settings alwaysOnTop] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusTogglePositionLocked:)) {
        item.state = [Settings positionLocked] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusToggleAutoRefresh:)) {
        NSString *schedule = [self refreshScheduleText];
        item.title = [Settings autoRefreshEnabled] && schedule.length > 0
            ? [NSString stringWithFormat:@"自动刷新（%@）", schedule]
            : @"自动刷新";
        item.state = [Settings autoRefreshEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusToggleChangeNotifications:)) {
        item.state = [Settings changeNotificationsEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusToggleLaunchAtLogin:)) {
        NSString *unavailableReason = [self launchAtLoginUnavailableReason];
        if (unavailableReason) {
            item.title = @"登录时启动（需先安装）";
            item.state = NSControlStateValueOff;
            return NO;
        }
        item.title = @"登录时启动";
        item.state = [self launchAtLoginEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusUseExactDisplay:) ||
        action == @selector(statusUseCompactDisplay:) ||
        action == @selector(statusUseIconOnlyDisplay:)) {
        NSInteger mode = [Settings statusDisplayMode];
        BOOL checked = (action == @selector(statusUseExactDisplay:) && mode == StatusDisplayModeExact)
                || (action == @selector(statusUseCompactDisplay:) && mode == StatusDisplayModeCompact)
                || (action == @selector(statusUseIconOnlyDisplay:) && mode == StatusDisplayModeIconOnly);
        item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusCopyFollowers:)) {
        return _card.hasFollowers;
    }
    if (action == @selector(statusCopyHistory:)) {
        item.title = @"复制一周 CSV";
        return _weekPointCount > 0;
    }
    if (action == @selector(statusCopyTrend:)) {
        item.title = [NSString stringWithFormat:@"复制%@趋势 CSV", [self trendRangeTitle]];
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
        item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusUseOneMinute:) ||
        action == @selector(statusUseFiveMinutes:) ||
        action == @selector(statusUseFifteenMinutes:)) {
        NSTimeInterval interval = [Settings refreshInterval];
        BOOL checked = (action == @selector(statusUseOneMinute:) && interval <= 90)
                || (action == @selector(statusUseFiveMinutes:) && interval > 90 && interval <= 450)
                || (action == @selector(statusUseFifteenMinutes:) && interval > 450);
        item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
        return YES;
    }
    if (action == @selector(statusUseSevenDayTrend:) ||
        action == @selector(statusUseThirtyDayTrend:) ||
        action == @selector(statusUseNinetyDayTrend:)) {
        NSInteger range = [Settings trendRange];
        BOOL checked = (action == @selector(statusUseSevenDayTrend:) && range == TrendRangeSevenDays)
                || (action == @selector(statusUseThirtyDayTrend:) && range == TrendRangeThirtyDays)
                || (action == @selector(statusUseNinetyDayTrend:) && range == TrendRangeNinetyDays);
        item.state = checked ? NSControlStateValueOn : NSControlStateValueOff;
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
        [self syncStatusPopover];
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
    [self syncStatusPopover];
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
    if (@available(macOS 13.0, *)) {
        if ([self isRunningFromMountedDiskImage]) {
            return @"请先拖到“应用程序”后再开启登录启动";
        }
        return nil;
    }
    return @"登录启动需要 macOS 13+";
}

- (BOOL)launchAtLoginSupported {
    return [self launchAtLoginUnavailableReason] == nil;
}

- (BOOL)launchAtLoginEnabled {
    if (![self launchAtLoginSupported]) {
        return NO;
    }
    if (@available(macOS 13.0, *)) {
        return SMAppService.mainAppService.status == SMAppServiceStatusEnabled;
    }
    return NO;
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
    [self syncStatusPopover];
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
    [self syncStatusPopover];

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
        [strongSelf syncStatusPopover];
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

- (NSString *)trendRangeTitle {
    NSInteger range = [Settings trendRange];
    if (range == TrendRangeNinetyDays) {
        return @"90天";
    }
    if (range == TrendRangeSevenDays) {
        return @"7天";
    }
    return @"30天";
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
    NSArray<NSDictionary *> *history = [Settings historyForCurrentTarget];
    [Settings backfillSegmentedHistoryCSVIfNeededForMID:[Settings mid] history:history];
    NSInteger trendRange = [Settings trendRange];
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - [self trendIntervalForRange:trendRange];
    NSTimeInterval weekCutoff = NSDate.date.timeIntervalSince1970 - OneWeekInterval;
    NSArray<NSDictionary *> *trendHistory = TrendHistoryIncludingBaseline(history, cutoff);
    _historyPointCount = history.count;
    _weekPointCount = HistoryPointCountAtOrAfterCutoff(history, weekCutoff);
    _trendPointCount = HistoryPointCountAtOrAfterCutoff(history, cutoff);
    _historyHasDelta = trendHistory.count >= 2;
    _historyDelta = _historyHasDelta
        ? [trendHistory.lastObject[@"followers"] integerValue] - [trendHistory.firstObject[@"followers"] integerValue]
        : 0;
    _card.historyValues = [self sparklineValuesForTrendHistory:trendHistory];
}

- (void)copyHistoryCSV {
    NSArray<NSDictionary *> *history = [Settings historyForCurrentTarget];
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - OneWeekInterval;
    NSArray<NSDictionary *> *weekHistory = HistoryPointsAtOrAfterCutoff(history, cutoff);
    if (weekHistory.count == 0) {
        [self showTransientCardStatus:@"近7天暂无历史"];
        return;
    }

    [self copyHistoryPoints:weekHistory status:@"一周 CSV 已复制"];
}

- (void)copyTrendCSV {
    NSArray<NSDictionary *> *history = [Settings historyForCurrentTarget];
    NSTimeInterval cutoff = NSDate.date.timeIntervalSince1970 - [self trendIntervalForRange:[Settings trendRange]];
    NSArray<NSDictionary *> *trendHistory = TrendHistoryIncludingBaseline(history, cutoff);
    if (trendHistory.count == 0) {
        [self showTransientCardStatus:@"暂无趋势"];
        return;
    }

    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard setString:CSVStringForHistoryPoints(trendHistory, YES, cutoff) forType:NSPasteboardTypeString];
    [self showTransientCardStatus:[NSString stringWithFormat:@"%@趋势已复制", [self trendRangeTitle]]];
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
    _mainStatusLabel.stringValue = text ?: @"";
    _mainStatusLabel.textColor = error ? NSColor.systemRedColor : NSColor.secondaryLabelColor;
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
    button.enabled = enabled;
    button.alphaValue = enabled ? 1.0 : 0.42;
}

- (void)syncMainStatusWidgetButton {
    if (!_mainStatusWidgetButton) return;
    BOOL shown = _statusPopover.shown;
    NSString *title = shown ? @"关闭顶栏卡片" : @"打开顶栏卡片";
    NSString *symbol = shown ? @"xmark.circle" : @"menubar.rectangle";
    _mainStatusWidgetButton.title = title;
    _mainStatusWidgetButton.toolTip = title;
    NSImage *image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:title];
    if (image) {
        image.template = YES;
        _mainStatusWidgetButton.image = image;
        _mainStatusWidgetButton.imagePosition = NSImageLeft;
    }
}

- (void)syncMainPageControls {
    if (!_mainUIDField) return;
    if (![self textFieldIsBeingEdited:_mainUIDField]) {
        _mainUIDField.stringValue = [NSString stringWithFormat:@"%lld", [Settings mid]];
    }

    NSString *name = _card.name.length > 0 ? _card.name : [Settings name];
    if (name.length == 0) {
        name = UnknownTargetName;
    }
    _mainTargetLabel.stringValue = [NSString stringWithFormat:@"当前监控：%@", name];
    if (_card.hasFollowers) {
        NSString *followers = DecimalStringForInteger(_card.followers);
        NSString *updated = _card.updatedText.length > 0 ? _card.updatedText : @"--";
        NSString *status = _card.status.length > 0 ? _card.status : @"实时统计";
        _mainStatusLabel.stringValue = [NSString stringWithFormat:@"%@ · %@ · %@", followers, status, updated];
        _mainStatusLabel.textColor = NSColor.secondaryLabelColor;
    } else {
        _mainStatusLabel.stringValue = _card.status.length > 0 ? _card.status : @"等待刷新";
        _mainStatusLabel.textColor = NSColor.secondaryLabelColor;
    }
    NSString *scheduleText = [self refreshScheduleText];
    _mainScheduleLabel.stringValue = [NSString stringWithFormat:@"自动刷新：%@", scheduleText.length > 0 ? scheduleText : @"等待计时"];
    _mainScheduleLabel.textColor = [Settings autoRefreshEnabled] ? NSColor.secondaryLabelColor : NSColor.tertiaryLabelColor;

    NSInteger appearance = [Settings appearanceMode];
    _mainAppearanceControl.selectedSegment = appearance == AppearanceModeReadable
        ? 2
        : (appearance == AppearanceModeClear ? 1 : 0);

    NSInteger trendRange = [Settings trendRange];
    _mainTrendControl.selectedSegment = trendRange == TrendRangeNinetyDays
        ? 2
        : (trendRange == TrendRangeThirtyDays ? 1 : 0);

    NSInteger display = [Settings statusDisplayMode];
    _mainDisplayControl.selectedSegment = display == StatusDisplayModeIconOnly
        ? 2
        : (display == StatusDisplayModeCompact ? 1 : 0);

    NSTimeInterval interval = [Settings refreshInterval];
    _mainIntervalControl.selectedSegment = interval > 450 ? 2 : (interval > 90 ? 1 : 0);
    _mainIntervalControl.enabled = YES;
    _mainIntervalControl.alphaValue = 1.0;
    _mainIntervalControl.toolTip = [Settings autoRefreshEnabled]
        ? @"选择自动刷新间隔"
        : @"自动刷新暂停中，重新开启后按此间隔刷新";

    _mainAutoRefreshButton.state = [Settings autoRefreshEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    _mainNotificationsButton.state = [Settings changeNotificationsEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    BOOL launchAtLoginSupported = [self launchAtLoginSupported];
    NSString *launchAtLoginReason = [self launchAtLoginUnavailableReason];
    _mainLaunchAtLoginButton.enabled = launchAtLoginSupported;
    _mainLaunchAtLoginButton.alphaValue = launchAtLoginSupported ? 1.0 : 0.45;
    _mainLaunchAtLoginButton.state = [self launchAtLoginEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    _mainLaunchAtLoginButton.toolTip = launchAtLoginReason ?: @"登录后自动启动";
    _mainMenuBarOnlyButton.state = [Settings launchHidden] ? NSControlStateValueOn : NSControlStateValueOff;
    [self syncMainStatusWidgetButton];
    _mainAlwaysOnTopButton.state = [Settings alwaysOnTop] ? NSControlStateValueOn : NSControlStateValueOff;
    _mainPositionLockedButton.state = [Settings positionLocked] ? NSControlStateValueOn : NSControlStateValueOff;

    BOOL hasHistory = _historyPointCount > 0;
    BOOL hasWeekHistory = _weekPointCount > 0;
    if (_mainHistoryLabel) {
        if (_historyHasDelta) {
            NSString *deltaText = CompactSignedDelta(_historyDelta);
            _mainHistoryLabel.stringValue = [NSString stringWithFormat:@"%@趋势 · %lu点 · %@", [self trendRangeTitle], (unsigned long)_trendPointCount, deltaText];
            _mainHistoryLabel.textColor = FollowerDeltaColor(_historyDelta);
        } else if (_historyPointCount > 0) {
            _mainHistoryLabel.stringValue = [NSString stringWithFormat:@"%@趋势 · 数据继续积累中", [self trendRangeTitle]];
            _mainHistoryLabel.textColor = NSColor.secondaryLabelColor;
        } else {
            _mainHistoryLabel.stringValue = @"暂无历史点，刷新成功后自动记录。";
            _mainHistoryLabel.textColor = NSColor.tertiaryLabelColor;
        }
    }
    [self setButton:_mainRefreshButton enabled:!_isRefreshing];
    _mainRefreshButton.title = _isRefreshing ? @"刷新中" : @"刷新";
    _mainRefreshButton.toolTip = _isRefreshing ? @"正在刷新当前账号" : @"立即刷新当前账号";
    [self setButton:_mainCopyButton enabled:_card.hasFollowers];
    _mainCopyButton.toolTip = _card.hasFollowers ? @"复制当前粉丝数" : @"刷新成功后可复制粉丝数";
    [self setButton:_mainProfileButton enabled:_card.mid > 0];
    _mainProfileButton.toolTip = _card.mid > 0 ? @"打开当前 B站主页" : @"UID 有效后可打开主页";
    [self setButton:_mainCopyHistoryButton enabled:hasWeekHistory];
    _mainCopyHistoryButton.toolTip = hasWeekHistory ? @"复制当前账号近 7 天 CSV" : @"近 7 天暂无可复制历史";
    BOOL hasTrend = _trendPointCount > 0;
    [self setButton:_mainCopyTrendButton enabled:hasTrend];
    _mainCopyTrendButton.toolTip = hasTrend
        ? [NSString stringWithFormat:@"复制%@趋势 CSV（含基线点）", [self trendRangeTitle]]
        : [NSString stringWithFormat:@"%@趋势数据继续积累中", [self trendRangeTitle]];
    [self setButton:_mainClearHistoryButton enabled:hasHistory];
    _mainClearHistoryButton.toolTip = hasHistory ? @"清空当前账号历史" : @"暂无可清空的历史";
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
    BOOL wasShown = _statusPopover.shown;
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
    NSSegmentedControl *control = [[NSSegmentedControl alloc] initWithFrame:frame];
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
                                                    styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                      backing:NSBackingStoreBuffered
                                                        defer:NO];
    _preferencesWindow.title = @"BILI粉丝数设置";
    _preferencesWindow.releasedWhenClosed = NO;
    _preferencesWindow.level = NSNormalWindowLevel;
    _preferencesWindow.opaque = NO;
    _preferencesWindow.backgroundColor = NSColor.windowBackgroundColor;
    _preferencesWindow.titlebarAppearsTransparent = NO;

    NSVisualEffectView *root = [[NSVisualEffectView alloc] initWithFrame:frame];
    root.material = NSVisualEffectMaterialWindowBackground;
    root.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    root.state = NSVisualEffectStateActive;
    root.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _preferencesWindow.contentView = root;
    _preferencesEffectView = root;

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
    _preferencesStatusLabel.stringValue = text ?: @"";
    _preferencesStatusLabel.textColor = error ? NSColor.systemRedColor : NSColor.secondaryLabelColor;
}

- (void)syncPreferencesWindowControls {
    if (!_preferencesWindow) return;
    if (![self textFieldIsBeingEdited:_preferencesUIDField]) {
        _preferencesUIDField.stringValue = [NSString stringWithFormat:@"%lld", [Settings mid]];
    }

    NSInteger appearance = [Settings appearanceMode];
    _preferencesAppearanceControl.selectedSegment = appearance == AppearanceModeReadable
        ? 2
        : (appearance == AppearanceModeClear ? 1 : 0);

    NSInteger display = [Settings statusDisplayMode];
    _preferencesDisplayControl.selectedSegment = display == StatusDisplayModeIconOnly
        ? 2
        : (display == StatusDisplayModeCompact ? 1 : 0);

    NSTimeInterval interval = [Settings refreshInterval];
    _preferencesIntervalControl.selectedSegment = interval > 450 ? 2 : (interval > 90 ? 1 : 0);
    _preferencesIntervalControl.enabled = YES;
    _preferencesIntervalControl.alphaValue = 1.0;
    _preferencesIntervalControl.toolTip = [Settings autoRefreshEnabled]
        ? @"选择自动刷新间隔"
        : @"自动刷新暂停中，重新开启后按此间隔刷新";

    _preferencesAutoRefreshButton.state = [Settings autoRefreshEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    _preferencesNotificationsButton.state = [Settings changeNotificationsEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    BOOL launchAtLoginSupported = [self launchAtLoginSupported];
    NSString *launchAtLoginReason = [self launchAtLoginUnavailableReason];
    _preferencesLaunchAtLoginButton.enabled = launchAtLoginSupported;
    _preferencesLaunchAtLoginButton.alphaValue = launchAtLoginSupported ? 1.0 : 0.45;
    _preferencesLaunchAtLoginButton.state = [self launchAtLoginEnabled] ? NSControlStateValueOn : NSControlStateValueOff;
    _preferencesLaunchAtLoginButton.toolTip = launchAtLoginReason ?: @"登录后自动启动";
    _preferencesMenuBarOnlyButton.state = [Settings launchHidden] ? NSControlStateValueOn : NSControlStateValueOff;
    _preferencesAlwaysOnTopButton.state = [Settings alwaysOnTop] ? NSControlStateValueOn : NSControlStateValueOff;
    _preferencesPositionLockedButton.state = [Settings positionLocked] ? NSControlStateValueOn : NSControlStateValueOff;
    [self setButton:_preferencesRefreshButton enabled:!_isRefreshing];
    _preferencesRefreshButton.title = _isRefreshing ? @"刷新中" : @"立即刷新";

    NSString *name = [Settings name].length > 0 ? [Settings name] : UnknownTargetName;
    if (launchAtLoginReason) {
        [self setPreferencesStatus:launchAtLoginReason error:NO];
    } else {
        [self setPreferencesStatus:[NSString stringWithFormat:@"当前 %@ · UID %lld", name, [Settings mid]] error:NO];
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
        [self syncStatusPopover];
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
    if ((!_statusPopover.shown && !_window.isVisible) || ![Settings autoRefreshEnabled]) {
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
        if (![Settings autoRefreshEnabled] || (!strongSelf->_statusPopover.shown && !strongSelf->_window.isVisible)) {
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
    if ([Settings autoRefreshEnabled] && (_statusPopover.shown || _window.isVisible)) {
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
    if (_isRefreshing) {
        return @"正在刷新";
    }
    if (![Settings autoRefreshEnabled]) {
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

    NSDateFormatter *dateFormatter = [NSDateFormatter new];
    dateFormatter.locale = [NSLocale localeWithLocaleIdentifier:@"zh_CN"];
    NSDateComponents *now = [calendar components:NSCalendarUnitYear fromDate:[NSDate date]];
    NSDateComponents *then = [calendar components:NSCalendarUnitYear fromDate:date];
    dateFormatter.dateFormat = now.year == then.year ? @"M月d日 HH:mm" : @"yyyy年M月d日 HH:mm";
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
    [self syncStatusPopover];
}

- (void)refresh:(BOOL)manual {
    long long mid = [Settings mid];
    if (_isRefreshing) {
        if (_activeRefreshMID != mid) {
            _pendingRefreshAfterCurrent = YES;
            _pendingRefreshManual = _pendingRefreshManual || manual;
            _card.status = @"切换后刷新";
        } else if (manual) {
            _card.status = @"正在刷新";
        }
        _card.needsDisplay = YES;
        [self syncStatusPopover];
        return;
    }
    _isRefreshing = YES;
    [_transientStatusTimer invalidate];
    _transientStatusTimer = nil;
    _activeRefreshMID = mid;
    NSString *fallbackName = [Settings name];
    _card.status = manual ? @"正在刷新" : @"自动刷新";
    [self updateStatusItemTitle];
    [self syncStatusPopover];
    [self syncMainPageControlsIfVisible];
    [BiliFansClient fetchMID:mid fallbackName:fallbackName completion:^(FanResult *result, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            _isRefreshing = NO;
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
                [self updateStatusItemTitle];
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
                }
                [self updateStatusItemTitle];
                [self syncStatusPopover];
            }
        });
    }];
}
@end

int main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        AppDelegate *delegate = [AppDelegate new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
