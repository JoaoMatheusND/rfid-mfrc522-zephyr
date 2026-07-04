# rfid-mfrc522-zephyr

MFRC522 RFID r/w driver as a zephyr module. The final `west.yml` will look like:

```yaml
      [...]
    - name: rfid-mfrc522-zephyr
      url: https://github.com/JoaoMatheusND/oled-sh1106-zephyr
      revision: zephyr
      clone-depth: 1
      path: deps/rfid
```