# Esphome-components-ultimate-min

ESP tylko odbiera wM-Bus (T1/C1) i publikuje telegramy przez MQTT. Dekodowanie jest poza ESP.
Minimalne komponenty do ESPHome:
- **wmbus_radio** (SX1276) – odbiór wM-Bus i zwrot ramek do automations (`on_frame`)
- **wmbus_common** – tylko minimum: LinkMode (T1/C1) + usuwanie DLL CRC (Format A/B)

## Co to robi
To jest **RF -> MQTT bridge**. ESP nie dekoduje liczników.
Wypluwasz:
- `frame->as_hex()` (HEX po normalizacji: T1 3of6 decode / C1 suffix cut / DLL CRC removed)
- opcjonalnie `frame->as_rtlwmbus()`

## Dlaczego to jest "safe"
- CRC DLL jest sprawdzane. Jeśli CRC się nie zgadza -> ramka jest odrzucana.
- Nie ma tabel producentów, driverów meterów, AES itp.

## Użycie w ESPHome
Zobacz `examples/UltimateReader.yaml`.


## RAW vs NORM (ważne)
- `Frame::as_hex()` zwraca **telegram znormalizowany (NORM)**:
  - T1: po dekodowaniu 3of6
  - C1: po ucięciu 2B suffix
  - DLL CRC: sprawdzone i zdjęte (A/B); jeśli CRC nie pasuje -> ramka jest odrzucana
- `Frame::as_hex_raw()` zwraca **surowe bajty z radia (RAW)** (przed normalizacją).


## Przykłady
- `examples/UltimateReader_strict.yaml` –  + opcjonalny RAW.
- `examples/UltimateReader_lite.yaml` – profil oszczędny (frame->size()), logger WARN, bez API/time/captive_portal.

## Użycie (ESPHome)

W YAML dodaj:

```yaml
external_components:
  - source: github://Kustonium/esphome-wmbus-bridge@main
    components: [wmbus_common, wmbus_radio]
    refresh: 0d
```

Potem użyj jednego z przykładów w `examples/`.
