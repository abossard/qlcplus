#!/bin/bash
# MCP V4 HTTP Smoke Test
# Exercises all MCP tools against a running QLC+ v4 instance.
# Usage: ./smoke_test_v4.sh [port]
#
# Prerequisites: qlcplus v4 binary built with -Dmcp_server=ON
# The script starts the app, runs tests, then kills it.

set -euo pipefail

PORT="${1:-19696}"
MCP_URL="http://127.0.0.1:${PORT}/mcp"
QLCPLUS_BIN="${QLCPLUS_BIN:-$(dirname "$0")/../../build/main/qlcplus}"
PASS=0
FAIL=0
SKIP=0
RESULTS=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

log_pass() { PASS=$((PASS+1)); RESULTS="${RESULTS}\n${GREEN}PASS${NC} $1"; }
log_fail() { FAIL=$((FAIL+1)); RESULTS="${RESULTS}\n${RED}FAIL${NC} $1: $2"; }
log_skip() { SKIP=$((SKIP+1)); RESULTS="${RESULTS}\n${YELLOW}SKIP${NC} $1: $2"; }

# Send a JSON-RPC tools/call request and return the result
call_tool() {
    local tool_name="$1"
    local args="$2"
    local payload="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"${tool_name}\",\"arguments\":${args}}}"
    curl -s -X POST "${MCP_URL}" \
        -H "Content-Type: application/json" \
        -d "${payload}" 2>/dev/null
}

# Check if response contains an error
has_error() {
    echo "$1" | python3 -c "
import sys, json
data = json.load(sys.stdin)
result = data.get('result', {})
content = result.get('content', [{}])
text = content[0].get('text', '') if content else ''
try:
    parsed = json.loads(text)
    if isinstance(parsed, dict) and 'error' in parsed:
        sys.exit(0)  # has error
    if isinstance(parsed, list) and len(parsed) > 0 and 'error' in parsed[0]:
        sys.exit(0)
except:
    pass
sys.exit(1)  # no error
" 2>/dev/null
}

# Extract text content from response
get_text() {
    echo "$1" | python3 -c "
import sys, json
data = json.load(sys.stdin)
result = data.get('result', {})
content = result.get('content', [{}])
text = content[0].get('text', '') if content else ''
print(text)
" 2>/dev/null
}

# Check response is valid (not empty, has result, no JSON-RPC error)
check_ok() {
    local test_name="$1"
    local response="$2"
    local extra_check="${3:-}"

    if [ -z "$response" ]; then
        log_fail "$test_name" "empty response"
        return 1
    fi

    # Check for JSON-RPC level error
    local rpc_error
    rpc_error=$(echo "$response" | python3 -c "
import sys, json
data = json.load(sys.stdin)
if 'error' in data:
    print(data['error'].get('message', 'unknown'))
" 2>/dev/null || true)

    if [ -n "$rpc_error" ]; then
        log_fail "$test_name" "RPC error: $rpc_error"
        return 1
    fi

    # Check for tool-level error
    if has_error "$response"; then
        local text
        text=$(get_text "$response")
        log_fail "$test_name" "tool error: $(echo "$text" | head -c 200)"
        return 1
    fi

    log_pass "$test_name"
    return 0
}

echo "=== MCP V4 HTTP Smoke Test ==="
echo "Port: ${PORT}"
echo "Binary: ${QLCPLUS_BIN}"
echo ""

# --- Start QLC+ ---
if [ ! -f "$QLCPLUS_BIN" ]; then
    echo "ERROR: Binary not found at $QLCPLUS_BIN"
    echo "Build with: cmake .. -Dqmlui=OFF -Dmcp_server=ON && cmake --build . --target qlcplus -j8"
    exit 1
fi

echo "Starting QLC+ v4..."
"$QLCPLUS_BIN" --mcp-port "$PORT" -n -m &
APP_PID=$!
trap "kill $APP_PID 2>/dev/null || true; wait $APP_PID 2>/dev/null || true" EXIT

# Wait for MCP server to be ready
echo -n "Waiting for MCP server"
for i in $(seq 1 30); do
    if curl -s -o /dev/null -w "%{http_code}" "${MCP_URL}" 2>/dev/null | grep -q "200\|405\|400"; then
        echo " ready!"
        break
    fi
    echo -n "."
    sleep 1
done
echo ""

# --- Initialize MCP session ---
echo "--- Initializing MCP session ---"
INIT=$(curl -s -X POST "${MCP_URL}" \
    -H "Content-Type: application/json" \
    -d '{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"smoke-test","version":"1.0"}}}')
check_ok "initialize" "$INIT" || true

# --- Engine-only tools ---
echo ""
echo "--- Engine-only tools ---"

# patch_fixtures
R=$(call_tool "patch_fixtures" '{"fixtures":[{"name":"Dimmer 1","manufacturer":"Generic","model":"Generic","mode":"1 Channel","universe":0,"address":0,"quantity":4,"gap":0}]}')
check_ok "patch_fixtures (4 generic dimmers)" "$R" || true

# query_fixtures
R=$(call_tool "query_fixtures" '{}')
check_ok "query_fixtures" "$R" || true

# query_fixture_channels
R=$(call_tool "query_fixture_channels" '{"fixtureIDs":[0]}')
check_ok "query_fixture_channels" "$R" || true

# create_scenes
R=$(call_tool "create_scenes" '{"scenes":[{"name":"Full","channelValues":[{"fixtureID":0,"channel":0,"value":255}]},{"name":"Half","channelValues":[{"fixtureID":0,"channel":0,"value":128}]},{"name":"Off","channelValues":[{"fixtureID":0,"channel":0,"value":0}]}]}')
check_ok "create_scenes (3 scenes)" "$R" || true

# create_chasers
R=$(call_tool "create_chasers" '{"chasers":[{"name":"Dimmer Chase","steps":["Full","Half","Off"]}]}')
check_ok "create_chasers" "$R" || true

# create_collections
R=$(call_tool "create_collections" '{"collections":[{"name":"All On","functionNames":["Full"]}]}')
check_ok "create_collections" "$R" || true

# create_palettes
R=$(call_tool "create_palettes" '{"palettes":[{"name":"Red","type":"color","color":"#FF0000"}]}')
check_ok "create_palettes" "$R" || true

# query_functions
R=$(call_tool "query_functions" '{}')
check_ok "query_functions" "$R" || true

# query_universes
R=$(call_tool "query_universes" '{}')
check_ok "query_universes" "$R" || true

# configure_universes
R=$(call_tool "configure_universes" '{"universes":[{"id":0,"name":"Main Stage"}]}')
check_ok "configure_universes" "$R" || true

# delete_functions (create a throwaway then delete)
R=$(call_tool "create_scenes" '{"scenes":[{"name":"Throwaway","channelValues":[{"fixtureID":0,"channel":0,"value":1}]}]}')
THROWAWAY_ID=$(get_text "$R" | python3 -c "import sys,json; d=json.loads(sys.stdin.read()); print(d[0].get('id',''))" 2>/dev/null || echo "")
if [ -n "$THROWAWAY_ID" ]; then
    R=$(call_tool "delete_functions" "{\"ids\":[${THROWAWAY_ID}]}")
    check_ok "delete_functions" "$R" || true
else
    log_skip "delete_functions" "could not get throwaway ID"
fi

# --- VC tools ---
echo ""
echo "--- VC tools (VCBridgeV4) ---"

# vc_create_pages
R=$(call_tool "vc_create_pages" '{"pages":[{"name":"Main Page"}]}')
check_ok "vc_create_pages" "$R" || true

# vc_query_pages (basic)
R=$(call_tool "vc_query_pages" '{}')
check_ok "vc_query_pages" "$R" || true

# vc_create_widgets - button
R=$(call_tool "vc_create_widgets" '{"widgets":[{"type":"button","pageIndex":0,"geometry":{"x":10,"y":10,"w":100,"h":50},"caption":"Test Button"}]}')
check_ok "vc_create_widgets (button)" "$R" || true

# vc_create_widgets - slider
R=$(call_tool "vc_create_widgets" '{"widgets":[{"type":"slider","pageIndex":0,"geometry":{"x":120,"y":10,"w":60,"h":200},"caption":"Test Slider"}]}')
check_ok "vc_create_widgets (slider)" "$R" || true

# vc_create_widgets - frame
R=$(call_tool "vc_create_widgets" '{"widgets":[{"type":"frame","pageIndex":0,"geometry":{"x":200,"y":10,"w":300,"h":200},"caption":"Test Frame"}]}')
check_ok "vc_create_widgets (frame)" "$R" || true

# vc_create_widgets - label
R=$(call_tool "vc_create_widgets" '{"widgets":[{"type":"label","pageIndex":0,"geometry":{"x":10,"y":250,"w":150,"h":30},"caption":"Hello MCP"}]}')
check_ok "vc_create_widgets (label)" "$R" || true

# vc_create_widgets - cuelist
R=$(call_tool "vc_create_widgets" '{"widgets":[{"type":"cuelist","pageIndex":0,"geometry":{"x":10,"y":300,"w":250,"h":150},"caption":"Test CueList"}]}')
check_ok "vc_create_widgets (cuelist)" "$R" || true

# vc_query_widgets
R=$(call_tool "vc_query_widgets" '{}')
check_ok "vc_query_widgets" "$R" || true

# vc_update_widgets (change caption)
# Get widget ID from query
WIDGET_ID=$(get_text "$R" | python3 -c "
import sys,json
data = json.loads(sys.stdin.read())
if isinstance(data, list) and len(data) > 0:
    # Find a button widget
    for w in data:
        if w.get('type') == 'button':
            print(w.get('id', ''))
            break
" 2>/dev/null || echo "")
if [ -n "$WIDGET_ID" ]; then
    R=$(call_tool "vc_update_widgets" "{\"widgets\":[{\"id\":${WIDGET_ID},\"caption\":\"Updated Button\"}]}")
    check_ok "vc_update_widgets (caption)" "$R" || true
else
    log_skip "vc_update_widgets" "no widget ID found"
fi

# vc_detect_overlaps
R=$(call_tool "vc_detect_overlaps" '{"pageIndex":0}')
check_ok "vc_detect_overlaps" "$R" || true

# vc_delete_widgets (delete the label)
LABEL_ID=$(call_tool "vc_query_widgets" '{}' | python3 -c "
import sys,json
data = json.load(sys.stdin)
text = data.get('result',{}).get('content',[{}])[0].get('text','[]')
widgets = json.loads(text)
for w in widgets:
    if w.get('type') == 'label':
        print(w.get('id',''))
        break
" 2>/dev/null || echo "")
if [ -n "$LABEL_ID" ]; then
    R=$(call_tool "vc_delete_widgets" "{\"widgetIDs\":[${LABEL_ID}]}")
    check_ok "vc_delete_widgets" "$R" || true
else
    log_skip "vc_delete_widgets" "no label ID found"
fi

# --- Results ---
echo ""
echo "==============================="
echo "=== RESULTS ==="
echo "==============================="
echo -e "$RESULTS"
echo ""
echo "==============================="
echo -e "PASS: ${GREEN}${PASS}${NC}  FAIL: ${RED}${FAIL}${NC}  SKIP: ${YELLOW}${SKIP}${NC}"
echo "==============================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
