# Contributing

Nori is developed by its maintainer; this repository carries the sources and the releases.
Contributions are welcome, with a specific focus: make this project more reliable and safe, not
bigger. Fixing defects matters more than adding features.

## What's welcome: hardening

- **Find bugs.** Stress-test, fuzz, throw strange inputs and edge cases at it, and report what breaks.
- **Fix bugs.** Spotted a real defect (a crash, a miscompile, a memory-safety hole, a wrong result, a
  race)? A small, focused pull request is welcome.
- **Tests.** Reproductions and regression tests that pin down correctness and safety.

Experiment with the language and tools, push on them, and send fixes that make them more correct.

## What's not accepted: new features and redesigns

New functionality, API or behavior changes, refactors for preference, and "I'd design it differently"
rewrites are not accepted here; the maintainer handles new implementations and direction. A PR
that adds or changes features (rather than fixing a defect) will be closed with thanks. Please open
an issue to discuss the idea instead.

## In short

- Found a bug? Open an issue; a focused fix PR is welcome.
- Have a feature idea or redesign? Open an issue to discuss; don't send an implementation.
- Want to help most? Test, fuzz, and hunt for safety and correctness problems.
