# pyright: reportMissingImports=false
"""
Post-build script for RP2040 firmware.

How flasher-portal copy works:
- IDRYER_FLASHER_PORTAL_PATH must point to the flasher-portal directory itself.
  Example: /Users/ruslanpavlucenko/Projects/iDryerPortal/flasher-portal
- Local firmware/ is updated on every build.
- flasher-portal is updated only from main/master when the current git HEAD is
  exactly on tag vX.X.X and the tag matches firmware VERSION_STR.

flasher-portal layout:
  firmware/rp2040/<sht31_addr>/vX.X.X/firmware.uf2
  firmware/versions.json
"""

import json
import os
import re
import shutil
import subprocess
from SCons.Script import DefaultEnvironment  # type: ignore

GREEN = "\033[92m"
BLUE = "\033[94m"
YELLOW = "\033[93m"
RESET = "\033[0m"

env = DefaultEnvironment()
proj_dir = env.subst("$PROJECT_DIR")
build_dir = env.subst("$BUILD_DIR")
ver_hdr = os.path.join(proj_dir, "src", "version.h")
out_dir = os.path.join(proj_dir, "firmware")


def _idryer_flasher_portal_root(scons_env):
    raw = os.environ.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        env_dict = scons_env.get("ENV")
        if isinstance(env_dict, dict):
            raw = env_dict.get("IDRYER_FLASHER_PORTAL_PATH")
    if not raw:
        return None
    return str(raw).strip().strip('"').strip("'")


def _portal_path_hint(portal_root: str):
    if "\u2026" in portal_root or "\u22ef" in portal_root:
        return "path contains Unicode ellipsis; use the real full path"
    if "/.../" in portal_root or portal_root.endswith("/...") or portal_root.startswith(".../"):
        return "path contains literal ...; use the real full path"
    if os.path.basename(portal_root.rstrip(os.sep)) != "flasher-portal":
        return "IDRYER_FLASHER_PORTAL_PATH must point to flasher-portal, not iDryerPortal"
    return None


def _run_git(args):
    try:
        return subprocess.check_output(["git", *args], cwd=proj_dir, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return None


def _current_branch() -> str:
    return _run_git(["rev-parse", "--abbrev-ref", "HEAD"]) or "unknown"


def _release_tag_for_version(version: str):
    tags_raw = _run_git(["tag", "--points-at", "HEAD"]) or ""
    tags = [line.strip() for line in tags_raw.splitlines() if line.strip()]
    expected = f"v{version}"
    if expected in tags and re.fullmatch(r"v\d+\.\d+\.\d+", expected):
        return expected
    return None


def extract_version_from_header(path: str) -> str:
    try:
        txt = open(path, "r", encoding="utf-8").read()
    except Exception:
        return "0.0.0"
    m = re.search(r'#\s*define\s+VERSION_STR\s+"([^"]+)"', txt)
    if m:
        return m.group(1).strip()
    major = re.search(r"#\s*define\s+VERSION_MAJOR\s+(\d+)", txt)
    minor = re.search(r"#\s*define\s+VERSION_MINOR\s+(\d+)", txt)
    patch = re.search(r"#\s*define\s+VERSION_PATCH\s+(\d+)", txt)
    return f"{major.group(1)}.{minor.group(1)}.{patch.group(1)}" if major and minor and patch else "0.0.0"


def get_sht31_address() -> str:
    build_flags = env.get("BUILD_FLAGS", [])
    for flag in build_flags:
        match = re.search(r"-DSHT31_ADDRESS=(0x[0-9a-fA-F]+)", str(flag))
        if match:
            return match.group(1)
    return ""


def _load_versions(path: str) -> dict:
    if not os.path.exists(path):
        return {"schema": 1, "links": {}, "controller": {}}
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except Exception:
        data = {}
    data.setdefault("schema", 1)
    data.setdefault("links", {})
    data.setdefault("controller", {})
    return data


def _write_json(path: str, data: dict):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(data, fh, ensure_ascii=False, indent=2)
        fh.write("\n")


def _update_versions_json(portal_root: str, sht31_addr: str, version: str, file_rel: str):
    versions_path = os.path.join(portal_root, "firmware", "versions.json")
    data = _load_versions(versions_path)
    controller = data.setdefault("controller", {})
    entries = [item for item in controller.get(sht31_addr, []) if item.get("version") != version]
    entries.insert(0, {
        "version": version,
        "protocolMajor": int(version.split(".")[0]),
        "file": file_rel,
        "recommended": True,
    })
    for item in entries[1:]:
        item["recommended"] = False
    controller[sht31_addr] = entries
    _write_json(versions_path, data)


def _save_copy(src_path: str, scons_env):
    version = extract_version_from_header(ver_hdr)
    sht31_addr = get_sht31_address()
    release_tag = _release_tag_for_version(version)
    branch = _current_branch()

    if sht31_addr:
        filename = f"firmware_{sht31_addr}_v{version}.uf2"
    else:
        filename = f"firmware_v{version}.uf2"

    os.makedirs(out_dir, exist_ok=True)
    local_dst = os.path.join(out_dir, filename)

    print(f"{BLUE}[RP2040] Copying {filename}...{RESET}")
    print(f"[RP2040] git branch: {branch}")
    print(f"[RP2040] release tag: {release_tag or 'none'}")

    shutil.copy2(src_path, local_dst)
    print(f"[RP2040] {filename} → {local_dst}")

    portal_root = _idryer_flasher_portal_root(scons_env)
    if not portal_root:
        print(f"{YELLOW}[RP2040] IDRYER_FLASHER_PORTAL_PATH is not set → flasher-portal skipped{RESET}")
        print(f"{GREEN}[RP2040] ✅ local firmware updated{RESET}")
        return
    if not os.path.isdir(portal_root):
        print(f"{YELLOW}[RP2040] WARNING: IDRYER_FLASHER_PORTAL_PATH is not a directory: {portal_root!r}{RESET}")
        hint = _portal_path_hint(portal_root)
        if hint:
            print(f"{YELLOW}[RP2040] Hint: {hint}{RESET}")
        print(f"{GREEN}[RP2040] ✅ local firmware updated{RESET}")
        return
    hint = _portal_path_hint(portal_root)
    if hint:
        print(f"{YELLOW}[RP2040] Hint: {hint}{RESET}")
        print(f"{GREEN}[RP2040] ✅ local firmware updated{RESET}")
        return
    if branch not in ("main", "master"):
        print(
            f"{YELLOW}[RP2040] skip flasher-portal: release copy requires main/master, "
            f"current branch is {branch}{RESET}"
        )
        print(f"{GREEN}[RP2040] ✅ local firmware updated{RESET}")
        return
    if not release_tag:
        print(
            f"{YELLOW}[RP2040] skip flasher-portal: release copy requires exact tag v{version} "
            f"on current HEAD{RESET}"
        )
        print(f"{GREEN}[RP2040] ✅ local firmware updated{RESET}")
        return

    addr_dir = sht31_addr or "default"
    file_rel = f"firmware/rp2040/{addr_dir}/v{version}/firmware.uf2"
    flasher_dst = os.path.join(portal_root, file_rel)
    os.makedirs(os.path.dirname(flasher_dst), exist_ok=True)
    shutil.copy2(src_path, flasher_dst)
    _update_versions_json(portal_root, addr_dir, version, file_rel)
    print(f"[RP2040] firmware.uf2 → {flasher_dst}")
    print(f"[RP2040] versions → {os.path.join(portal_root, 'firmware', 'versions.json')}")
    print(f"{GREEN}[RP2040] ✅ local + flasher-portal firmware updated{RESET}")


def _copy_bin_to_flasher(scons_env):
    """Кладёт сырой firmware.bin рядом с firmware.uf2 в flasher-portal — для
    прокси-OTA на RP2040 (PicoOTA применяет .bin, не .uf2). Отдельный
    post-action на таргете firmware.bin, т.к. .bin собирается ПОСЛЕ .elf/.uf2.

    Те же условия, что и для .uf2: только main/master + точный тег vX.X.X.
    versions.json и .uf2 не трогаем — .uf2 остаётся для веб-флешера (USB).
    """
    bin_src = os.path.join(build_dir, "firmware.bin")
    if not os.path.isfile(bin_src):
        print(f"{YELLOW}[RP2040] firmware.bin not found at {bin_src} → OTA .bin skipped{RESET}")
        return

    version = extract_version_from_header(ver_hdr)
    sht31_addr = get_sht31_address()
    portal_root = _idryer_flasher_portal_root(scons_env)
    if not portal_root or not os.path.isdir(portal_root):
        return
    if _portal_path_hint(portal_root):
        return
    if _current_branch() not in ("main", "master"):
        return
    if not _release_tag_for_version(version):
        return

    addr_dir = sht31_addr or "default"
    bin_dst = os.path.join(portal_root, "firmware", "rp2040", addr_dir, f"v{version}", "firmware.bin")
    os.makedirs(os.path.dirname(bin_dst), exist_ok=True)
    shutil.copy2(bin_src, bin_dst)
    print(f"{GREEN}[RP2040] firmware.bin (OTA) → {bin_dst}{RESET}")


def post_action(target, source, env):
    built = getattr(target[0], "get_abspath", lambda: str(target[0]))()
    root, ext = os.path.splitext(built)
    uf2 = root + ".uf2" if ext.lower() != ".uf2" else built
    if not os.path.isfile(uf2):
        uf2 = os.path.join(build_dir, "firmware.uf2")
    print(f"[post_build] Built target: {built}")
    print(f"[post_build] Probing UF2:  {uf2}")
    if os.path.isfile(uf2):
        _save_copy(uf2, env)
    else:
        print("[post_build] UF2 not found; skipping.")


env.AddPostAction("${BUILD_DIR}/${PROGNAME}.elf", post_action)
env.AddPostAction("${BUILD_DIR}/firmware.uf2", post_action)
# .bin собирается отдельным таргетом ПОСЛЕ .elf/.uf2 — копируем его своим
# post-action, иначе в момент post_action .bin ещё не существует.
env.AddPostAction("${BUILD_DIR}/firmware.bin", lambda target, source, env: _copy_bin_to_flasher(env))
