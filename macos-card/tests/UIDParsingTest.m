#define main BILIFansCardAppMain
#import "../src/BILIFansCard.m"
#undef main

static void AssertMID(NSString *input, long long expected) {
    long long actual = BilibiliMIDFromInput(input);
    if (actual != expected) {
        fprintf(stderr, "Expected %lld for %s, got %lld\n", expected, input.UTF8String, actual);
        abort();
    }
}

static NSDictionary *HistoryPoint(NSTimeInterval updatedAt, NSInteger followers) {
    return @{@"updated_at": @(updatedAt), @"followers": @(followers), @"mid": @3546718146661176LL};
}

static void AssertTrendFollowers(NSArray<NSDictionary *> *history, NSTimeInterval cutoff, const NSInteger *expected, NSUInteger expectedCount) {
    NSArray<NSDictionary *> *actual = TrendHistoryIncludingBaseline(history, cutoff);
    if (actual.count != expectedCount) {
        fprintf(stderr, "Expected %lu trend points, got %lu\n", (unsigned long)expectedCount, (unsigned long)actual.count);
        abort();
    }
    for (NSUInteger i = 0; i < expectedCount; i++) {
        NSInteger followers = [actual[i][@"followers"] integerValue];
        if (followers != expected[i]) {
            fprintf(stderr, "Expected follower value %ld at %lu, got %ld\n", (long)expected[i], (unsigned long)i, (long)followers);
            abort();
        }
    }
}

static void AssertTrendCount(NSArray<NSDictionary *> *history, NSTimeInterval cutoff, NSUInteger expected) {
    NSUInteger actual = HistoryPointCountAtOrAfterCutoff(history, cutoff);
    if (actual != expected) {
        fprintf(stderr, "Expected %lu in-range points, got %lu\n", (unsigned long)expected, (unsigned long)actual);
        abort();
    }
}

static void AssertFilteredHistoryCount(NSArray<NSDictionary *> *history, NSTimeInterval cutoff, NSUInteger expected) {
    NSArray<NSDictionary *> *actual = HistoryPointsAtOrAfterCutoff(history, cutoff);
    if (actual.count != expected) {
        fprintf(stderr, "Expected %lu filtered history points, got %lu\n", (unsigned long)expected, (unsigned long)actual.count);
        abort();
    }
}

static void AssertBiliError(NSDictionary *root, NSString *fallback, NSInteger fallbackCode, NSInteger expectedCode, NSString *expectedDescription) {
    NSError *error = BiliAPIError(root, fallback, fallbackCode);
    if (error.code != expectedCode || ![error.localizedDescription isEqualToString:expectedDescription]) {
        fprintf(stderr,
                "Expected error %ld/%s, got %ld/%s\n",
                (long)expectedCode,
                expectedDescription.UTF8String,
                (long)error.code,
                error.localizedDescription.UTF8String);
        abort();
    }
}

static void AssertContains(NSString *value, NSString *needle) {
    if ([value rangeOfString:needle].location == NSNotFound) {
        fprintf(stderr, "Expected CSV to contain %s, got %s\n", needle.UTF8String, value.UTF8String);
        abort();
    }
}

static NSDictionary<NSString *, id> *SnapshotDefaults(NSArray<NSString *> *keys) {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    NSMutableDictionary<NSString *, id> *snapshot = [NSMutableDictionary new];
    for (NSString *key in keys) {
        id value = [defaults objectForKey:key];
        if (value) snapshot[key] = value;
    }
    return snapshot.copy;
}

static void RestoreDefaults(NSArray<NSString *> *keys, NSDictionary<NSString *, id> *snapshot) {
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    for (NSString *key in keys) {
        [defaults removeObjectForKey:key];
    }
    for (NSString *key in snapshot) {
        [defaults setObject:snapshot[key] forKey:key];
    }
    [defaults synchronize];
}

static BOOL ResultsContainMID(NSArray<FanResult *> *results, long long mid) {
    for (FanResult *result in results) {
        if (result.mid == mid) return YES;
    }
    return NO;
}

static BOOL HistoryContainsFollowers(NSArray<NSDictionary *> *history, NSInteger followers) {
    for (NSDictionary *point in history) {
        if ([point[@"followers"] integerValue] == followers) return YES;
    }
    return NO;
}

static void AssertHistoryDoesNotExpireAfterOneYear(void) {
    NSTimeInterval day = 24 * 60 * 60;
    NSTimeInterval referenceTime = 2000 * day;
    NSArray<NSDictionary *> *history = @[
        HistoryPoint(referenceTime - 800 * day, 800),
        HistoryPoint(referenceTime - 20 * day, 20)
    ];
    NSArray<NSDictionary *> *normalized = [Settings normalizedHistoryGroup:history
                                                             referenceDate:[NSDate dateWithTimeIntervalSince1970:referenceTime]];
    if (!HistoryContainsFollowers(normalized, 800) || !HistoryContainsFollowers(normalized, 20)) {
        fprintf(stderr, "Expected history points older than one year to be retained\n");
        abort();
    }
}

static void AssertRecentResultsFallback(void) {
    NSArray<NSString *> *keys = @[@"target_mid",
                                  @"target_name",
                                  @"latest_mid",
                                  @"latest_name",
                                  @"latest_followers",
                                  @"latest_updated_at",
                                  @"latest_results_by_mid"];
    NSDictionary<NSString *, id> *snapshot = SnapshotDefaults(keys);
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    for (NSString *key in keys) {
        [defaults removeObjectForKey:key];
    }

    [defaults setObject:@(111LL) forKey:@"target_mid"];
    [defaults setObject:@"旧缓存" forKey:@"target_name"];
    [defaults setObject:@(111LL) forKey:@"latest_mid"];
    [defaults setObject:@"旧缓存" forKey:@"latest_name"];
    [defaults setObject:@(1234) forKey:@"latest_followers"];
    [defaults setDouble:2000 forKey:@"latest_updated_at"];
    [defaults setObject:@{@"bad": @"not-a-result"} forKey:@"latest_results_by_mid"];
    NSArray<FanResult *> *corruptResults = [Settings recentResults];
    if (corruptResults.count != 1 || !ResultsContainMID(corruptResults, 111LL)) {
        fprintf(stderr, "Expected legacy current result when per-UID cache is corrupt\n");
        abort();
    }

    NSDictionary *otherResult = @{@"mid": @(222LL),
                                  @"name": @"另一个账号",
                                  @"followers": @(5678),
                                  @"updated_at": @(2200)};
    [defaults setObject:@{@"222": otherResult} forKey:@"latest_results_by_mid"];
    NSArray<FanResult *> *mixedResults = [Settings recentResults];
    if (mixedResults.count != 2 || !ResultsContainMID(mixedResults, 111LL) || !ResultsContainMID(mixedResults, 222LL)) {
        fprintf(stderr, "Expected legacy current result to be merged with valid recent results\n");
        abort();
    }

    RestoreDefaults(keys, snapshot);
}

int main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        AssertMID(@"3546718146661176", 3546718146661176LL);
        AssertMID(@"https://space.bilibili.com/3546718146661176", 3546718146661176LL);
        AssertMID(@"space.bilibili.com/1514206231?spm_id_from=333.337.0.0", 1514206231LL);
        AssertMID(@"https://m.bilibili.com/space/1514206231", 1514206231LL);
        AssertMID(@"https://www.bilibili.com/video/BV1xx411c7mD?mid=1514206231", 1514206231LL);
        AssertMID(@"https://www.bilibili.com/read/cv1?author_id=3546718146661176", 3546718146661176LL);
        AssertMID(@"bilibili://space/1514206231", 1514206231LL);
        AssertMID(@"https://www.example.com/space/1514206231", 0);
        AssertMID(@"https://fakebilibili.com/space/1514206231", 0);
        AssertMID(@"not-a-uid", 0);

        NSArray<NSDictionary *> *history = @[
            HistoryPoint(1400, 14),
            HistoryPoint(900, 9),
            HistoryPoint(1200, 12),
            HistoryPoint(1005, 10)
        ];
        NSInteger withBaseline[] = {9, 10, 12, 14};
        AssertTrendFollowers(history, 1000, withBaseline, 4);
        AssertTrendCount(history, 1000, 3);

        NSArray<NSDictionary *> *noPreviousHistory = @[
            HistoryPoint(1400, 14),
            HistoryPoint(1200, 12),
            HistoryPoint(1005, 10)
        ];
        NSInteger noBaseline[] = {10, 12, 14};
        AssertTrendFollowers(noPreviousHistory, 1000, noBaseline, 3);
        AssertTrendCount(noPreviousHistory, 1000, 3);

        NSInteger exactCutoff[] = {10, 12, 14};
        AssertTrendFollowers(history, 1005, exactCutoff, 3);
        AssertTrendCount(history, 1005, 3);
        AssertFilteredHistoryCount(history, 1005, 3);

        AssertTrendFollowers(history, 1600, NULL, 0);
        AssertTrendCount(history, 1600, 0);
        AssertFilteredHistoryCount(history, 1600, 0);

        NSString *fullCSV = CSVStringForHistoryPoints(noPreviousHistory, NO, 0);
        AssertContains(fullCSV, @"time,uid,followers\n");
        if ([fullCSV rangeOfString:@"scope"].location != NSNotFound) {
            fprintf(stderr, "Full history CSV should not include scope column: %s\n", fullCSV.UTF8String);
            abort();
        }

        NSString *trendCSV = CSVStringForHistoryPoints(TrendHistoryIncludingBaseline(history, 1000), YES, 1000);
        AssertContains(trendCSV, @"time,uid,followers,scope\n");
        AssertContains(trendCSV, @",9,baseline\n");
        AssertContains(trendCSV, @",10,range\n");
        AssertContains(trendCSV, @",14,range\n");

        NSString *period = HistoryCSVPeriodIdentifierForDate([NSDate dateWithTimeIntervalSince1970:0]);
        if (![period isEqualToString:@"1970-01"]) {
            fprintf(stderr, "Expected monthly CSV period 1970-01, got %s\n", period.UTF8String);
            abort();
        }

        AssertBiliError(@{@"code": @-352, @"message": @"风控校验失败"},
                        @"接口返回异常",
                        9,
                        -352,
                        @"风控校验失败（-352）");
        AssertBiliError(@{@"code": @-400, @"msg": @"请求错误"},
                        @"接口返回异常",
                        9,
                        -400,
                        @"请求错误（-400）");
        AssertBiliError(@{@"code": @0},
                        @"接口返回异常",
                        9,
                        9,
                        @"接口返回异常");

        AssertRecentResultsFallback();
        AssertHistoryDoesNotExpireAfterOneYear();
    }
    return 0;
}
