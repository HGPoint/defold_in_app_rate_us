package com.defold.android.inappreview;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;
import com.google.android.play.core.review.ReviewInfo;
import com.google.android.play.core.review.ReviewManager;
import com.google.android.play.core.review.ReviewManagerFactory;

/** Google Play In-App Review bridge. Play operations run on the Android UI thread. */
public final class InAppReview {
    private static final String TAG = "InAppReview";
    private static final String PLAY_STORE_PACKAGE = "com.android.vending";
    private static final Object LOCK = new Object();

    // Keep these values aligned with inapp_review_private.h.
    private static final int OK = 0;
    private static final int UNSUPPORTED = 1;
    private static final int BUSY = 2;
    private static final int PLATFORM_ERROR = 4;
    private static final int FINISHED = 1;
    private static final int REQUEST_FAILED = 2;
    private static final int LAUNCH_FAILED = 3;
    private static final int NOT_VISIBLE = 4;

    private static boolean pending;
    private static int activeRequestId;
    private static int generation;

    private InAppReview() {}

    private static native void nativeOnResult(int requestId, int completion);

    public static boolean IsSupported(Activity activity) {
        if (activity == null || Build.VERSION.SDK_INT < 21) {
            return false;
        }
        try {
            ApplicationInfo store = activity.getPackageManager()
                    .getApplicationInfo(PLAY_STORE_PACKAGE, 0);
            return store.enabled;
        } catch (PackageManager.NameNotFoundException error) {
            return false;
        }
    }

    public static int Request(final Activity activity, final int requestId) {
        if (!IsSupported(activity)) {
            return UNSUPPORTED;
        }

        final int currentGeneration;
        synchronized (LOCK) {
            if (pending) {
                return BUSY;
            }
            pending = true;
            activeRequestId = requestId;
            currentGeneration = ++generation;
        }

        try {
            activity.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    startRequest(activity, requestId, currentGeneration);
                }
            });
        } catch (RuntimeException error) {
            Log.w(TAG, "Unable to schedule review request", error);
            clearRequest(requestId, currentGeneration);
            return PLATFORM_ERROR;
        }
        return OK;
    }

    public static void Reset() {
        synchronized (LOCK) {
            generation++;
            pending = false;
            activeRequestId = 0;
        }
    }

    private static boolean isCurrent(int requestId, int currentGeneration) {
        synchronized (LOCK) {
            return pending && activeRequestId == requestId && generation == currentGeneration;
        }
    }

    private static void clearRequest(int requestId, int currentGeneration) {
        synchronized (LOCK) {
            if (pending && activeRequestId == requestId && generation == currentGeneration) {
                pending = false;
                activeRequestId = 0;
            }
        }
    }

    private static void finish(int requestId, int currentGeneration, int completion) {
        synchronized (LOCK) {
            if (!pending || activeRequestId != requestId || generation != currentGeneration) {
                return;
            }
            pending = false;
            activeRequestId = 0;
        }
        nativeOnResult(requestId, completion);
    }

    private static boolean isVisible(Activity activity) {
        return !activity.isFinishing() && !activity.isDestroyed() && activity.hasWindowFocus();
    }

    private static void startRequest(final Activity activity, final int requestId,
                                     final int currentGeneration) {
        if (!isCurrent(requestId, currentGeneration)) {
            return;
        }
        if (!isVisible(activity)) {
            finish(requestId, currentGeneration, NOT_VISIBLE);
            return;
        }

        try {
            final ReviewManager manager = ReviewManagerFactory.create(activity);
            manager.requestReviewFlow().addOnCompleteListener(new OnCompleteListener<ReviewInfo>() {
                @Override
                public void onComplete(Task<ReviewInfo> task) {
                    if (!isCurrent(requestId, currentGeneration)) {
                        return;
                    }
                    if (!task.isSuccessful()) {
                        Log.w(TAG, "Unable to request review flow", task.getException());
                        finish(requestId, currentGeneration, REQUEST_FAILED);
                        return;
                    }
                    if (!isVisible(activity)) {
                        finish(requestId, currentGeneration, NOT_VISIBLE);
                        return;
                    }

                    try {
                        manager.launchReviewFlow(activity, task.getResult())
                                .addOnCompleteListener(new OnCompleteListener<Void>() {
                                    @Override
                                    public void onComplete(Task<Void> flow) {
                                        if (!isCurrent(requestId, currentGeneration)) {
                                            return;
                                        }
                                        if (flow.isSuccessful()) {
                                            // Play does not disclose whether a dialog was shown or a rating left.
                                            finish(requestId, currentGeneration, FINISHED);
                                        } else {
                                            Log.w(TAG, "Unable to launch review flow", flow.getException());
                                            finish(requestId, currentGeneration, LAUNCH_FAILED);
                                        }
                                    }
                                });
                    } catch (RuntimeException error) {
                        Log.w(TAG, "Unable to launch review flow", error);
                        finish(requestId, currentGeneration, LAUNCH_FAILED);
                    }
                }
            });
        } catch (RuntimeException error) {
            Log.w(TAG, "Unable to request review flow", error);
            finish(requestId, currentGeneration, REQUEST_FAILED);
        }
    }
}
