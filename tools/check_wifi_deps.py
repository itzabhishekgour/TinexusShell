#!/usr/bin/env python3
import subprocess

kver = "7.0.0-28-generic"
modules = ["rfkill", "libarc4", "cfg80211", "mac80211", "mt76", "mt76-connac-lib", "mt792x-lib", "mt7921-common", "mt7921e"]

seen = set()
to_check = list(modules)

print(f"Checking dependencies for kernel {kver}:")
for m in to_check:
    res = subprocess.run(["modinfo", "-k", kver, "-F", "depends", m], capture_output=True, text=True)
    deps = res.stdout.strip()
    fname = subprocess.run(["modinfo", "-k", kver, "-F", "filename", m], capture_output=True, text=True).stdout.strip()
    print(f"{m:16} -> depends: [{deps}] | file: {fname}")
    if deps:
        for d in deps.split(","):
            d = d.strip()
            if d and d not in to_check:
                to_check.append(d)

print("\nAll required modules:")
print(to_check)
