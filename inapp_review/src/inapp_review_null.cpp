#include "inapp_review_private.h"

#if !defined(DM_PLATFORM_ANDROID) && !defined(DM_PLATFORM_IOS) && !defined(DM_PLATFORM_OSX)

bool InAppReviewPlatform_IsSupported()
{
    return false;
}

InAppReviewResult InAppReviewPlatform_Request(int)
{
    return INAPP_REVIEW_UNSUPPORTED;
}

void InAppReviewPlatform_Finalize()
{
}

#endif
