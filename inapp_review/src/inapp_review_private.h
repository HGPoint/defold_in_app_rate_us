#pragma once

enum InAppReviewResult
{
    INAPP_REVIEW_OK = 0,
    INAPP_REVIEW_UNSUPPORTED = 1,
    INAPP_REVIEW_BUSY = 2,
    INAPP_REVIEW_NOT_VISIBLE = 3,
    INAPP_REVIEW_PLATFORM_ERROR = 4,
};

enum InAppReviewCompletion
{
    INAPP_REVIEW_REQUESTED = 0,
    INAPP_REVIEW_FINISHED = 1,
    INAPP_REVIEW_REQUEST_FAILED = 2,
    INAPP_REVIEW_LAUNCH_FAILED = 3,
    INAPP_REVIEW_COMPLETION_NOT_VISIBLE = 4,
};

bool InAppReviewPlatform_IsSupported();
InAppReviewResult InAppReviewPlatform_Request(int request_id);
void InAppReviewPlatform_Finalize();

// Platform callbacks may run on the Android UI thread. The Lua callback is
// dispatched from the extension's Update function on the Defold thread.
void InAppReview_Complete(int request_id, InAppReviewCompletion result);
