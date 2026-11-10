#!/usr/bin/env python3
"""Parse the CSV files produced by tools/benchmark_gpu.sh.

usage: tools/benchmark_parse.py [OUTDIR]

The values are KWin's whole-compositor render-time query (render end - start),
in microseconds, after the first 2 seconds of each run. The table shows the
median of the per-run medians, the run-to-run range, and the median p95.
"""
import csv
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def config_key(name):
    return (0, 0) if name == "exact" else (1, int(name))


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/genie-benchmark")
    runs = defaultdict(list)
    for path in sorted(out.glob("*/*.csv")):
        m = re.match(r"(.+)_p([0-9.]+)(?:_r(\d+))?$", path.parent.name)
        if not m:
            continue
        cfg, p = m.group(1), float(m.group(2))
        data = []
        with path.open() as f:
            for row in csv.DictReader(f):
                try:
                    data.append({k: int(v) for k, v in row.items()})
                except ValueError:
                    pass
        if not data:
            continue
        t0 = data[0]["pageflip timestamp"]
        steady = [r for r in data if r["pageflip timestamp"] - t0 >= 2_000_000_000]
        if not steady:
            steady = data
        us = [(r["render end"] - r["render start"]) / 1000.0 for r in steady]
        intervals = [
            (data[i + 1]["pageflip timestamp"] - data[i]["pageflip timestamp"]) / 1e6
            for i in range(len(data) - 1)
        ]
        runs[(cfg, p)].append(
            {
                "median": statistics.median(us),
                "p95": sorted(us)[int(len(us) * 0.95)],
                "frames": len(steady),
                "interval": statistics.median(intervals),
            }
        )

    if not runs:
        print(f"no CSV files under {out}", file=sys.stderr)
        return 1

    for p in sorted({p for _, p in runs}):
        print(f"p={p}")
        print(f'{"cfg":>6s} {"runs":>4s} {"med us":>8s} {"range us":>16s} {"p95 us":>8s} {"frames":>6s} {"int ms":>7s}')
        for cfg in sorted({c for c, cp in runs if cp == p}, key=config_key):
            rs = runs[(cfg, p)]
            med = [r["median"] for r in rs]
            print(
                f'{cfg:>6s} {len(rs):4d} {statistics.median(med):8.1f} '
                f'{min(med):7.0f}-{max(med):<8.0f} '
                f'{statistics.median([r["p95"] for r in rs]):8.1f} '
                f'{int(statistics.median([r["frames"] for r in rs])):6d} '
                f'{statistics.median([r["interval"] for r in rs]):7.2f}'
            )
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
