# Product

## Register

product

## Users
Lighting designers, DJs, and stage/club operators running live shows with QLC+ (this fork: `qlcplus5`, the v5 QML UI). They work at a desk or laptop, often in dim venues, frequently driving the rig from a hardware **MIDI controller** rather than the screen. Their context is time-pressured and live: a mistake is visible on the rig in real time. The fork adds DJ/VDJ integration, beat-synced shows, and AI/MCP tooling.

## Product Purpose
QLC+ controls lighting fixtures over DMX (Art-Net, sACN, USB-DMX, etc.). The new **Scene Workbench** lets an operator load an existing Scene, adjust it live via MIDI and/or on-screen DMX controls, and then **Update** the Scene or **Save as New** — so building and tweaking a show stays in the flow of performance, not buried in editor dialogs. Success = an operator can re-shape a look with their hands on a controller and commit it in seconds, never losing track of which fixtures/channels they're touching.

## Brand Personality
Professional console, not consumer app. Three words: **precise, dense, dependable.** It should feel like pro lighting/audio hardware software (a console surface), where every control maps to a real, physical output and the operator trusts what they see equals what the rig does.

## Anti-references
- Consumer SaaS dashboards (rounded pastel cards, big hero metrics, marketing whitespace).
- Cream/light "editorial" themes — this is a dim-venue dark tool.
- Decorative motion, gradient accents, glassmorphism. Motion only conveys state (a fader moving from MIDI, a channel toggling included/excluded).

## Design Principles
1. **What you see equals what the rig does.** Controls reflect live DMX; edits are visible immediately.
2. **Hands on the controller.** Every action is MIDI-bindable; the screen mirrors the hardware, it doesn't compete with it.
3. **Selection is explicit and reversible.** The operator always knows which fixtures and channel groups a save will write — and excluded ones are visibly excluded.
4. **Earned familiarity.** Match QLC+'s existing console vocabulary (UISettings dark theme, faders, fixture rows) — no invented affordances for standard tasks.
5. **Dense but legible in the dark.** High-contrast text, compact layout, no light-gray-on-charcoal.

## Accessibility & Inclusion
Dim-venue use: high contrast (body text ≥ 4.5:1 on charcoal), no reliance on color alone for include/exclude (use fill + icon + label). Respect reduced motion (fader/state changes crossfade or snap). Keyboard operable in addition to MIDI/pointer.
