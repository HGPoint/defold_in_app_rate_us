# Defold In-App Review (Rate Us)

`inapp_review` is a Defold native extension for the system rating request on
Google Play, iOS, and macOS. Other targets receive the same Lua module with
`is_supported() == false`.

```lua
-- Call from a script or GUI script after a positive moment in the game.
if inapp_review.is_supported() then
    local accepted, reason = inapp_review.request_review(function(self, result)
        if result.status == "error" then
            print("Review request failed:", result.reason)
        end
    end)
    if not accepted then
        print("Review request unavailable:", reason)
    end
end
```

The callback is optional. A call without it is also valid:

```lua
local accepted, reason = inapp_review.request_review()
```

`request_review()` returns `true, nil` if the extension accepted the request.
It returns `false, reason` for `unsupported`, `busy`, `not_visible`, or
`platform_error`. Invalid arguments raise a Lua error. Only one request may be
pending at a time.

If supplied, the callback receives `self, result` on the Defold update thread:

| Platform | `result.status` | Meaning |
| --- | --- | --- |
| Android | `finished` | The Google Play flow ended. It may not have shown a dialog. |
| iOS/macOS | `requested` | The StoreKit method was called. |
| Android | `error` | `result.reason` is `request_failed`, `launch_failed`, or `not_visible`. |

Neither API tells the game whether the dialog appeared, whether the user wrote
a review, or what rating they gave. Do not mark a player as having rated the
game from `accepted`, `finished`, or `requested`.

## Installation

Add an archive of a release of this repository to your Defold project
dependencies. The extension code is in `inapp_review/`; the Lua module is named
`inapp_review`. This repository can also be opened as a standalone Defold
project.

## Platforms

- **Android:** Google Play In-App Review library `2.0.2` on Android 5.0+.
  `is_supported()` checks for an enabled Play Store. The app also needs to be
  eligible for the Play review flow; a request can fail or be suppressed.
- **iOS:** StoreKit from iOS 10.3. On iOS 14+ with an active `UIWindowScene`,
  the request targets that scene. Older app lifecycles use the StoreKit
  compatibility method.
- **macOS:** StoreKit from macOS 10.14.
- **Other Defold targets:** `false, "unsupported"` from `request_review()`.

Apple's Objective-C `SKStoreReviewController` methods are used by this
Objective-C++ extension. Apple has deprecated them in newer SDKs in favor of
Swift APIs; the methods remain available for the supported OS versions.

## When to call it

Call the native prompt after a natural positive point, such as finishing a
level. Store quotas and user settings can suppress the popup. A **Rate Us**
button can instead open the store product page directly when a reliable action
is needed. For the App Store, append `action=write-review` to the product URL
when appropriate.

To test the Android UI, install a release through a Google Play test track;
a regular sideload does not exercise the real Play flow reliably. StoreKit's
request does not display a prompt in TestFlight builds.

## Additional testing notes

On Android, an app installed from a Google Play internal test track is exempt
from the normal review quota. The selected test account must have downloaded
the app through Play and have no current review; delete an existing review in
Play Store to repeat the UI test. Internal app sharing can show the UI but
disables review submission. On iOS/macOS, development builds show the request
UI for testing; TestFlight builds do not show it.

## Sources

- [Google Play In-App Review integration](https://developer.android.com/guide/playcore/in-app-review/kotlin-java)
- [Google Play In-App Review guidelines and testing](https://developer.android.com/guide/playcore/in-app-review)
- [Google Play review test setup](https://developer.android.com/guide/playcore/in-app-review/test)
- [Apple StoreKit review requests](https://developer.apple.com/documentation/storekit/requesting-app-store-reviews)
- [Defold native extensions](https://defold.com/manuals/extensions/)
