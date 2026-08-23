#!/usr/bin/env bash
# Automated smoke test for QLC+ — covers all curl-based checks from MANUAL_REVIEW.md.
# Requires: a running QLC+ instance with GARAGE.qxw loaded (web server is on by default).
#
# Usage: ./scripts/smoke-test.sh
set -euo pipefail

# ---------- Config ----------
MCP_URL="${MCP_URL:-http://127.0.0.1:9696/mcp}"
WEB_URL="${WEB_URL:-http://localhost:9999}"
SCENE_NAME="MANUAL_TEST_SCENE"
CHASER_NAME="SMOKE_BEAT_TEST"

PASS=0
FAIL=0

# ---------- Output helpers ----------
GREEN=$'\033[32m'
RED=$'\033[31m'
BOLD=$'\033[1m'
RESET=$'\033[0m'

section() { echo "${BOLD}=== $1 ===${RESET}"; }
pass() { echo "  ${GREEN}PASS${RESET} $1"; PASS=$((PASS+1)); }
fail() { echo "  ${RED}FAIL${RESET} $1: $2"; FAIL=$((FAIL+1)); }

# ---------- Preflight ----------
command -v jq >/dev/null 2>&1 || { echo "jq is required" >&2; exit 2; }
command -v curl >/dev/null 2>&1 || { echo "curl is required" >&2; exit 2; }

MCP_SESSION_ID=""
MCP_HEADERS_FILE="$(dirname "$0")/.mcp-headers.$$"
trap 'rm -f "$MCP_HEADERS_FILE" "$MCP_HEADERS_FILE.body"' EXIT

mcp_init_session() {
    # Writes response body to stdout, captures Mcp-Session-Id into MCP_SESSION_ID.
    # Must NOT be called via $(mcp_init_session) — would lose the variable.
    # Instead: call once, store body in a file, then read.
    curl -sS -D "$MCP_HEADERS_FILE" -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -d '{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"smoke-test","version":"1.0"}}}' \
        -o "$MCP_HEADERS_FILE.body" 2>/dev/null || true
    if [[ -f "$MCP_HEADERS_FILE" ]]; then
        MCP_SESSION_ID=$(awk 'tolower($1)=="mcp-session-id:" {sub(/\r$/,"",$2); print $2; exit}' "$MCP_HEADERS_FILE")
    fi
}

# Per MCP spec, the client must send `notifications/initialized` after the
# initialize handshake before issuing tool calls.
mcp_send_initialized() {
    [[ -z "$MCP_SESSION_ID" ]] && return 0
    curl -sS -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -H "Mcp-Session-Id: $MCP_SESSION_ID" \
        -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
        -o /dev/null 2>/dev/null || true
}

# ---------- MCP helper ----------
# Returns the raw JSON-RPC response body (stdout) or empty on transport failure.
mcp_call() {
    if [[ -n "$MCP_SESSION_ID" ]]; then
        curl -sS -X POST "$MCP_URL" \
            -H 'Content-Type: application/json' \
            -H "Mcp-Session-Id: $MCP_SESSION_ID" \
            -d "$1" 2>/dev/null || true
    else
        curl -sS -X POST "$MCP_URL" \
            -H 'Content-Type: application/json' \
            -d "$1" 2>/dev/null || true
    fi
}

# Extract the inner text payload of a tools/call result and parse it as JSON.
# QLC+ MCP tools wrap their JSON response in result.content[0].text as a string.
mcp_result_json() {
    echo "$1" | jq -r '.result.content[0].text // empty' | jq '.' 2>/dev/null || echo ""
}

# Raw text payload (for shape inspection / error messages).
mcp_result_text() {
    echo "$1" | jq -r '.result.content[0].text // empty'
}

echo "${BOLD}QLC+ Smoke Test${RESET}"
echo "MCP:  $MCP_URL"
echo "WEB:  $WEB_URL"
echo

# =====================================================================
section "Section 2: MCP Server"
# =====================================================================

# 2.1 Server reachable — GET should respond with 405 or 400 (not connection refused)
code=$(curl -s -o /dev/null -w "%{http_code}" "$MCP_URL" 2>/dev/null || echo "000")
if [[ "$code" == "405" || "$code" == "400" || "$code" == "404" ]]; then
    pass "2.1 Server reachable (HTTP $code)"
elif [[ "$code" == "000" ]]; then
    fail "2.1 Server reachable" "connection refused — is QLC+ running?"
    echo "Aborting remaining tests." >&2
    echo
    echo "${BOLD}=== Results: $PASS passed, $FAIL failed ===${RESET}"
    exit 1
elif [[ "$code" =~ ^5 ]]; then
    fail "2.1 Server reachable" "server error (HTTP $code)"
else
    pass "2.1 Server reachable (HTTP $code)"
fi

# 2.2 Initialize handshake — also captures the Mcp-Session-Id used by all later calls.
mcp_init_session
init_resp=$(cat "$MCP_HEADERS_FILE.body" 2>/dev/null || true)
rm -f "$MCP_HEADERS_FILE.body"
server_name=$(echo "$init_resp" | jq -r '.result.serverInfo.name // empty')
if [[ -n "$server_name" && -n "$MCP_SESSION_ID" ]]; then
    pass "2.2 Initialize handshake (serverInfo.name=$server_name, session=${MCP_SESSION_ID:0:8}…)"
    mcp_send_initialized
elif [[ -n "$server_name" ]]; then
    fail "2.2 Initialize handshake" "got serverInfo but no Mcp-Session-Id header"
else
    fail "2.2 Initialize handshake" "missing result.serverInfo: $init_resp"
fi

# 2.3 Tool list — floor bumped whenever a batch of tools lands, so a dropped
# registration fails here instead of silently shrinking the surface.
TOOL_COUNT_FLOOR=61
tools_resp=$(mcp_call '{"jsonrpc":"2.0","id":2,"method":"tools/list"}')
tool_count=$(echo "$tools_resp" | jq -r '.result.tools | length // 0')
if [[ "$tool_count" -ge "$TOOL_COUNT_FLOOR" ]]; then
    pass "2.3 Tool list ($tool_count tools)"
else
    fail "2.3 Tool list" "expected ≥$TOOL_COUNT_FLOOR tools, got $tool_count"
fi

# 2.3b Batch 1 setup/teardown tools are present
missing_tools=""
for t in delete_universes delete_fixtures delete_fixture_groups vc_delete_pages; do
    echo "$tools_resp" | jq -e --arg t "$t" '.result.tools[] | select(.name == $t)' >/dev/null 2>&1 \
        || missing_tools="$missing_tools $t"
done
if [[ -z "$missing_tools" ]]; then
    pass "2.3b Setup/teardown tools registered"
else
    fail "2.3b Setup/teardown tools registered" "missing:$missing_tools"
fi

# 2.4 query_fixtures — array with id, name
qf_resp=$(mcp_call '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"query_fixtures","arguments":{}}}')
qf_json=$(mcp_result_json "$qf_resp")
if [[ -n "$qf_json" ]]; then
    fix_count=$(echo "$qf_json" | jq 'length' 2>/dev/null || echo 0)
    has_fields=$(echo "$qf_json" | jq -r '.[0] | if (.id != null and .name != null) then "yes" else "no" end' 2>/dev/null || echo "no")
    if [[ "$fix_count" -gt 0 && "$has_fields" == "yes" ]]; then
        pass "2.4 query_fixtures ($fix_count fixtures with id+name)"
    else
        fail "2.4 query_fixtures" "count=$fix_count hasFields=$has_fields"
    fi
else
    fail "2.4 query_fixtures" "no JSON payload in response: $qf_resp"
fi

# 2.5 Idempotent scene create — create twice, expect same id, only one exists.
# Use create_scenes (the actual batch tool name in this fork).
make_scene_payload() {
    cat <<EOF
{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"create_scenes","arguments":{"items":[{"name":"$SCENE_NAME"}]}}}
EOF
}

extract_first_id() {
    # Result text is a JSON array of {id, name, status, ...}.
    echo "$1" | jq -r '.[0].id // empty' 2>/dev/null || true
}

resp1=$(mcp_call "$(make_scene_payload)")
text1=$(mcp_result_json "$resp1")
id1=$(extract_first_id "$text1")

resp2=$(mcp_call "$(make_scene_payload)")
text2=$(mcp_result_json "$resp2")
id2=$(extract_first_id "$text2")

if [[ -n "$id1" && "$id1" == "$id2" ]]; then
    pass "2.5 Idempotent scene create (id=$id1 returned both times)"
else
    fail "2.5 Idempotent scene create" "id1='$id1' id2='$id2' (resp1=$resp1)"
fi

# Verify only one MANUAL_TEST_SCENE exists via query_functions.
qfn_resp=$(mcp_call '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"query_functions","arguments":{}}}')
qfn_json=$(mcp_result_json "$qfn_resp")
if [[ -n "$qfn_json" ]]; then
    matches=$(echo "$qfn_json" | jq --arg n "$SCENE_NAME" 'map(select(.name == $n)) | length' 2>/dev/null || echo 0)
    if [[ "$matches" == "1" ]]; then
        pass "2.5 Idempotent scene create — only one '$SCENE_NAME' exists"
    else
        fail "2.5 Idempotent scene create" "found $matches scenes named '$SCENE_NAME'"
    fi
else
    fail "2.5 Idempotent scene create" "query_functions returned no JSON payload"
fi

# Cleanup: delete the scene we created.
if [[ -n "$id1" ]]; then
    del_resp=$(mcp_call "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"delete_functions\",\"arguments\":{\"ids\":[$id1]}}}")
    if [[ -n "$(echo "$del_resp" | jq -r '.result // empty')" ]]; then
        pass "2.5 Cleanup — deleted scene id=$id1"
    else
        fail "2.5 Cleanup" "delete_functions response: $del_resp"
    fi
fi

echo

# =====================================================================
section "Section 3.20: REST API"
# =====================================================================

fix_count=$(curl -sS "$WEB_URL/api/fixtures" 2>/dev/null | jq 'length' 2>/dev/null || echo 0)
if [[ "$fix_count" =~ ^[0-9]+$ && "$fix_count" -gt 0 ]]; then
    pass "3.20 /api/fixtures returns $fix_count fixtures"
else
    fail "3.20 /api/fixtures" "count=$fix_count (expected >0)"
fi

ch_count=$(curl -sS "$WEB_URL/api/channels" 2>/dev/null | jq '. | length' 2>/dev/null || echo 0)
if [[ "$ch_count" =~ ^[0-9]+$ && "$ch_count" -gt 0 ]]; then
    pass "3.20 /api/channels returns $ch_count channels"
else
    fail "3.20 /api/channels" "count=$ch_count (expected >0)"
fi

echo

# =====================================================================
section "Section 6: Security"
# =====================================================================

# 6.1 Path traversal — literal ../ (--path-as-is prevents curl from normalizing)
# Server may return 200 with SPA fallback HTML — the real check is no passwd content.
trav_code=$(curl -s --path-as-is -o /dev/null -w "%{http_code}" "$WEB_URL/vc/../../../etc/passwd" 2>/dev/null || echo "000")
trav_body=$(curl -sS --path-as-is "$WEB_URL/vc/../../../etc/passwd" 2>/dev/null | head -c 200 || true)
if echo "$trav_body" | grep -qE '^(root|nobody|daemon):'; then
    fail "6.1 Path traversal" "passwd content leaked (HTTP $trav_code)"
else
    pass "6.1 Path traversal blocked (HTTP $trav_code, no /etc/passwd content)"
fi

# 6.1 URL-encoded traversal — server may return 200 with a safe body (SPA fallback).
# The real security check is that no /etc/passwd content leaks, regardless of status.
enc_code=$(curl -s -o /dev/null -w "%{http_code}" "$WEB_URL/vc/..%2F..%2F..%2Fetc%2Fpasswd" 2>/dev/null || echo "000")
enc_body=$(curl -sS "$WEB_URL/vc/..%2F..%2F..%2Fetc%2Fpasswd" 2>/dev/null || true)
if echo "$enc_body" | grep -qE '^(root|nobody|daemon):'; then
    fail "6.1 URL-encoded traversal" "passwd content leaked (HTTP $enc_code)"
else
    pass "6.1 URL-encoded traversal — no passwd leak (HTTP $enc_code, body safe)"
fi

# 6.2 Content-Type for /vc/
vc_ct=$(curl -sI "$WEB_URL/vc/" 2>/dev/null | awk 'tolower($1)=="content-type:" {sub(/\r$/,""); for(i=2;i<=NF;i++) printf "%s ", $i; print ""}' | head -1)
if echo "$vc_ct" | grep -qi 'text/html'; then
    pass "6.2 /vc/ Content-Type is text/html ($vc_ct)"
else
    fail "6.2 /vc/ Content-Type" "got: $vc_ct"
fi

# 6.3 Discover JS + CSS asset filenames from the HTML
vc_html=$(curl -sS "$WEB_URL/vc/" 2>/dev/null || true)
js_asset=$(echo "$vc_html" | grep -oE 'assets/index-[A-Za-z0-9_-]+\.js' | head -1)
css_asset=$(echo "$vc_html" | grep -oE 'assets/index-[A-Za-z0-9_-]+\.css' | head -1)

if [[ -n "$js_asset" ]]; then
    js_ct=$(curl -sI "$WEB_URL/vc/$js_asset" 2>/dev/null | awk 'tolower($1)=="content-type:" {sub(/\r$/,""); for(i=2;i<=NF;i++) printf "%s ", $i; print ""}' | head -1)
    if echo "$js_ct" | grep -qiE 'application/javascript|text/javascript'; then
        pass "6.3 JS asset MIME type ($js_asset → $js_ct)"
    else
        fail "6.3 JS asset MIME type" "$js_asset → $js_ct"
    fi
else
    fail "6.3 JS asset discovery" "no assets/index-*.js found in /vc/ HTML"
fi

if [[ -n "$css_asset" ]]; then
    css_ct=$(curl -sI "$WEB_URL/vc/$css_asset" 2>/dev/null | awk 'tolower($1)=="content-type:" {sub(/\r$/,""); for(i=2;i<=NF;i++) printf "%s ", $i; print ""}' | head -1)
    if echo "$css_ct" | grep -qi 'text/css'; then
        pass "6.3 CSS asset MIME type ($css_asset → $css_ct)"
    else
        fail "6.3 CSS asset MIME type" "$css_asset → $css_ct"
    fi
else
    fail "6.3 CSS asset discovery" "no assets/index-*.css found in /vc/ HTML"
fi

echo

# =====================================================================
section "Section 8.11: MCP beat string round-trip"
# =====================================================================

# Need a target function for the chaser step. Pick the first scene from query_functions.
target_fn=$(echo "$qfn_json" | jq -r '
    map(select(.type == "Scene")) | (.[0].name // empty)' 2>/dev/null || true)

if [[ -z "$target_fn" ]]; then
    target_fn=$(echo "$qfn_json" | jq -r '(.[0].name // empty)' 2>/dev/null || true)
fi

if [[ -z "$target_fn" ]]; then
    fail "8.11 MCP beat round-trip" "no functions found to use as chaser step"
else
    # The create_chasers schema declares fadeIn/hold/fadeOut as numbers.
    # When tempoType="beats", numeric values are interpreted as beat counts
    # (e.g. 0.1875 = 3/16, 0.25 = 1/4, 0.0625 = 1/16). String→number fraction
    # conversion is covered by mcp_conversions_test in the C++ test suite.
    beat_payload=$(jq -nc --arg cn "$CHASER_NAME" --arg fn "$target_fn" '{
        jsonrpc:"2.0", id:7, method:"tools/call",
        params:{name:"create_chasers", arguments:{items:[{
            name:$cn, tempoType:"beats",
            steps:[{functionName:$fn, fadeIn:0.1875, hold:0.25, fadeOut:0.0625}]
        }]}}}')
    beat_resp=$(mcp_call "$beat_payload")
    beat_json=$(mcp_result_json "$beat_resp")
    chaser_id=$(extract_first_id "$beat_json")
    inner_err=$(echo "$beat_json" | jq -r '.error // empty' 2>/dev/null || true)
    err=$(echo "$beat_resp" | jq -r '.error.message // empty')
    if [[ -n "$chaser_id" && -z "$err" && -z "$inner_err" ]]; then
        pass "8.11 MCP beat round-trip — chaser created (id=$chaser_id, step=$target_fn)"
        # Cleanup
        mcp_call "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"delete_functions\",\"arguments\":{\"ids\":[$chaser_id]}}}" >/dev/null
    else
        fail "8.11 MCP beat round-trip" "err='$err' inner='$inner_err' resp=$beat_resp"
    fi
fi

echo
echo "${BOLD}=== Results: ${GREEN}$PASS passed${RESET}${BOLD}, ${RED}$FAIL failed${RESET}${BOLD} ===${RESET}"

[[ "$FAIL" -eq 0 ]] || exit 1
