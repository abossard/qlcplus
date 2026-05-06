# P2C Audio Trigger QML Panels

Implemented read-only QML monitor panels for `VCAudioTriggersProperties.qml`:

- **Perceptual Bands**: sub, bass, low-mid, mid, and high colored power bars using exposed QML properties.
- **Envelope Monitor**: five placeholder envelope bars plus attack/release labels with TODOs for future bindings.
- **AGC**: placeholder gain meter and floor readouts with TODOs for future bindings.
- **Triggers**: five placeholder per-band trigger indicators and an active beat indicator.
- **Spectral**: placeholder centroid, flatness, and flux readouts with TODOs for future bindings.

Updated `VCAudioTriggersItem.qml` with a runtime monitor strip below the existing spectrum:

- five thin perceptual band level bars;
- beat and per-band trigger dots, with TODOs for future per-band trigger state exposure.

No C++ changes were made. Missing runtime data remains explicitly marked with QML TODO comments and placeholder values.
