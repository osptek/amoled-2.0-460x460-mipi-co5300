# 2.0" 460×460 AMOLED MIPI module (CO5300) — documentation & samples

**简体中文：** [`README.md`](README.md)

---

> This repository provides **sample projects** for this module, together with datasheets, specifications, and interface / bring-up documentation for selection reference and integration.

## Product overview

| Item | Description |
|:--|:--|
| Module | 2.0-inch **AMOLED** panel, **460×460** resolution |
| Interface | **MIPI** |
| Driver IC | **CO5300** |
| Spec ID | **`2.0-amoled-460x460-mipi-co5300`** is the common product designation in documentation |

---

## Repository layout

### Top-level

| Path | Contents |
|:--|:--|
| `docs/` | Datasheets, adapter schematics, touch IC documentation |
| `examples/` | **Sample projects** |

### `examples/` layout

| Location | Description (internal package folder) |
|:--|:--|
| `examples/` root | **ESP-IDF代码** (esp-lvgl-port + LVGL9, CST820 touch) |

### Sample project paths

| Description | Path |
|:--|:--|
| esp-lvgl-port + LVGL9 | `examples/esp32p4-idf5_co5300-mipi_esp-lvgl-port_lvgl9/` |
