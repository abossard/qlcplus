import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtempSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { buildProject } from "./build-project.mjs";

const fixture = `
cmake_minimum_required(VERSION 3.16)
project(dev_control_build NONE)
option(qmlui "Build the QML application" OFF)
add_custom_target(qlcplus5 ALL
    COMMAND \${CMAKE_COMMAND} -E touch \${CMAKE_BINARY_DIR}/app.ready)
add_custom_target(io_plugin ALL
    COMMAND \${CMAKE_COMMAND} -E touch \${CMAKE_BINARY_DIR}/plugin.ready)
add_custom_target(resources ALL
    COMMAND \${CMAKE_COMMAND} -E touch \${CMAKE_BINARY_DIR}/resources.ready)
`;

function runStep(command, args, cwd) {
    const result = spawnSync(command, args, { cwd, encoding: "utf8" });
    if (result.error) throw result.error;
    assert.notEqual(result.status, null, result.stderr);
    return result.status;
}

for (const configured of [false, true]) {
    test(`builds app, plugins and resources (${configured ? "existing" : "fresh"} configuration)`, async (t) => {
        const root = mkdtempSync(join(tmpdir(), "qlcplus-build-"));
        t.after(() => rmSync(root, { recursive: true, force: true }));
        const buildDir = join(root, "build");
        writeFileSync(join(root, "CMakeLists.txt"), fixture);
        if (configured) {
            assert.equal(runStep("cmake", ["-S", root, "-B", buildDir, "-Dqmlui=ON"], root), 0);
        }

        const code = await buildProject(buildDir, runStep);

        assert.equal(code, 0);
        for (const artifact of ["app.ready", "plugin.ready", "resources.ready"]) {
            assert.ok(existsSync(join(buildDir, artifact)), `Missing build artifact: ${artifact}`);
        }
    });
}

test("does not build after a failed configuration", async (t) => {
    const root = mkdtempSync(join(tmpdir(), "qlcplus-build-"));
    t.after(() => rmSync(root, { recursive: true, force: true }));
    const buildDir = join(root, "build");
    writeFileSync(join(root, "CMakeLists.txt"), fixture + '\nmessage(FATAL_ERROR "expected configuration failure")\n');
    const commands = [];

    const code = await buildProject(buildDir, (command, args, cwd) => {
        commands.push(args);
        return runStep(command, args, cwd);
    });

    assert.notEqual(code, 0);
    assert.equal(commands.length, 1);
    assert.equal(existsSync(join(buildDir, "app.ready")), false);
});
