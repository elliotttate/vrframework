import json
import os
import time

import ida_bytes
import ida_funcs
import ida_name
import ida_nalt
import ida_segment
import ida_ua
import idaapi
import idautils
import idc


WATCH_OFFSETS = {
    0x188, 0x190, 0x198,
    0x320, 0x330, 0x340, 0x350, 0x360, 0x370, 0x380, 0x390,
    0x5C8, 0x5D8, 0x600, 0x650, 0x660, 0x670, 0x680,
}

DEFAULT_TABLES = [
    0x1465D6808,
    0x1465D6BA0,
    0x145FA0440,
    0x146589F90,
    0x145E3F290,
    0x145E1FF90,
]


def hx(value):
    if value is None or value == idaapi.BADADDR:
        return None
    return "0x%X" % int(value)


def seg_name(ea):
    seg = ida_segment.getseg(ea)
    return ida_segment.get_segm_name(seg) if seg else None


def name_at(ea):
    if ea is None or ea == idaapi.BADADDR:
        return None
    name = ida_name.get_name(ea)
    if name:
        return name
    func = ida_funcs.get_func(ea)
    if func:
        return ida_name.get_name(func.start_ea) or hx(func.start_ea)
    return None


def demangle(name):
    return idc.demangle_name(name, idc.get_inf_attr(idc.INF_SHORT_DN)) if name else None


def qword(ea):
    try:
        return ida_bytes.get_qword(ea)
    except Exception:
        return None


def func_offset_hits(start):
    func = ida_funcs.get_func(start)
    if not func:
        return []
    hits = []
    for ea in idautils.FuncItems(func.start_ea):
        insn = ida_ua.insn_t()
        if ida_ua.decode_insn(insn, ea) <= 0:
            continue
        for idx in range(8):
            op = insn.ops[idx]
            if op.type == ida_ua.o_void:
                break
            value = None
            if op.type in (ida_ua.o_displ, ida_ua.o_mem):
                value = int(op.addr)
            elif op.type == ida_ua.o_imm:
                value = int(op.value)
            if value in WATCH_OFFSETS:
                hits.append({
                    "ea": hx(ea),
                    "offset": hx(value),
                    "line": idc.generate_disasm_line(ea, 0) or "",
                })
    return hits[:80]


def disasm_head(start, limit=24):
    func = ida_funcs.get_func(start)
    if not func:
        return []
    rows = []
    for i, ea in enumerate(idautils.FuncItems(func.start_ea)):
        if i >= limit:
            break
        rows.append({"ea": hx(ea), "line": idc.generate_disasm_line(ea, 0) or ""})
    return rows


def dump_table(head, slot_count=96):
    row = {
        "head": hx(head),
        "head_name": name_at(head),
        "head_demangled": demangle(name_at(head)),
        "head_seg": seg_name(head),
        "prefix": [],
        "slots": [],
    }
    for back in (0x28, 0x20, 0x18, 0x10, 0x8):
        ea = head - back
        value = qword(ea)
        row["prefix"].append({
            "ea": hx(ea),
            "qword": hx(value),
            "name": name_at(value),
            "demangled": demangle(name_at(value)),
            "seg": seg_name(value) if value else None,
        })
    for slot in range(slot_count):
        entry = head + slot * 8
        target = qword(entry)
        row["slots"].append({
            "slot": slot,
            "entry": hx(entry),
            "target": hx(target),
            "target_name": name_at(target),
            "target_demangled": demangle(name_at(target)),
            "target_seg": seg_name(target) if target else None,
            "offset_hits": func_offset_hits(target) if target and seg_name(target) == ".text" else [],
            "disasm_head": disasm_head(target, 12) if target and seg_name(target) == ".text" else [],
        })
    return row


def parse_tables():
    raw = os.environ.get("FH5_IDA_VTABLES", "")
    if not raw.strip():
        return DEFAULT_TABLES
    out = []
    for part in raw.split(","):
        part = part.strip()
        if not part:
            continue
        out.append(int(part, 16))
    return out


def main():
    out_dir = os.environ.get("FH5_IDA_OUT", r"E:\ForzaHorizon5_IDA_Decompile\empress_headless_runtime_vtables")
    os.makedirs(out_dir, exist_ok=True)
    tables = [dump_table(addr) for addr in parse_tables()]
    payload = {
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "database": idaapi.get_input_file_path(),
        "input_file": ida_nalt.get_input_file_path(),
        "image_base": hx(idaapi.get_imagebase()),
        "input_sha256": ida_nalt.retrieve_input_file_sha256().hex()
            if ida_nalt.retrieve_input_file_sha256() else None,
        "tables": tables,
    }
    json_path = os.path.join(out_dir, "fh5_empress_runtime_vtables.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)
    md_path = os.path.join(out_dir, "fh5_empress_runtime_vtables.md")
    lines = []
    lines.append("# FH5 Empress Runtime VTables")
    lines.append("")
    lines.append("- database: `%s`" % payload["database"])
    lines.append("- input_sha256: `%s`" % payload["input_sha256"])
    for table in tables:
        lines.append("")
        lines.append("## `%s` name=`%s` demangled=`%s` seg=`%s`" % (
            table["head"], table["head_name"], table["head_demangled"], table["head_seg"]))
        lines.append("### Prefix")
        for p in table["prefix"]:
            lines.append("- `%s` -> `%s` name=`%s` demangled=`%s` seg=`%s`" % (
                p["ea"], p["qword"], p["name"], p["demangled"], p["seg"]))
        lines.append("### Slots")
        for slot in table["slots"][:64]:
            hits = ",".join(sorted({h["offset"] for h in slot["offset_hits"]}))
            lines.append("- slot %02d `%s` -> `%s` name=`%s` seg=`%s` hits=`%s`" % (
                slot["slot"], slot["entry"], slot["target"], slot["target_name"], slot["target_seg"], hits))
            for hit in slot["offset_hits"][:8]:
                lines.append("  - `%s` `%s` %s" % (hit["ea"], hit["offset"], hit["line"]))
    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("WROTE %s" % json_path)
    print("WROTE %s" % md_path)
    idc.qexit(0)


if __name__ == "__main__":
    main()
