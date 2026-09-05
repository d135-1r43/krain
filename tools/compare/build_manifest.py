#!/usr/bin/env python3
"""Builds manifest.json for the compare app by scanning a renders directory.

Pairing happens here rather than in C++ on purpose: the Catch2 suite runs its cases
in parallel processes, so no producer can safely own a shared index file. Each
producer writes its own WAVs plus a JSON sidecar, and this scans the result.
"""

import json
import struct
import sys
from datetime import datetime, timezone
from pathlib import Path


def read_wav_format(path: Path) -> dict:
    """Pulls sample rate and duration out of the WAV header.

    Worth doing here rather than in the page: decodeAudioData resamples to the
    AudioContext rate, so by the time the browser has the audio, the file's own
    rate is gone.
    """
    try:
        with path.open("rb") as handle:
            if handle.read(4) != b"RIFF":
                return {}
            handle.seek(8)
            if handle.read(4) != b"WAVE":
                return {}

            sample_rate = channels = bits = None
            while chunk := handle.read(8):
                if len(chunk) < 8:
                    break
                tag, size = chunk[:4], struct.unpack("<I", chunk[4:])[0]
                if tag == b"fmt ":
                    fmt = handle.read(size)
                    channels, sample_rate = struct.unpack("<HI", fmt[2:8])
                    bits = struct.unpack("<H", fmt[14:16])[0]
                elif tag == b"data" and sample_rate:
                    frames = size // max(1, channels * bits // 8)
                    return {"sampleRate": sample_rate, "channels": channels,
                            "seconds": round(frames / sample_rate, 3)}
                else:
                    handle.seek(size + (size % 2), 1)
    except (OSError, struct.error):
        pass
    return {}


def build(directory: Path) -> dict:
    items = []

    for processed in sorted(directory.glob("*--processed.wav")):
        item_id = processed.name[: -len("--processed.wav")]
        source = directory / f"{item_id}--source.wav"

        if not source.exists():
            print(f"  skipping {item_id}: no matching source", file=sys.stderr)
            continue

        item = {
            "id": item_id,
            "source": source.name,
            "processed": processed.name,
            "preset": item_id,
            "origin": "render",
            "bytes": processed.stat().st_size,
        }
        item.update(read_wav_format(processed))

        sidecar = directory / f"{item_id}.json"
        if sidecar.exists():
            try:
                meta = json.loads(sidecar.read_text())
            except json.JSONDecodeError as error:
                print(f"  {sidecar.name} is not valid JSON: {error}", file=sys.stderr)
            else:
                item["preset"] = meta.get("preset", item_id)
                item["presetDescription"] = meta.get("presetDescription", "")
                item["sourceName"] = meta.get("source", "")
                item["sourceDescription"] = meta.get("sourceDescription", "")
                item["origin"] = meta.get("origin", "render")
                if "params" in meta:
                    item["params"] = meta["params"]

        items.append(item)

    # Renders first, test captures after: the presets are what you iterate on.
    items.sort(key=lambda i: (i["origin"] != "render", i["id"]))

    return {"generated": datetime.now(timezone.utc).isoformat(timespec="seconds"), "items": items}


def main() -> int:
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()

    if not directory.is_dir():
        print(f"Not a directory: {directory}", file=sys.stderr)
        return 1

    manifest = build(directory)
    (directory / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    print(f"{len(manifest['items'])} comparison(s) indexed in {directory}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
