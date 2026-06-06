import json
import os
import time

import ida_auto
import ida_bytes
import ida_funcs
import ida_ida
import ida_kernwin
import ida_name
import ida_nalt
import ida_segment
import idaapi
import idautils
import idc


IDA_BASE = 0x140000000
TARGETS = {
    "ccam_matrix_helper_320": 0x1406BE3A0,
    "forzamulticam_matrix_getter_650": 0x1406B0C20,
    "forzamulticam_vector_getter_600": 0x1406B0860,
    "forzamulticam_select_wrapper": 0x1406B5ED0,
    "forzamulticam_bridge_copy_660": 0x140746BB0,
}

INFERRED_HEAD_OFFSETS = {
    "ccam_matrix_helper_320": [0x78],
    "forzamulticam_matrix_getter_650": [0x60],
    "forzamulticam_vector_getter_600": [0x68],
    "forzamulticam_select_wrapper": [0x10],
}


def hx(value):
    if value is None or value == idaapi.BADADDR:
        return None
    return "0x%X" % int(value)


def seg_name(ea):
    seg = ida_segment.getseg(ea)
    return ida_segment.get_segm_name(seg) if seg else None


def func_name(ea):
    name = ida_name.get_name(ea)
    if name:
        return name
    func = ida_funcs.get_func(ea)
    if func:
        return ida_name.get_name(func.start_ea) or hx(func.start_ea)
    return None


def qword(ea):
    try:
        value = ida_bytes.get_qword(ea)
    except Exception:
        return None
    if value == idaapi.BADADDR:
        return None
    return value


def walk_qwords():
    for i in range(ida_segment.get_segm_qty()):
        seg = ida_segment.getnseg(i)
        name = ida_segment.get_segm_name(seg) or ""
        if name not in (".rdata", ".data"):
            continue
        ea = seg.start_ea
        end = seg.end_ea
        while ea + 8 <= end:
            value = qword(ea)
            if value is not None:
                yield ea, value, name
            ea += 8


def table_slots(head, count=96):
    slots = []
    for i in range(count):
        entry = head + i * 8
        target = qword(entry)
        slots.append({
            "slot": i,
            "entry": hx(entry),
            "target": hx(target),
            "target_name": func_name(target) if target else None,
            "target_seg": seg_name(target) if target else None,
        })
    return slots


def collect_slot_hits():
    hits = {key: [] for key in TARGETS}
    inferred = []
    for ea, value, data_seg in walk_qwords():
        for key, target in TARGETS.items():
            if value != target:
                continue
            row = {
                "entry": hx(ea),
                "data_seg": data_seg,
                "target": hx(value),
                "target_name": func_name(value),
            }
            hits[key].append(row)
            for offset in INFERRED_HEAD_OFFSETS.get(key, []):
                head = ea - offset
                inferred.append({
                    "reason": key,
                    "slot_offset": hx(offset),
                    "head": hx(head),
                    "head_name": ida_name.get_name(head) or None,
                    "entry": hx(ea),
                    "target": hx(value),
                    "slots": table_slots(head, 24 if key.startswith("forzamulticam") else 64),
                })
    return hits, inferred


def collect_named_vtables():
    rows = []
    needles = [
        "CCam",
        "CamCopter",
        "ForzaMultiCam",
        "CMultiCam",
        "Ref_count_obj2",
        "Ref_count",
    ]
    for ea, name in idautils.Names():
        demangled = idc.demangle_name(name, idc.get_inf_attr(idc.INF_SHORT_DN))
        hay = "%s %s" % (name or "", demangled or "")
        if not any(n in hay for n in needles):
            continue
        rows.append({
            "ea": hx(ea),
            "name": name,
            "demangled": demangled,
            "seg": seg_name(ea),
            "qword": hx(qword(ea)),
            "qword_name": func_name(qword(ea)) if qword(ea) else None,
        })
    rows.sort(key=lambda x: int(x["ea"], 16))
    return rows


def main():
    if os.environ.get("FH5_IDA_SKIP_AUTOWAIT", "0") != "1":
        ida_auto.auto_wait()
    out_dir = os.environ.get("FH5_IDA_OUT", r"E:\ForzaHorizon5_IDA_Decompile\empress_headless_camera_tables")
    os.makedirs(out_dir, exist_ok=True)

    hits, inferred = collect_slot_hits()
    named = collect_named_vtables()
    payload = {
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "database": idaapi.get_input_file_path(),
        "input_file": ida_nalt.get_input_file_path(),
        "image_base": hx(idaapi.get_imagebase()),
        "ida_min_ea": hx(ida_ida.inf_get_min_ea()),
        "ida_max_ea": hx(ida_ida.inf_get_max_ea()),
        "input_sha256": ida_nalt.retrieve_input_file_sha256().hex()
            if ida_nalt.retrieve_input_file_sha256() else None,
        "targets": {k: hx(v) for k, v in TARGETS.items()},
        "slot_hits": hits,
        "inferred_table_heads": inferred,
        "named_camera_multicam_vtables": named,
    }

    json_path = os.path.join(out_dir, "fh5_empress_camera_tables.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)

    md_path = os.path.join(out_dir, "fh5_empress_camera_tables.md")
    lines = []
    lines.append("# FH5 Empress Camera Tables")
    lines.append("")
    lines.append("- database: `%s`" % payload["database"])
    lines.append("- input: `%s`" % payload["input_file"])
    lines.append("- image_base: `%s`" % payload["image_base"])
    lines.append("- input_sha256: `%s`" % payload["input_sha256"])
    lines.append("")
    lines.append("## Slot Hits")
    for key, rows in hits.items():
        lines.append("")
        lines.append("### %s `%s`" % (key, hx(TARGETS[key])))
        if not rows:
            lines.append("- none")
            continue
        for row in rows:
            lines.append("- `%s` seg=`%s` -> `%s` `%s`" % (
                row["entry"], row["data_seg"], row["target"], row["target_name"]))
    lines.append("")
    lines.append("## Inferred Table Heads")
    for row in inferred:
        lines.append("- `%s` from %s slot_offset `%s` entry `%s`" % (
            row["head"], row["reason"], row["slot_offset"], row["entry"]))
    lines.append("")
    lines.append("## Named Camera / Multicam Rows")
    for row in named[:250]:
        lines.append("- `%s` seg=`%s` name=`%s` demangled=`%s` qword=`%s` qword_name=`%s`" % (
            row["ea"], row["seg"], row["name"], row["demangled"], row["qword"], row["qword_name"]))
    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print("WROTE %s" % json_path)
    print("WROTE %s" % md_path)
    idc.qexit(0)


if __name__ == "__main__":
    main()
