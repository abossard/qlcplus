#!/usr/bin/env bash
set -euo pipefail

MCP_URL="${MCP_URL:-http://127.0.0.1:9696/mcp}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HEADERS_FILE="$SCRIPT_DIR/.duration-invariant-headers.$$"
BODY_FILE="$SCRIPT_DIR/.duration-invariant-body.$$"
trap 'rm -f "$HEADERS_FILE" "$BODY_FILE"' EXIT

MCP_SESSION_ID=""
FADE_IN_MS=500
UPDATED_FADE_IN_MS=1000
HOLD_MS=1000
DURATION_MS=$((FADE_IN_MS + HOLD_MS))
UPDATED_DURATION_MS=$((UPDATED_FADE_IN_MS + HOLD_MS))
MCP_RGB_FADE_IN_MS=700
MCP_RGB_DURATION_MS=$((MCP_RGB_FADE_IN_MS + HOLD_MS))
FIXTURE_NAME="MCP_DURATION_INVARIANT_RGB"
GROUP_NAME="MCP_DURATION_INVARIANT_GROUP"
SCENE_NAME="MCP_DURATION_INVARIANT_SCENE"
SEQUENCE_NAME="MCP_DURATION_INVARIANT_SEQUENCE"
MATRIX_NAME="MCP_DURATION_INVARIANT_MATRIX"

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

command -v curl >/dev/null 2>&1 || fail "curl is required"
command -v jq >/dev/null 2>&1 || fail "jq is required"

mcp_init_session() {
    curl -sS -D "$HEADERS_FILE" -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -d '{"jsonrpc":"2.0","id":0,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"duration-invariant-test","version":"1.0"}}}' \
        -o "$BODY_FILE"

    MCP_SESSION_ID=$(awk 'tolower($1)=="mcp-session-id:" {sub(/\r$/,"",$2); print $2; exit}' "$HEADERS_FILE")
    jq -e '.result.serverInfo.name' "$BODY_FILE" >/dev/null || fail "MCP initialize failed: $(cat "$BODY_FILE")"
    [[ -n "$MCP_SESSION_ID" ]] || fail "MCP initialize did not return Mcp-Session-Id"
}

mcp_send_initialized() {
    curl -sS -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -H "Mcp-Session-Id: $MCP_SESSION_ID" \
        -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
        >/dev/null
}

mcp_call() {
    local payload="$1"
    curl -sS -X POST "$MCP_URL" \
        -H 'Content-Type: application/json' \
        -H "Mcp-Session-Id: $MCP_SESSION_ID" \
        -d "$payload"
}

mcp_tool() {
    local id="$1"
    local name="$2"
    local args="$3"
    local payload
    payload=$(jq -nc --argjson id "$id" --arg name "$name" --argjson args "$args" \
        '{jsonrpc:"2.0", id:$id, method:"tools/call", params:{name:$name, arguments:$args}}')
    mcp_call "$payload"
}

mcp_result_json() {
    jq -r '.result.content[0].text // empty' | jq '.'
}

code=$(curl -s -o /dev/null -w '%{http_code}' "$MCP_URL" || true)
[[ "$code" != "000" ]] || fail "MCP server is not reachable at $MCP_URL. Start QLC+ first."

mcp_init_session
mcp_send_initialized

fixture_resp=$(mcp_tool 1 patch_fixtures "$(jq -nc --arg name "$FIXTURE_NAME" '{items:[{manufacturer:"Generic", model:"Generic RGB", mode:"RGB", name:$name, universe:0, address:500, quantity:1}]}')")
fixture_json=$(echo "$fixture_resp" | mcp_result_json)
fixture_id=$(echo "$fixture_json" | jq -r '.[0].id // empty')
[[ -n "$fixture_id" ]] || fail "Could not patch fixture: $fixture_resp"

fixture_ids=$(jq -nc --argjson id "$fixture_id" '[$id]')
group_resp=$(mcp_tool 2 create_fixture_groups "$(jq -nc --arg name "$GROUP_NAME" --argjson fixtureIDs "$fixture_ids" '{items:[{name:$name, fixtureIDs:$fixtureIDs, columns:1, rows:1}]}')")
group_json=$(echo "$group_resp" | mcp_result_json)
group_id=$(echo "$group_json" | jq -r '.[0].id // empty')
[[ -n "$group_id" ]] || fail "Could not create fixture group: $group_resp"

scene_resp=$(mcp_tool 3 create_scenes "$(jq -nc --arg name "$SCENE_NAME" --argjson fixtureIDs "$fixture_ids" '{items:[{name:$name, fixtureIDs:$fixtureIDs, channelValues:[]}]}')")
scene_json=$(echo "$scene_resp" | mcp_result_json)
scene_id=$(echo "$scene_json" | jq -r '.[0].id // empty')
[[ -n "$scene_id" ]] || fail "Could not create scene: $scene_resp"

sequence_args=$(jq -nc \
    --arg name "$SEQUENCE_NAME" \
    --argjson sceneID "$scene_id" \
    --argjson fadeIn "$FADE_IN_MS" \
    --argjson holdTime "$HOLD_MS" \
    '{items:[{name:$name, boundSceneID:$sceneID, fadeIn:$fadeIn, holdTime:$holdTime, fadeOut:0, tempoType:"time"}]}')
sequence_resp=$(mcp_tool 4 create_sequences "$sequence_args")
sequence_json=$(echo "$sequence_resp" | mcp_result_json)
sequence_id=$(echo "$sequence_json" | jq -r '.[0].id // empty')
[[ -n "$sequence_id" ]] || fail "Could not create sequence: $sequence_resp"

query_resp=$(mcp_tool 5 query_functions '{}')
query_json=$(echo "$query_resp" | mcp_result_json)
sequence_hold=$(echo "$query_json" | jq -r --arg name "$SEQUENCE_NAME" '.[] | select(.name == $name and .type == "Sequence") | .hold' | tail -n 1)
sequence_duration=$(echo "$query_json" | jq -r --arg name "$SEQUENCE_NAME" '.[] | select(.name == $name and .type == "Sequence") | .duration' | tail -n 1)
[[ "$sequence_hold" == "$HOLD_MS" ]] || fail "query_functions sequence hold=$sequence_hold, expected $HOLD_MS"
[[ "$sequence_duration" == "$DURATION_MS" ]] || fail "query_functions sequence duration=$sequence_duration, expected $DURATION_MS"

matrix_args=$(jq -nc \
    --arg name "$MATRIX_NAME" \
    --argjson groupID "$group_id" \
    --argjson duration "$DURATION_MS" \
    --argjson fadeIn "$FADE_IN_MS" \
    '{items:[{name:$name, fixtureGroupID:$groupID, algorithm:"Plain Color", duration:$duration, fadeIn:$fadeIn, fadeOut:0, tempoType:"time"}]}')
matrix_resp=$(mcp_tool 6 create_rgb_matrices "$matrix_args")
matrix_json=$(echo "$matrix_resp" | mcp_result_json)
created_duration=$(echo "$matrix_json" | jq -r '.[0].duration // empty')
created_hold=$(echo "$matrix_json" | jq -r '.[0].hold // empty')
created_fade_in=$(echo "$matrix_json" | jq -r '.[0].fadeIn // empty')
[[ "$created_duration" == "$DURATION_MS" ]] || fail "create_rgb_matrices duration=$created_duration, expected $DURATION_MS"
[[ "$created_hold" == "$HOLD_MS" ]] || fail "create_rgb_matrices hold=$created_hold, expected $HOLD_MS"
[[ "$created_fade_in" == "$FADE_IN_MS" ]] || fail "create_rgb_matrices fadeIn=$created_fade_in, expected $FADE_IN_MS"

query_resp=$(mcp_tool 7 query_functions '{}')
query_json=$(echo "$query_resp" | mcp_result_json)
queried_duration=$(echo "$query_json" | jq -r --arg name "$MATRIX_NAME" '.[] | select(.name == $name and .type == "RGBMatrix") | .duration' | tail -n 1)
queried_hold=$(echo "$query_json" | jq -r --arg name "$MATRIX_NAME" '.[] | select(.name == $name and .type == "RGBMatrix") | .hold' | tail -n 1)
[[ "$queried_duration" == "$DURATION_MS" ]] || fail "query_functions duration=$queried_duration, expected $DURATION_MS"
[[ "$queried_hold" == "$HOLD_MS" ]] || fail "query_functions hold=$queried_hold, expected $HOLD_MS"

matrix_query_resp=$(mcp_tool 8 query_rgb_matrices "$(jq -nc --arg name "$MATRIX_NAME" '{name:$name}')")
matrix_query_json=$(echo "$matrix_query_resp" | mcp_result_json)
queried_matrix_duration=$(echo "$matrix_query_json" | jq -r '.[0].duration // empty')
queried_matrix_hold=$(echo "$matrix_query_json" | jq -r '.[0].hold // empty')
queried_matrix_fade_in=$(echo "$matrix_query_json" | jq -r '.[0].fadeIn // empty')
[[ "$queried_matrix_duration" == "$DURATION_MS" ]] || fail "query_rgb_matrices duration=$queried_matrix_duration, expected $DURATION_MS"
[[ "$queried_matrix_hold" == "$HOLD_MS" ]] || fail "query_rgb_matrices hold=$queried_matrix_hold, expected $HOLD_MS"
[[ "$queried_matrix_fade_in" == "$FADE_IN_MS" ]] || fail "query_rgb_matrices fadeIn=$queried_matrix_fade_in, expected $FADE_IN_MS"

actual_hold=$((queried_matrix_duration - queried_matrix_fade_in))
[[ "$actual_hold" == "$HOLD_MS" ]] || fail "hold invariant failed: duration($queried_matrix_duration) - fadeIn($queried_matrix_fade_in) = $actual_hold, expected $HOLD_MS"

matrix_fade_only_args=$(jq -nc \
    --arg name "$MATRIX_NAME" \
    --argjson groupID "$group_id" \
    --argjson fadeIn "$MCP_RGB_FADE_IN_MS" \
    '{items:[{name:$name, fixtureGroupID:$groupID, algorithm:"Plain Color", fadeIn:$fadeIn, fadeOut:0, tempoType:"time"}]}')
matrix_fade_only_resp=$(mcp_tool 9 create_rgb_matrices "$matrix_fade_only_args")
matrix_fade_only_json=$(echo "$matrix_fade_only_resp" | mcp_result_json)
fade_only_duration=$(echo "$matrix_fade_only_json" | jq -r '.[0].duration // empty')
fade_only_hold=$(echo "$matrix_fade_only_json" | jq -r '.[0].hold // empty')
fade_only_fade_in=$(echo "$matrix_fade_only_json" | jq -r '.[0].fadeIn // empty')
[[ "$fade_only_fade_in" == "$MCP_RGB_FADE_IN_MS" ]] || fail "fade-only RGB update fadeIn=$fade_only_fade_in, expected $MCP_RGB_FADE_IN_MS"
[[ "$fade_only_hold" == "$HOLD_MS" ]] || fail "fade-only RGB update hold=$fade_only_hold, expected $HOLD_MS"
[[ "$fade_only_duration" == "$MCP_RGB_DURATION_MS" ]] || fail "fade-only RGB update duration=$fade_only_duration, expected $MCP_RGB_DURATION_MS"

cat <<EOF
PASS: MCP duration invariant holds for sequence and RGB matrix timing.

MANUAL EDITOR INVARIANT TEST REQUIRED:
MCP can create and query RGB Matrices, but it cannot exercise FunctionEditor::setFadeInSpeed()
directly without going through future MCP speed-update tooling. To verify Phase 3 in the
running QLC+ UI:
1. Open RGB Matrix "$MATRIX_NAME".
2. Confirm fadeIn=$MCP_RGB_FADE_IN_MS, duration=$MCP_RGB_DURATION_MS, hold=$HOLD_MS.
3. Change only fadeIn to $UPDATED_FADE_IN_MS in the editor.
4. Query with:
   query_rgb_matrices {"name":"$MATRIX_NAME"}
5. Expected: fadeIn=$UPDATED_FADE_IN_MS, duration=$UPDATED_DURATION_MS, hold=$HOLD_MS.
EOF
