# macOS Source Parts

`../BILIFansCard.m` imports these files in order and remains the only compiled
Objective-C source file. Keep these parts ordered by dependency:

1. `BFCShared.inc`
2. `BFCSettings.inc`
3. `BFCBiliClient.inc`
4. `BFCViews.inc`
5. `BFCAppDelegate.inc`

This keeps static helpers available to the lightweight test harness while
making the implementation easier to navigate by responsibility.
