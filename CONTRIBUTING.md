# Contributing

## Branch strategy

```
main        — stable, always builds, flashed to prototype
develop     — integration branch, CI must pass before merge
feature/*   — individual features (e.g. feature/sht40-driver)
hotfix/*    — urgent fixes against main
```

## Commit conventions

```
feat: add SHT40 sensor driver
fix: MQ-2 warm-up not blocking alert evaluation
docs: update PINOUT with I2S pins
refactor: extract alert threshold logic to helper
test: add NVS config unit test
```

## Pull request checklist

- [ ] Firmware builds without warnings (`pio run -e esp32c6`)
- [ ] Native unit tests pass (`pio test -e native`)
- [ ] `config.h` updated if pins or thresholds changed
- [ ] Relevant doc updated (BOM, PINOUT, PHASE1, MQTT_TOPICS)
- [ ] Tested on physical hardware if sensor/output change

## Step confirmation protocol

Phase 1 is implemented step-by-step.
**Each step requires explicit confirmation before starting the next.**
Update PHASE1.md checkboxes as steps complete.
