import { existsSync, mkdirSync } from "node:fs";
import { join } from "node:path";

export async function buildProject(buildDir, runStep) {
    mkdirSync(buildDir, { recursive: true });
    if (!existsSync(join(buildDir, "CMakeCache.txt"))) {
        const code = await runStep("cmake", ["..", "-Dqmlui=ON"], buildDir);
        if (code !== 0) return code;
    }
    return runStep("cmake", ["--build", ".", "-j8"], buildDir);
}
