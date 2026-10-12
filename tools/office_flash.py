"""Local USB helper for ESP32-WROOM-32. Never uses Wi-Fi or HTTP OTA.

Requires Python 3 and esptool==4.12.0. Serial commands reset the inline bridge;
the operator must explicitly acknowledge that interruption before running.
"""
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import subprocess
import sys

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def run(port, *args):
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32", "--port", port,
           "--baud", "115200", *map(str, args)]
    subprocess.run(cmd, check=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--ack-bridge-interruption", action="store_true")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--backup", action="store_true")
    mode.add_argument("--flash-factory", type=Path)
    mode.add_argument("--restore", type=Path)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("BUILD-MANIFEST.json"))
    parser.add_argument("--output", type=Path, default=Path("flash-backups"))
    args = parser.parse_args()
    if not args.ack_bridge_interruption:
        parser.error("USB resets the bridge. Verify the boiler/display are ready for an interruption, then add --ack-bridge-interruption.")
    try:
        import esptool
    except ImportError:
        parser.error("Install esptool first: python -m pip install esptool==4.12.0")
    if getattr(esptool, "__version__", "") != "4.12.0":
        parser.error("This helper is prepared for esptool==4.12.0; install that exact version.")
    if args.flash_factory:
        manifest = json.loads(args.manifest.read_text())
        if manifest.get("version") != "2026.10.12.6":
            parser.error("Wrong manifest version")
        artifact = manifest["artifacts"]["factory"]
        if sha256(args.flash_factory) != artifact["sha256"]:
            parser.error("Factory image SHA-256 does not match the manifest")
    if args.restore:
        record_path = args.restore.with_suffix(".json")
        record = json.loads(record_path.read_text())
        if record.get("chip") != "esp32" or sha256(args.restore) != record["sha256"]:
            parser.error("Backup metadata/chip/hash mismatch")
    # Explicit --chip rejects S2/C3/S3 before any write. Read operations still
    # reset the device; they cannot promise uninterrupted UART service.
    run(args.port, "chip_id")
    run(args.port, "flash_id")
    args.output.mkdir(parents=True, exist_ok=True)
    stem = "esp32-before-write-" + dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    backup = args.output / (stem + ".bin")
    run(args.port, "read_flash", "0", "ALL", backup)
    if backup.stat().st_size < 4 * 1024 * 1024:
        parser.error("Expected at least 4 MB flash; target/layout must be rechecked")
    record = {"chip": "esp32", "bytes": backup.stat().st_size, "sha256": sha256(backup),
              "created": dt.datetime.now().isoformat(), "status": "EXACT_BACKUP_OF_UNKNOWN_INSTALLED_STATE"}
    backup.with_suffix(".json").write_text(json.dumps(record, indent=2))
    print("Full flash backup:", backup, "SHA-256:", record["sha256"])
    image = args.flash_factory or args.restore
    if image:
        run(args.port, "write_flash", "0", image)
        run(args.port, "verify_flash", "0", image)
        print("USB write and flash-byte verification complete. Screen/UART/network/HA remain to be verified physically.")

if __name__ == "__main__":
    main()
