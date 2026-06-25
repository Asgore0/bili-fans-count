// Settings, defaults, latest-result cache, history cache, and CSV persistence.
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
static NSMutableSet<NSNumber *> *sBackfilledHistoryCSVMIDs = nil;

+ (void)invalidateHistoryCache {
    sCachedHistoryMID = 0;
    sCachedHistoryForMID = nil;
}

+ (void)setHistoryCacheForMID:(long long)mid points:(NSArray<NSDictionary *> *)points {
    if (mid <= 0) {
        [self invalidateHistoryCache];
        return;
    }
    sCachedHistoryMID = mid;
    sCachedHistoryForMID = [points copy] ?: @[];
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

    NSNumber *midNumber = @(mid);
    if (!sBackfilledHistoryCSVMIDs) {
        sBackfilledHistoryCSVMIDs = [NSMutableSet new];
    }
    if ([sBackfilledHistoryCSVMIDs containsObject:midNumber]) {
        return;
    }

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
    if (hasCSV) {
        [sBackfilledHistoryCSVMIDs addObject:midNumber];
        return;
    }

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

    if (payloads.count == 0) {
        [sBackfilledHistoryCSVMIDs addObject:midNumber];
        return;
    }
    NSURL *targetDirectoryURL = HistoryCSVDirectoryURLForMID(mid, YES);
    if (!targetDirectoryURL) return;
    BOOL wroteAllPayloads = YES;
    for (NSString *period in payloads) {
        NSURL *fileURL = [targetDirectoryURL URLByAppendingPathComponent:[period stringByAppendingString:@".csv"]
                                                              isDirectory:NO];
        NSError *writeError = nil;
        if (![payloads[period] writeToURL:fileURL
                               atomically:YES
                                 encoding:NSUTF8StringEncoding
                                    error:&writeError]) {
            NSLog(@"BILI粉丝数：回填历史 CSV 失败：%@", writeError.localizedDescription);
            wroteAllPayloads = NO;
        }
    }
    if (wroteAllPayloads) {
        [sBackfilledHistoryCSVMIDs addObject:midNumber];
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
    long long currentMID = [self mid];
    NSMutableArray<NSDictionary *> *currentHistory = [NSMutableArray new];
    for (NSDictionary *point in trimmed) {
        if ([point[@"mid"] longLongValue] == currentMID) {
            [currentHistory addObject:point];
        }
    }
    [self setHistoryCacheForMID:currentMID points:currentHistory];
    FlushUserSettings();
}
+ (void)clearSegmentedHistoryCSVForMID:(long long)mid {
    [sBackfilledHistoryCSVMIDs removeObject:@(mid)];
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
