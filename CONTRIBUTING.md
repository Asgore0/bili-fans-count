# Contributing

Thanks for improving BILI Fans Count.

## Development Rules

- Keep generated artifacts out of git. Use `outputs/` for local DMGs/APKs.
- Do not commit user-specific caches such as `.gradle/`, `.swift-module-cache/`, or signed app binaries.
- Prefer small, focused changes with a short explanation in the commit message.
- Run the relevant checks before opening a pull request.

## macOS Checks

```bash
clang -fobjc-arc -Wall -Wextra -Werror -fsyntax-only -mmacosx-version-min=12.0 macos-card/src/BILIFansCard.m

clang -fobjc-arc -Wall -Wextra -Werror -mmacosx-version-min=12.0 \
  -framework Cocoa \
  -framework QuartzCore \
  -framework UserNotifications \
  -framework ServiceManagement \
  macos-card/tests/UIDParsingTest.m \
  -o /tmp/bilifans-uid-parse-test

/tmp/bilifans-uid-parse-test
```

## Android Check

```bash
./gradlew :app:assembleDebug
```

## Packaging

```bash
./macos-card/package-macos.sh verified
```

Release binaries should be attached to GitHub Releases rather than committed to the repository.
