import json
import os
import time

import ida_auto
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
    0x188: "bridge_provider_a",
    0x190: "bridge_provider_b",
    0x198: "bridge_multicam_ptr",
    0x320: "camera_matrix_row0",
    0x330: "camera_matrix_row1",
    0x340: "camera_matrix_row2",
    0x350: "camera_matrix_row3",
    0x360: "camera_view_tail0",
    0x370: "camera_view_tail1",
    0x380: "camera_view_tail2",
    0x390: "camera_view_tail3",
    0x5C8: "active_camera_slot_or_cam_field",
    0x5D8: "multicam_selected_flag",
    0x600: "multicam_vector_lane",
    0x650: "multicam_matrix_lane0",
    0x660: "multicam_matrix_lane1",
    0x670: "multicam_matrix_lane2",
    0x680: "multicam_matrix_lane3",
}


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
    return ida_name.get_name(ea) or None


def qword(ea):
    try:
        return ida_bytes.get_qword(ea)
    except Exception:
        return None


def disasm_context(func, max_lines=80):
    rows = []
    for i, ea in enumerate(idautils.FuncItems(func.start_ea)):
        if i >= max_lines:
            rows.append({"ea": "...", "text": "truncated"})
            break
        rows.append({"ea": hx(ea), "text": idc.generate_disasm_line(ea, 0) or ""})
    return rows


def operand_offset(op):
    if op.type in (ida_ua.o_displ, ida_ua.o_mem):
        return int(op.addr)
    if op.type == ida_ua.o_imm:
        return int(op.value)
    return None


def collect_function_hits():
    results = {}
    for start in idautils.Functions():
        func = ida_funcs.get_func(start)
        if not func:
            continue
        hits = []
        insn_count = 0
        movups_count = 0
        indirect_count = 0
        for ea in idautils.FuncItems(start):
            insn = ida_ua.insn_t()
            if ida_ua.decode_insn(insn, ea) <= 0:
                continue
            insn_count += 1
            mnemonic = idc.print_insn_mnem(ea).lower()
            if mnemonic in ("movups", "movaps", "movss", "movsd"):
                movups_count += 1
            if mnemonic in ("jmp", "call") and "[" in (idc.generate_disasm_line(ea, 0) or ""):
                indirect_count += 1
            for idx in range(8):
                op = insn.ops[idx]
                if op.type == ida_ua.o_void:
                    break
                off = operand_offset(op)
                if off in WATCH_OFFSETS:
                    hits.append({
                        "ea": hx(ea),
                        "mnemonic": mnemonic,
                        "offset": hx(off),
                        "label": WATCH_OFFSETS[off],
                        "line": idc.generate_disasm_line(ea, 0) or "",
                    })
        if not hits:
            continue

        offsets = sorted(set(hit["offset"] for hit in hits))
        score = len(hits)
        hit_offsets_int = {int(hit["offset"], 16) for hit in hits}
        if {0x320, 0x330, 0x340, 0x350}.issubset(hit_offsets_int):
            score += 40
        if {0x660, 0x670, 0x680}.issubset(hit_offsets_int):
            score += 35
        if 0x198 in hit_offsets_int and 0x660 in hit_offsets_int:
            score += 35
        if 0x5C8 in hit_offsets_int or 0x5D8 in hit_offsets_int:
            score += 15
        if movups_count >= 8:
            score += 8

        results[hx(start)] = {
            "start": hx(start),
            "end": hx(func.end_ea),
            "name": name_at(start),
            "size": int(func.end_ea - start),
            "score": score,
            "insn_count": insn_count,
            "mov_like_count": movups_count,
            "indirect_count": indirect_count,
            "offsets": offsets,
            "hits": hits[:120],
            "context": disasm_context(func, 100),
        }
    return sorted(results.values(), key=lambda r: (r["score"], len(r["hits"]), r["size"]), reverse=True)


def data_qword_hits_for_functions(functions):
    targets = {int(row["start"], 16) for row in functions[:80]}
    hits = []
    for i in range(ida_segment.get_segm_qty()):
        seg = ida_segment.getnseg(i)
        sname = ida_segment.get_segm_name(seg) or ""
        if sname not in (".rdata", ".data"):
            continue
        ea = seg.start_ea
        while ea + 8 <= seg.end_ea:
            value = qword(ea)
            if value in targets:
                hits.append({
                    "entry": hx(ea),
                    "seg": sname,
                    "target": hx(value),
                    "target_name": name_at(value),
                    "possible_table_heads": [hx(ea - off) for off in (0x10, 0x18, 0x20, 0x60, 0x68, 0x78)],
                })
            ea += 8
    return hits


def main():
    if os.environ.get("FH5_IDA_SKIP_AUTOWAIT", "1") != "1":
        ida_auto.auto_wait()
    out_dir = os.environ.get("FH5_IDA_OUT", r"E:\ForzaHorizon5_IDA_Decompile\empress_headless_camera_patterns")
    os.makedirs(out_dir, exist_ok=True)

    functions = collect_function_hits()
    qhits = data_qword_hits_for_functions(functions)
    payload = {
        "generated_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "database": idaapi.get_input_file_path(),
        "input_file": ida_nalt.get_input_file_path(),
        "image_base": hx(idaapi.get_imagebase()),
        "input_sha256": ida_nalt.retrieve_input_file_sha256().hex()
            if ida_nalt.retrieve_input_file_sha256() else None,
        "watch_offsets": {hx(k): v for k, v in WATCH_OFFSETS.items()},
        "ranked_functions": functions[:200],
        "data_qword_hits": qhits,
    }

    json_path = os.path.join(out_dir, "fh5_empress_camera_pattern_probe.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, sort_keys=True)

    md_path = os.path.join(out_dir, "fh5_empress_camera_pattern_probe.md")
    lines = []
    lines.append("# FH5 Empress Camera Pattern Probe")
    lines.append("")
    lines.append("- database: `%s`" % payload["database"])
    lines.append("- input_sha256: `%s`" % payload["input_sha256"])
    lines.append("")
    lines.append("## Ranked Functions")
    for row in functions[:60]:
        lines.append("")
        lines.append("### `%s` score=%s size=%s offsets=%s" % (
            row["start"], row["score"], row["size"], ",".join(row["offsets"])))
        lines.append("- name: `%s` mov_like=%s indirect=%s" % (
            row["name"], row["mov_like_count"], row["indirect_count"]))
        for hit in row["hits"][:24]:
            lines.append("  - `%s` `%s` %s" % (hit["ea"], hit["offset"], hit["line"]))
    lines.append("")
    lines.append("## Data Qword Hits")
    for hit in qhits[:200]:
        lines.append("- `%s` -> `%s` heads=%s" % (
            hit["entry"], hit["target"], ",".join(hit["possible_table_heads"])))
    with open(md_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print("WROTE %s" % json_path)
    print("WROTE %s" % md_path)
    idc.qexit(0)


if __name__ == "__main__":
    main()
