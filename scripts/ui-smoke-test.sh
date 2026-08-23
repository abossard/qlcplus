#!/usr/bin/env bash
# UI smoke test for QLC+ 5 (qlcplus5).
#
# Complements scripts/smoke-test.sh, which covers the MCP/REST/security surface.
# This one answers a different question: did the app actually come up as a usable
# UI, and are the service-level behaviours the UI depends on correct?
#
# It is deliberately runnable against an app the user already has open -- it
# creates nothing it does not delete, and never stops a running server.
#
# Usage:
#   ./scripts/ui-smoke-test.sh                 # against an already-running app
#   ./scripts/ui-smoke-test.sh --launch        # launch qlcplus5 first, stop it after
#
# Env overrides: MCP_URL, WEB_URL, QLCPLUS_BIN, WORKSPACE

set -uo pipefail

MCP_URL="${MCP_URL:-http://127.0.0.1:9696/mcp}"
WEB_URL="${WEB_URL:-http://localhost:9999}"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QLCPLUS_BIN="${QLCPLUS_BIN:-$REPO_ROOT/build/qmlui/qlcplus5}"
WORKSPACE="${WORKSPACE:-$REPO_ROOT/GARAGE.qxw}"

PASS=0; FAIL=0; SKIP=0
GREEN=$'\033[32m'; RED=$'\033[31m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
pass() { printf '  %sPASS%s %s\n' "$GREEN" "$RESET" "$1"; PASS=$((PASS+1)); }
fail() { printf '  %sFAIL%s %s — %s\n' "$RED" "$RESET" "$1" "${2:-}"; FAIL=$((FAIL+1)); }
skip() { printf '  %sSKIP%s %s — %s\n' "$YELLOW" "$RESET" "$1" "${2:-}"; SKIP=$((SKIP+1)); }
section() { printf '\n%s=== %s ===%s\n' "$BOLD" "$1" "$RESET"; }

command -v curl >/dev/null || { echo "curl required" >&2; exit 2; }
command -v jq  >/dev/null || { echo "jq required" >&2; exit 2; }

LAUNCHED_PID=""
cleanup() {
    [[ -n "$LAUNCHED_PID" ]] && kill "$LAUNCHED_PID" 2>/dev/null
    rm -f "$HDR" "$HDR.body" 2>/dev/null
}
HDR="$(mktemp -t qlcuismoke)"
trap cleanup EXIT

port_listening() { lsof -nP -iTCP:"$1" -sTCP:LISTEN >/dev/null 2>&1; }

# ---------------------------------------------------------------- launch mode
if [[ "${1:-}" == "--launch" ]]; then
    if pgrep -f 'qlcplus5' >/dev/null 2>&1; then
        echo "An instance is already running; refusing to launch a second one." >&2
        echo "Run without --launch to test the running instance." >&2
        exit 2
    fi
    [[ -x "$QLCPLUS_BIN" ]] || { echo "Not built: $QLCPLUS_BIN" >&2; exit 2; }
    "$QLCPLUS_BIN" -d ${WORKSPACE:+-o "$WORKSPACE"} >/dev/null 2>&1 &
    LAUNCHED_PID=$!
    for _ in $(seq 1 60); do
        port_listening 9696 && break
        kill -0 "$LAUNCHED_PID" 2>/dev/null || { echo "app exited during startup" >&2; exit 1; }
        sleep 1
    done
fi

printf '%sQLC+ UI Smoke Test%s\n' "$BOLD" "$RESET"
printf 'MCP: %s\nWEB: %s\n' "$MCP_URL" "$WEB_URL"

# ------------------------------------------------------------ 1. process/UI up
section "1. Application process"

if pgrep -f 'qlcplus5' >/dev/null 2>&1; then
    pass "1.1 qlcplus5 process is running"
else
    fail "1.1 qlcplus5 process is running" "not found — start it, or pass --launch"
    printf '\n%sResults: %d passed, %d failed, %d skipped%s\n' "$BOLD" "$PASS" "$FAIL" "$SKIP" "$RESET"
    exit 1
fi

# A QML app that failed to build its scene graph still has a live process, so
# check it actually owns an on-screen window rather than trusting the PID.
if command -v osascript >/dev/null 2>&1; then
    win=$(osascript -e 'tell application "System Events" to tell (first process whose name contains "qlcplus") to count windows' 2>/dev/null)
    if [[ "${win:-0}" -ge 1 ]]; then
        pass "1.2 UI window is mapped ($win window(s))"

        # A window that exists but has collapsed to nothing means the QML scene
        # graph failed to build -- the process survives, so size is the tell.
        geom=$(osascript -e 'tell application "System Events" to tell (first process whose name contains "qlcplus") to get size of first window' 2>/dev/null)
        w=$(printf '%s' "$geom" | cut -d, -f1 | tr -d ' ')
        h=$(printf '%s' "$geom" | cut -d, -f2 | tr -d ' ')
        if [[ "${w:-0}" -ge 640 && "${h:-0}" -ge 480 ]]; then
            pass "1.3 Window has a usable size (${w}x${h})"
        else
            fail "1.3 Window has a usable size" "got ${w}x${h} — QML scene graph may have failed to build"
        fi
    else
        skip "1.2 UI window is mapped" "no window reported (grant Accessibility to your terminal, or headless session)"
        skip "1.3 Window has a usable size" "no window to measure"
    fi
else
    skip "1.2 UI window is mapped" "osascript unavailable"
    skip "1.3 Window has a usable size" "osascript unavailable"
fi

# -------------------------------------------------- 2. server-type behaviour
# Regression guard for the forced-web-server fix. The fork enables web access by
# default; that must NOT drag the native server up with it, and must NOT stop the
# workspace's own settings from being honoured.
section "2. Server types on a default launch"

if port_listening 9999; then
    pass "2.1 Web server listening on 9999"
else
    fail "2.1 Web server listening on 9999" "the whole web UI depends on this"
fi

if port_listening 9998; then
    fail "2.2 Native server NOT started by default" "9998 is listening — a default launch must not open it"
else
    pass "2.2 Native server not started by default (9998 closed)"
fi

if port_listening 9696; then
    pass "2.3 MCP server listening on 9696"
else
    fail "2.3 MCP server listening on 9696" "built without -Dmcp_server=ON?"
fi

# ------------------------------------------------------------- 3. web UI serves
section "3. Web UI"

code=$(curl -s -o /dev/null -w '%{http_code}' -m 10 "$WEB_URL/vc/" 2>/dev/null)
ctype=$(curl -sI -m 10 "$WEB_URL/vc/" 2>/dev/null | awk 'tolower($1)=="content-type:"{print $2}' | tr -d '\r')
if [[ "$code" == "200" && "$ctype" == text/html* ]]; then
    pass "3.1 /vc/ serves the SPA (HTTP $code, $ctype)"
else
    fail "3.1 /vc/ serves the SPA" "HTTP $code, Content-Type '$ctype' — run 'npm run build' in webaccess/web-dmx"
fi

html=$(curl -s -m 10 "$WEB_URL/vc/" 2>/dev/null)
js=$(printf '%s' "$html" | grep -oE 'assets/index-[A-Za-z0-9_-]+\.js' | head -1)
if [[ -n "$js" ]]; then
    jscode=$(curl -s -o /dev/null -w '%{http_code}' -m 10 "$WEB_URL/vc/$js" 2>/dev/null)
    [[ "$jscode" == "200" ]] && pass "3.2 SPA JS bundle loads ($js)" \
                             || fail "3.2 SPA JS bundle loads" "$js returned HTTP $jscode"
else
    fail "3.2 SPA JS bundle loads" "no asset reference found in /vc/ HTML"
fi

# ---------------------------------------------------- 4. MCP drives live state
section "4. MCP reflects live UI state"

mcp_session=""
curl -sS -D "$HDR" -X POST "$MCP_URL" -H 'Content-Type: application/json' \
    -d '{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"ui-smoke","version":"1.0"}}}' \
    -o "$HDR.body" >/dev/null 2>&1
mcp_session=$(awk 'tolower($1)=="mcp-session-id:"{print $2}' "$HDR" | tr -d '\r')

mcp() {  # mcp <tool> <json-args>
    curl -sS -m 20 -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -H "Mcp-Session-Id: $mcp_session" \
        -d "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"tools/call\",\"params\":{\"name\":\"$1\",\"arguments\":$2}}" 2>/dev/null
}

# Tool results arrive as JSON encoded into a text field, so unwrap before parsing.
mcp_text() { jq -r '.result.content[0].text // empty' 2>/dev/null; }
mcp_len()  { mcp_text | jq 'if type=="array" then length elif type=="object" then (.[]|arrays|length) // 0 else 0 end' 2>/dev/null | head -1; }

if [[ -z "$mcp_session" ]]; then
    fail "4.1 MCP session established" "no Mcp-Session-Id returned"
else
    pass "4.1 MCP session established (${mcp_session:0:8}…)"

    fx=$(mcp query_fixtures '{}' | mcp_len)
    if [[ "${fx:-0}" -gt 0 ]]; then
        pass "4.2 Fixtures visible through MCP ($fx)"
    else
        skip "4.2 Fixtures visible through MCP" "workspace has no fixtures loaded"
    fi

    # The Virtual Console is the UI surface most of this fork's tooling drives.
    vc=$(mcp vc_query_pages '{}')
    if printf '%s' "$vc" | grep -q '"error"'; then
        fail "4.3 Virtual Console queryable" "$(printf '%s' "$vc" | head -c 120)"
    else
        pages=$(printf '%s' "$vc" | mcp_len)
        pass "4.3 Virtual Console queryable ($pages page(s))"
    fi
fi

# ------------------------------------------- 5. generated chasers are usable
# Every Stage Wizard generator that hands back a Chaser must register it with the
# Doc. An unregistered chaser keeps Function::invalidId(), never enters the
# function tree, and leaks -- the effect appears to generate but does not play.
section "5. Generated functions are registered"

if [[ -n "$mcp_session" ]]; then
    funcs=$(mcp query_functions '{}')
    if printf '%s' "$funcs" | grep -q '"error"'; then
        fail "5.1 Function list retrievable" "$(printf '%s' "$funcs" | head -c 120)"
    else
        total=$(printf '%s' "$funcs" | mcp_len)
        pass "5.1 Function list retrievable ($total functions)"

        # An id of 4294967295 (Function::invalidId()) reaching the tree is the
        # signature of the unregistered-chaser bug.
        if printf '%s' "$funcs" | mcp_text | grep -q '4294967295'; then
            fail "5.2 No function carries invalidId()" "an unregistered function reached the tree"
        else
            pass "5.2 No function carries invalidId()"
        fi
    fi
else
    skip "5.1 Function list retrievable" "no MCP session"
    skip "5.2 No function carries invalidId()" "no MCP session"
fi

# ------------------------------------------------------------------- results
printf '\n%sResults: %s%d passed%s, %s%d failed%s, %s%d skipped%s\n' \
    "$BOLD" "$GREEN" "$PASS" "$RESET" "$RED" "$FAIL" "$RESET" "$YELLOW" "$SKIP" "$RESET"
[[ "$FAIL" -eq 0 ]]
