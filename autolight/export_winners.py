#!/usr/bin/env python3
"""Export selected EXP- functions as permanent AutoLight presets."""
import argparse, asyncio, json
from datetime import datetime, timezone
from pathlib import Path
from autolight.qlc_client import QLC

PRESETS = Path(__file__).parent.parent / "autolight-presets.json"

def clean(name):
    return name[4:].strip(" -_") if name.startswith("EXP-") else name

def choose(exp, keep):
    if keep:
        wanted = {x.strip() for x in keep.split(",") if x.strip()}
        return [f for f in exp if str(f["id"]) in wanted or f["name"] in wanted]
    print("EXP- functions:")
    for f in exp: print(f'  {f["id"]}: {f["name"]} ({f["type"]})')
    wanted = input("Keep ids/names (comma-separated, blank=all): ").strip()
    return exp if not wanted else choose(exp, wanted)

async def clone(q, f):
    name, typ = clean(f["name"]), f.get("type")
    try:
        if typ in ("RGBMatrix", "RGB Matrix"):
            item = {k: f[k] for k in ("fixtureGroupID","algorithm","colors","duration","fadeIn","fadeOut",
                    "tempoType","controlMode","blendMode","runOrder","direction","rotation",
                    "mirror","mirrorBlend","properties") if k in f}
            res = await q.call("create_rgb_matrices", items=[dict(item, name=name, path=f.get("path", "AutoLight/Presets"))])
        elif typ == "Collection":
            res = await q.call("create_collections", items=[{"name": name, "functionIDs": f.get("functionIDs", []), "path": f.get("path", "AutoLight/Presets")}])
        else:
            print(f"  ⚠ Skipping unsupported type '{typ}' for: {f['name']}")
            return None
        if not res or (isinstance(res, list) and len(res) == 0):
            print(f"  ⚠ Failed to create preset for: {f['name']}")
            return None
        first = res[0] if isinstance(res, list) else res
        if isinstance(first, dict) and "error" in first:
            print(f"  ⚠ Error creating preset for {f['name']}: {first['error']}")
            return None
        # Only delete original after verified successful clone
        await q.call("delete_functions", ids=[f["id"]])
        return first | {"recipe": f, "exportedAt": datetime.now(timezone.utc).isoformat()}
    except Exception as e:
        print(f"  ⚠ Error exporting {f['name']}: {e}")
        return None

async def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--keep", help="Comma-separated EXP- ids or exact names to keep; prompts when omitted")
    args = ap.parse_args()
    async with QLC() as q:
        funcs = await q.call("query_functions")
        selected = choose([f for f in funcs if f.get("name", "").startswith("EXP-")], args.keep)
        presets = json.loads(PRESETS.read_text()) if PRESETS.exists() else []
        for f in selected:
            result = await clone(q, f)
            if result:
                presets.append(result)
        PRESETS.write_text(json.dumps(presets, indent=2) + "\n")
        print(f"Exported {len(selected)} preset(s) to {PRESETS}")

if __name__ == "__main__":
    asyncio.run(main())
