# Elite Mission Radar

A mission filtering assistant for Elite Dangerous.

Instead of manually reading hundreds of mission entries, Elite Mission Radar watches the mission board for you and plays an alert when missions matching your criteria appear.

---

# Before You Continue

This README is divided into two sections.

## Section 1 — Player Guide

For most users.

Explains:

- What the tool does
- How to install it
- How to use it
- Common questions

## Section 2 — Technical Documentation

For developers and curious users.

Explains:

- Internal architecture
- OCR pipeline
- Detection logic
- Performance optimizations
- Error handling
- Design decisions

If you only want to use the software, you can stop reading after Section 1.

---

# What Problem Does This Solve?

While playing Elite Dangerous, I found myself spending a lot of time doing this:

```text
Open mission board
      ↓
Read mission
      ↓
Not interested
      ↓
Read next mission
      ↓
Not interested
      ↓
Read next mission
      ↓
...
```

Most of the time I was only interested in a few specific mission types, materials, or reward levels.

Eventually I got tired of manually reading every mission, so I built a tool to help me filter them.

That tool became Elite Mission Radar.

---

# What Does It Do?

The radar watches the mission board while you scroll through it.

Whenever a mission matching your filters appears, it immediately plays an alert sound.

Simple.

```text
Mission Board
      ↓
Scan
      ↓
Find Keywords
      ↓
Alarm
```

---

# Example

Suppose you are looking for:

✔ Gold missions

✔ Silver missions

✔ High-value rewards

But you do not want:

✘ Donation missions

✘ Return missions

✘ Low-value rewards

Instead of reading every mission manually, the radar checks them automatically and alerts you when something interesting appears.

---

# Is It Difficult To Use?

Not at all.

The software includes a built-in setup wizard and beginner-friendly instructions.

For most users:

```text
Download
      ↓
Extract
      ↓
Install
      ↓
Follow Instructions
      ↓
Done
```

No programming knowledge is required.

---

# Installation

## Option 1 — Recommended (Installer)

For most users, simply download the latest release and run the installer.

```text
EliteMissionRadar_Setup.exe
```

The installer will:

- Create the installation directory
- Copy all required files
- Install OCR data files
- Create Start Menu shortcuts
- Create an optional Desktop shortcut

After installation:

```text
Start Menu
      ↓
Elite Mission Radar
      ↓
Launch
```

The built-in setup wizard will guide you through the initial configuration.

No programming knowledge is required.

---

## Option 2 — Build From Source

If you do not feel comfortable running a precompiled executable, you can build the project yourself.

Because the project is fully open source, anyone can:

- Download the source code
- Review the code
- Compile the project locally
- Verify exactly what is being executed

Modern AI assistants can usually help with the compilation process if you are unfamiliar with C++ development environments.

Building from source provides the highest possible level of transparency.

---

## Antivirus Warnings

Some antivirus products may flag the executable.

This is usually caused by a combination of:

- High-frequency screenshot capture
- OCR functionality
- Lack of an expensive code-signing certificate

Unfortunately these characteristics are also common among certain types of malware, which can lead to false positives.

The project is fully open source and can be independently audited by anyone.

If you are concerned about security, you are encouraged to review the source code and build the application yourself.
---

# Is This Allowed?

Based on my understanding, yes.

The radar:

✔ Does not read game memory

✔ Does not modify game memory

✔ Does not inject code

✔ Does not automate gameplay

✔ Does not modify game files

✔ Does not interact with Elite Dangerous internals

The software simply analyzes screenshots already visible on your screen.

However, users are always responsible for ensuring compliance with Frontier policies and terms of service.

---

# Is This Cheating?

No gameplay actions are automated.

The radar does not:

✘ Accept missions

✘ Navigate menus

✘ Fly your ship

✘ Control your game

✘ Make decisions for you

It simply helps you find missions faster.

You still decide what to accept, what to ignore, and how to play.

This did not create any unfair advantage.

It's just a filtering machine, like the Elite Observatory.

---

# Is It Safe?

You may occasionally see a small number of antivirus vendors flag the executable.

This is usually caused by three things:

- The application captures screenshots
- The application performs OCR
- The executable is not digitally signed

Unfortunately these are behaviors sometimes used by malware, which can lead to false positives.

The radar itself:

✔ Has no networking functionality

✔ Does not upload data

✔ Does not save screenshots

✔ Processes everything locally

✔ Deletes all image data when the program exits

For complete transparency, the entire source code is publicly available.

Anyone can inspect, audit, or compile the project themselves.

---

# Need Help?

The software contains detailed built-in guidance for first-time users.

Most setup issues can be solved by simply following the on-screen instructions.

If you are interested in how the software actually works internally, continue to Section 2.



# ====================================
# PART 2 - Technical Documentation & Development Notes
# ====================================

This section is intended for developers and curious users.

If you only want to use the software, everything you need is already covered in Part 1.

The following sections explain how the scanner works internally, how the design evolved over time, and the many problems encountered during development.

1. Design Philosophy
2. Development History
3. Current Architecture
4. Unexpected Problems Encountered During Development
5. Performance Measurements
6. Fingerprint System Deep Dive
7. OCR Pipeline Deep Dive
8. Configuration System
9. Known Limitations
10. Future Improvements
11. Why Open Source?

---

# Design Philosophy

Elite Mission Radar was designed around one simple goal:

Reduce the amount of time players spend manually reading mission boards.

The scanner does not interact with Elite Dangerous in any way.

It does not:

- Read game memory
- Write game memory
- Inject code
- Automate gameplay

Instead, it behaves like a second pair of eyes.

It watches the mission board and alerts the player when missions matching predefined criteria appear.

All gameplay decisions remain entirely under player control.

---

# Development History

## Version 1 - Full OCR

The first prototype was extremely simple.

Whenever the user scrolled through the mission board, the scanner captured the entire mission area and immediately passed the image to Tesseract OCR.

Pipeline:

Mission Board
      ↓
Screenshot
      ↓
OCR
      ↓
Keyword Search

Functionally, this worked.

Performance-wise, it was a disaster.

Typical performance:

```text
1-2 FPS
```

Testing revealed an important requirement.

When scrolling through the mission board at maximum speed, the scanner must maintain approximately 5 FPS or higher to avoid missing missions entirely.

Version 1 failed this requirement.

The concept worked, but the implementation was not practical.

---

## Version 2 - Color-Based Candidate Filtering

The next question became:

Why OCR everything?

Most of the mission board consists of information that is irrelevant to the current search.

Instead of OCR-scanning every visible mission, the scanner first searches for visual indicators.

Players select the mission color they are interested in.

The scanner then applies color masks and searches for blue shareable-mission markers before OCR is considered.

Pipeline:

Screenshot
      ↓
Color Masks
      ↓
Blue Marker Detection
      ↓
Candidate Generation
      ↓
OCR

This dramatically reduced OCR workload.

Typical performance improved to:

```text
4-5 FPS
```

This was much better, but still not reliable enough.

---

## Version 3 - Fingerprint Cache

Performance profiling revealed an unexpected bottleneck.

OCR itself was not the main problem.

Repeated OCR was.

The scanner was repeatedly OCR-scanning the same mission cards over and over again.

To solve this, a fingerprint cache was introduced.

Whenever a mission is rejected, a compact image fingerprint is generated.

Pipeline:

Mission Image
      ↓
Resize To 128×8
      ↓
Fingerprint Generation
      ↓
Store In Cache

When a new candidate appears:

Mission Image
      ↓
Fingerprint Comparison
      ↓
Previously Seen?

If the image matches a previously rejected mission:

```text
Skip OCR
```

The mission is immediately discarded.

Only genuinely new mission cards are allowed to reach the OCR engine.

This optimization increased performance to approximately:

```text
12+ FPS
```

on the development machine.

At this point the scanner could comfortably keep up with rapid mission-board scrolling.

---

# Current Architecture

Current detection pipeline:

User Scrolls
      ↓
Input Detection
      ↓
Screenshot Capture
      ↓
Resolution Scaling
      ↓
Color Masks
      ↓
Blue Marker Detection
      ↓
Candidate Extraction
      ↓
Fingerprint Cache
      ↓
OCR
      ↓
Text Cleanup
      ↓
Whitelist Match
      ↓
Blacklist Match
      ↓
Reward Validation
      ↓
Duplicate Suppression
      ↓
Alert

The most important architectural principle is:

```text
Avoid OCR whenever possible.
```

OCR is the most expensive operation in the entire pipeline.

Most optimizations focus on preventing unnecessary OCR calls.

---

# Unexpected Problems Encountered During Development

One of the biggest surprises during development was discovering that OCR itself was only a small part of the challenge.

Most development time was spent solving practical problems surrounding OCR rather than OCR itself.

The following are some of the most memorable issues encountered during development.

---

## Full OCR Was Too Slow

The first prototype OCR-scanned the entire mission area every time the player scrolled.

It worked.

Unfortunately it only achieved roughly 1–2 FPS on the development machine.

Testing showed that mission boards could be scrolled faster than the scanner could process them, causing missions to appear and disappear before OCR ever saw them.

This eventually led to the introduction of color filtering and candidate generation.

---

## Duplicate OCR Was Worse Than OCR

Initially I assumed OCR speed was the primary bottleneck.

Performance profiling revealed a different story.

The scanner was repeatedly OCR-scanning the same mission cards over and over again.

In many cases, identical missions remained visible for multiple screenshots.

The solution was a fingerprint cache that allows previously rejected missions to bypass OCR entirely.

---

## Mission Titles Were Being Split Apart

Connected-component extraction worked well until mission titles contained spaces.

A title could be split into multiple independent regions, causing incomplete captures and OCR failures.

The solution was horizontal morphological dilation before component extraction.

Small gaps between words are intentionally filled to ensure the entire title is treated as a single object.

---

## OCR Output Contained Unexpected Formatting

OCR output frequently contained:

- Multiple spaces
- Tabs
- Newlines
- Carriage returns

Text that appeared visually identical could fail keyword matching because of invisible formatting differences.

A dedicated text-cleanup stage was added before any filtering logic is applied.

---

## OCR Mistakes Could Never Be Fully Eliminated

Real-world OCR proved far less predictable than expected.

Examples observed during testing included:

```text
GOLD → G0LD
GOLD → GCLD
SILVER → SlLVER

PHARMACEUTICAL
PHARMACEUT1CAL
```

Rather than attempting to eliminate every OCR mistake, the project eventually adopted the philosophy that OCR errors are unavoidable and should be handled gracefully.

---

## Windows DPI Scaling Broke Coordinate Calculations

Everything worked perfectly.

Until Windows scaling was set to 150%.

Then every coordinate became wrong.

The scanner originally assumed that reported window dimensions matched actual rendered dimensions.

Windows DPI scaling proved otherwise.

DPI awareness was later enabled to ensure coordinate calculations remained consistent.

---

## HUD Color Customization Could Blind The Scanner

Color masks are one of the most important performance optimizations in the project.

Unfortunately they depend on predictable HUD colors.

Changing Elite Dangerous HUD colors can prevent candidate generation from finding valid targets.

In extreme cases the scanner may become effectively blind.

For this reason the classic orange HUD is strongly recommended.

---

## Input Methods Were More Diverse Than Expected

Early versions assumed players would scroll using W and S.

Testing quickly showed this assumption was wrong.

Different players used:

- Keyboard
- Mouse side buttons
- Controller D-Pad

Input handling eventually evolved into a multi-source system capable of monitoring all supported scrolling methods.

---

## The Scanner Should Not Run All The Time

A continuously running scanner wastes resources.

Most screenshots are identical.

Most OCR operations produce no new information.

The project eventually shifted to an event-driven architecture.

Scanning is primarily triggered when user input indicates that mission-board contents may have changed.

This significantly reduced unnecessary processing.

---

## Cache Invalidation Was Surprisingly Important

A mission rejected today may become valuable tomorrow.

For example:

Yesterday:

```text
GOLD = Ignore
```

Today:

```text
GOLD = Desired
```

A previously correct fingerprint cache would suddenly become incorrect.

To prevent stale decisions, cache contents must be cleared whenever filtering rules change.

---

## OCR Failure Can Poison Future Detection

One particularly nasty issue involved fingerprint caching.

Imagine a valuable mission is OCR-scanned incorrectly and rejected.

If that rejection is immediately cached, future screenshots of the same mission may never reach OCR again.

In other words:

```text
Bad OCR
↓
Wrong Rejection
↓
Fingerprint Cache
↓
Permanent Rejection
```

Several safeguards were added to reduce the chance of valuable missions being permanently filtered out by OCR mistakes.

---

## Resolution Is Not The Only Variable

Supporting multiple resolutions turned out to be easier than expected.

Supporting runtime resolution changes was harder.

Window resizing, fullscreen changes, and display reconfiguration could all invalidate previously calculated coordinates.

The scanner eventually gained the ability to recalculate geometry dynamically.

---

## Performance Monitoring Needed Its Own Safeguards

A low FPS reading does not always indicate a real problem.

Short bursts of activity can produce misleading measurements.

Eventually performance warnings were tied not only to FPS values but also to sustained scrolling activity.

This reduced false alarms and made warnings more meaningful.

---

## Maximum FPS Was Not The Goal

Many optimization efforts initially focused on achieving the highest possible frame rate.

Over time it became clear that the real objective was reliability.

Testing showed that maintaining approximately 5 FPS or higher was sufficient to avoid missing missions during rapid scrolling.

Anything significantly above that threshold provided diminishing practical benefits.

The final design therefore prioritizes stability and consistency over raw benchmark numbers.

---

## OCR Was Only A Small Part Of The Project

The project started as an OCR experiment.

It eventually became a system involving:

- Screenshot capture
- Input monitoring
- Resolution scaling
- Color segmentation
- Candidate extraction
- Fingerprint caching
- OCR
- Text normalization
- Rule filtering
- Duplicate suppression
- Performance monitoring

Ironically, the OCR engine itself ended up being only one component in a much larger detection pipeline.

---

# Performance Measurements

The following measurements were collected on the development machine.

## Version Comparison

| Version | Technique | Typical FPS |
|----------|----------|----------|
| V1 | Full OCR | 1-2 FPS |
| V2 | Color Filtering | 4-5 FPS |
| V3 | Fingerprint Cache | 12+ FPS |

## Performance Goal

Testing showed that approximately 5 FPS is required to reliably scan mission boards during rapid scrolling.

The scanner therefore prioritizes maintaining a stable scan rate above this threshold rather than maximizing raw FPS.

## Major Cost Contributors

Approximate relative cost:

Screenshot Capture       Low

Color Filtering         Low

Connected Components    Low

Fingerprint Comparison  Very Low

OCR                     Very High

As a result, most optimizations focus on reducing OCR calls.

# Fingerprint System Deep Dive

Fingerprint caching is one of the most important optimizations in the entire project.

Workflow:

Mission Image
      ↓
Resize To 128×8
      ↓
Fingerprint Generation
      ↓
Cache Comparison

Only images that appear genuinely new are allowed to reach the OCR engine.

This dramatically reduces duplicate OCR calls.

## Why 128×8?

Through experimentation, this size provided sufficient detail while remaining extremely cheap to compare.

Smaller fingerprints increased collision rates.

Larger fingerprints provided little practical benefit.

The 8-pixel height is used to ignore the vertical distortion that often occurs when scrolling the panel.

# OCR Pipeline Deep Dive

OCR processing consists of several stages.

Screenshot
      ↓
Color Mask
      ↓
Morphological Operations
      ↓
Connected Components
      ↓
Candidate Extraction
      ↓
OCR
      ↓
Text Cleanup
      ↓
Keyword Matching

Each stage exists because a previous version failed without it.

# Configuration System

The scanner is intentionally configurable.

Players can modify:

- White list
- Black list
- Reward requirements
- Scan speed
- Detection behavior

Configuration changes immediately affect scanner behavior.

Some changes automatically invalidate caches to prevent stale results.

# Known Limitations

- OCR is not perfect.
- Custom HUD colors may reduce detection quality.
- Future Elite Dangerous UI updates may require adjustments.
- Extremely low FPS may cause mission loss.
- OCR quality depends on screenshot quality.
- The scanner only works when mission information is visible on screen.

- # Future Improvements

Potential future improvements include:

- Adaptive color detection
- Automatic HUD calibration
- Smarter OCR correction
- Improved fingerprint algorithms
- Better support for unusual resolutions
- Optional machine-learning based filtering

- # Why Open Source?

Elite Mission Radar performs screenshot capture and OCR.

While the application never uploads data and performs all processing locally, such behavior can understandably raise security concerns.

For that reason the entire project is open source.

Anyone can:

- Review the code
- Audit the behavior
- Compile it independently
- Verify every claim made in this documentation

Trust should never be required when verification is possible.
