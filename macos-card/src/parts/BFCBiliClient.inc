// Bilibili API client and request fallback behavior.
@interface BiliFansClient : NSObject
+ (void)fetchMID:(long long)mid fallbackName:(NSString *)fallbackName completion:(void (^)(FanResult *result, NSError *error))completion;
+ (BOOL)isTransientRequestError:(NSError *)error;
@end

@implementation BiliFansClient

static NSInteger const BiliFansHTTPErrorCodeBase = 10000;
static NSInteger const BiliFansTransientRetryLimit = 1;

+ (BOOL)shouldTryFollowersFallbackAfterCardError:(NSError *)error {
    if (!error) {
        return YES;
    }
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        return NO;
    }
    return YES;
}

+ (BOOL)isTransientRequestError:(NSError *)error {
    if (!error) {
        return NO;
    }
    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorTimedOut:
            case NSURLErrorNetworkConnectionLost:
            case NSURLErrorCannotFindHost:
            case NSURLErrorCannotConnectToHost:
            case NSURLErrorDNSLookupFailed:
            case NSURLErrorNotConnectedToInternet:
                return YES;
            default:
                return NO;
        }
    }
    if ([error.domain isEqualToString:@"BILIFansCard"]) {
        NSInteger statusCode = error.code - BiliFansHTTPErrorCodeBase;
        return statusCode == 429 || (statusCode >= 500 && statusCode < 600);
    }
    return NO;
}

+ (NSTimeInterval)retryDelayForAttempt:(NSInteger)attempt {
    if (attempt <= 0) {
        return 0.65;
    }
    return MIN(2.0, 0.65 * pow(2.0, (double)attempt));
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
    [self get:address attempt:0 completion:completion];
}

+ (void)get:(NSString *)address attempt:(NSInteger)attempt completion:(void (^)(NSDictionary *, NSError *))completion {
    NSURL *url = [NSURL URLWithString:address];
    if (!url) {
        completion(nil, [NSError errorWithDomain:@"BILIFansCard" code:4 userInfo:@{NSLocalizedDescriptionKey: @"请求地址无效"}]);
        return;
    }
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:10];
    [request setValue:AppUserAgentString() forHTTPHeaderField:@"User-Agent"];
    [request setValue:@"https://www.bilibili.com/" forHTTPHeaderField:@"Referer"];
    [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
    void (^finish)(NSDictionary *, NSError *) = ^(NSDictionary *root, NSError *requestError) {
        if (requestError && attempt < BiliFansTransientRetryLimit && [self isTransientRequestError:requestError]) {
            NSTimeInterval delay = [self retryDelayForAttempt:attempt];
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
                           dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                [self get:address attempt:attempt + 1 completion:completion];
            });
            return;
        }
        completion(root, requestError);
    };
    NSURLSessionDataTask *task = [[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        if (error) {
            finish(nil, error);
            return;
        }
        if (![response isKindOfClass:NSHTTPURLResponse.class]) {
            finish(nil, [NSError errorWithDomain:@"BILIFansCard" code:5 userInfo:@{NSLocalizedDescriptionKey: @"响应类型异常"}]);
            return;
        }
        NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
        if (http.statusCode < 200 || http.statusCode >= 300) {
            finish(nil, [NSError errorWithDomain:@"BILIFansCard" code:BiliFansHTTPErrorCodeBase + http.statusCode userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"HTTP %ld", (long)http.statusCode]}]);
            return;
        }
        if (data.length == 0) {
            finish(nil, [NSError errorWithDomain:@"BILIFansCard" code:6 userInfo:@{NSLocalizedDescriptionKey: @"响应为空"}]);
            return;
        }
        id parsed = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
        if (![parsed isKindOfClass:NSDictionary.class]) {
            finish(nil, error ?: [NSError errorWithDomain:@"BILIFansCard" code:7 userInfo:@{NSLocalizedDescriptionKey: @"JSON 格式异常"}]);
            return;
        }
        finish(parsed, nil);
    }];
    [task resume];
}
@end
