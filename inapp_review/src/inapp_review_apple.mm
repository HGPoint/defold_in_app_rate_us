#include "inapp_review_private.h"

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX)

#import <Foundation/Foundation.h>
#import <StoreKit/StoreKit.h>

#if defined(DM_PLATFORM_IOS)
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

#include <dispatch/dispatch.h>

bool InAppReviewPlatform_IsSupported()
{
#if defined(DM_PLATFORM_IOS)
    if (@available(iOS 10.3, *))
        return true;
#else
    if (@available(macOS 10.14, *))
        return true;
#endif
    return false;
}

static InAppReviewResult RequestOnMainThread(int request_id)
{
    if (!InAppReviewPlatform_IsSupported())
        return INAPP_REVIEW_UNSUPPORTED;

#if defined(DM_PLATFORM_IOS)
    UIApplication* application = [UIApplication sharedApplication];
    if (application.applicationState != UIApplicationStateActive)
        return INAPP_REVIEW_NOT_VISIBLE;

    if (@available(iOS 14.0, *))
    {
        NSSet<UIScene*>* scenes = application.connectedScenes;
        for (UIScene* scene in scenes)
        {
            if (![scene isKindOfClass:[UIWindowScene class]] ||
                scene.activationState != UISceneActivationStateForegroundActive)
                continue;

            // StoreKit controls whether a prompt appears; this reports only that
            // the request reached StoreKit.
            [SKStoreReviewController requestReviewInScene:(UIWindowScene*)scene];
            InAppReview_Complete(request_id, INAPP_REVIEW_REQUESTED);
            return INAPP_REVIEW_OK;
        }

        // Defold can run with the pre-scene UIApplication lifecycle on iOS 14+.
        if (scenes.count != 0)
            return INAPP_REVIEW_NOT_VISIBLE;
    }
#else
    if (![NSApp isActive])
        return INAPP_REVIEW_NOT_VISIBLE;
#endif

    // The Objective-C SKStoreReviewController API is deprecated in newer SDKs;
    // it remains necessary for iOS without scenes and for this macOS bridge.
    [SKStoreReviewController requestReview];
    InAppReview_Complete(request_id, INAPP_REVIEW_REQUESTED);
    return INAPP_REVIEW_OK;
}

InAppReviewResult InAppReviewPlatform_Request(int request_id)
{
    if ([NSThread isMainThread])
        return RequestOnMainThread(request_id);

    __block InAppReviewResult result = INAPP_REVIEW_PLATFORM_ERROR;
    dispatch_sync(dispatch_get_main_queue(), ^{
        result = RequestOnMainThread(request_id);
    });
    return result;
}

void InAppReviewPlatform_Finalize()
{
}

#endif
