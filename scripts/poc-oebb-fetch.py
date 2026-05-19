#!/usr/bin/env python3
# Pre-Phase PoC für v2-S-Bahn-Migration — siehe docs/v2-sbahn-migration-plan.md
# §Pre-Phase. Wegwerf-Skript: wird in P.6 nach Übernahme der Fixtures (Schritt 3.4)
# wieder entfernt. Stdlib-only, kein venv/pip.
#
# Usage:
#   scripts/poc-oebb-fetch.py > .tmp/poc-oebb/morning.json 2> .tmp/poc-oebb/morning.summary
#   scripts/poc-oebb-fetch.py --aid OTHER --products 24 --max-jny 10

import argparse
import json
import sys
import urllib.error
import urllib.request

MGATE_URL = "https://fahrplan.oebb.at/bin/mgate.exe"
EVA_ATZGERSDORF = "8100634"
EVA_WIEN_HBF = "8100002"

DEFAULT_AID = "OWDL4fE4ixNiPBBm"   # CONCEPT §v2-4 Default; in §0.1 ggf. ersetzen
DEFAULT_PRODUCTS = "63"            # CONCEPT §v2-4 Default; Bits 0-5 (= S-Bahn + REX + Regio + IC/RJ/...)
DEFAULT_MAX_JNY = 6


def build_request(aid: str, products: str, max_jny: int) -> dict:
    return {
        "id": "bustaferl",
        "ver": "1.67",
        "lang": "deu",
        "auth": {"type": "AID", "aid": aid},
        "client": {"id": "OEBB", "type": "WEB", "name": "webapp", "l": "vs_webapp"},
        "formatted": False,
        "svcReqL": [{
            "meth": "StationBoard",
            "req": {
                "type": "DEP",
                "stbLoc": {"type": "S", "extId": EVA_ATZGERSDORF},
                "dirLoc": {"type": "S", "extId": EVA_WIEN_HBF},
                "maxJny": max_jny,
                "jnyFltrL": [{"type": "PROD", "mode": "INC", "value": products}],
            },
        }],
    }


def _resolve_line_name(jny: dict, common_prod: list) -> str:
    # HAFAS-Profile differieren: manche schicken jny.prodX direkt, andere jny.prodL[0].prodX.
    idx = jny.get("prodX")
    if idx is None:
        prod_l = jny.get("prodL") or []
        if prod_l and isinstance(prod_l[0], dict):
            idx = prod_l[0].get("prodX")
    if idx is None or not (0 <= idx < len(common_prod)):
        return "?"
    prod = common_prod[idx]
    return prod.get("name") or prod.get("nameS") or "?"


def summarize(response: dict, http_status: int, response_bytes: int) -> None:
    err = response.get("err", "?")
    svc_list = response.get("svcResL") or []
    jny_list = []
    common_prod = []
    if svc_list and isinstance(svc_list[0], dict):
        res = svc_list[0].get("res") or {}
        jny_list = res.get("jnyL") or []
        common_prod = (res.get("common") or {}).get("prodL") or []

    print(f"HTTP {http_status} ({response_bytes} bytes)", file=sys.stderr)
    print(f"err: {err}", file=sys.stderr)
    print(f"jnyL count: {len(jny_list)}", file=sys.stderr)
    print("first 3 departures:", file=sys.stderr)
    for jny in jny_list[:3]:
        stb = jny.get("stbStop") or {}
        cancelled = stb.get("dCncl", False)
        plan_time = stb.get("dTimeS", "?")
        real_time = stb.get("dTimeR", "")
        line_name = _resolve_line_name(jny, common_prod)
        rt_str = f" → {real_time}" if real_time and real_time != plan_time else ""
        cancel_str = " [CANCELLED]" if cancelled else ""
        print(f"  {line_name:<6} plan={plan_time}{rt_str}{cancel_str}", file=sys.stderr)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="PoC: ÖBB HAFAS StationBoard fetch Atzgersdorf → Wien Hbf"
    )
    parser.add_argument("--aid", default=DEFAULT_AID)
    parser.add_argument("--products", default=DEFAULT_PRODUCTS)
    parser.add_argument("--max-jny", type=int, default=DEFAULT_MAX_JNY)
    args = parser.parse_args()

    body = json.dumps(build_request(args.aid, args.products, args.max_jny)).encode("utf-8")
    req = urllib.request.Request(
        MGATE_URL,
        data=body,
        method="POST",
        headers={"Content-Type": "application/json; charset=UTF-8"},
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            payload = resp.read()
            status = resp.status
    except urllib.error.HTTPError as exc:
        payload = exc.read()
        status = exc.code
    except (urllib.error.URLError, TimeoutError) as exc:
        print(f"transport error: {exc}", file=sys.stderr)
        return 2

    try:
        parsed = json.loads(payload)
    except json.JSONDecodeError as exc:
        print(f"json decode error: {exc}", file=sys.stderr)
        print(payload.decode("utf-8", errors="replace"))
        return 3

    print(json.dumps(parsed, indent=2, ensure_ascii=False))
    summarize(parsed, status, len(payload))
    return 0


if __name__ == "__main__":
    sys.exit(main())
