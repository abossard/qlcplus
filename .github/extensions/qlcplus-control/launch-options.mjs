export function timingLaunchOptions(input = {}, previous = {}, parentEnv = {}) {
    const timingDiagnostics = input.timingDiagnostics === undefined
        ? (previous.timingDiagnostics ?? false) : input.timingDiagnostics;
    const timingIntervalMs = input.timingIntervalMs === undefined
        ? (previous.timingIntervalMs ?? 1000) : input.timingIntervalMs;

    if (typeof timingDiagnostics !== "boolean") {
        throw new TypeError("timingDiagnostics must be a boolean.");
    }
    if (!Number.isSafeInteger(timingIntervalMs) || timingIntervalMs < 1) {
        throw new RangeError("timingIntervalMs must be a positive safe integer.");
    }

    return {
        timingDiagnostics,
        timingIntervalMs,
        env: {
            ...parentEnv,
            QLCPLUS_TIMING_DIAG: timingDiagnostics ? "1" : "0",
            QLCPLUS_TIMING_DIAG_MS: String(timingIntervalMs),
        },
    };
}
