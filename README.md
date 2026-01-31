# esphome-wmbus-bridge

ESP tylko odbiera wM-Bus (T1/C1) i publikuje telegramy przez MQTT. Dekodowanie jest poza ESP.

## Co to jest
Minimalne komponenty do ESPHome:
- **wmbus_radio** – odbiornik wM-Bus na **SX1276** (SPI) i trigger `on_frame`
- **wmbus_common** – minimum do normalizacji ramek: LinkMode (T1/C1) + DLL CRC (Format A/B)

## RAW vs NORM
- `frame->as_hex()` = **NORM**: T1 po 3of6 decode, C1 po ucięciu 2B suffix, DLL CRC sprawdzone i zdjęte (A/B). Jeśli CRC nie pasuje → ramka jest odrzucana.
- `frame->as_hex_raw()` = **RAW** bajty z radia (przed normalizacją).

## Użycie (ESPHome)

```yaml
external_components:
  - source: github://Kustonium/esphome-wmbus-bridge@main
    components: [wmbus_common, wmbus_radio]
    refresh: 0d

Przykłady

examples/UltimateReader_strict.yaml

examples/UltimateReader_lite.yaml – profil oszczędny (filtr frame->size()), logger WARN, bez API/time/captive_portal

Atrybucja i licencja

Ten projekt jest pochodną prac:

SzczepanLeon/esphome-components (autor: Szczepan Leon)

wmbusmeters/wmbusmeters (GPL)

Licencja tego repo: GPL-3.0-or-later (szczegóły w LICENSE i NOTICE).