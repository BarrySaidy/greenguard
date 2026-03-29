# GreenGuard 🌿

An IoT plant health monitor built with senseBox MCU 2.1 and senseBox Eye.

## System
- **Sensors**: HDC1080 (temp/humidity), TSL45315 (light), VL53L0X (ToF)
- **Vision**: senseBox Eye with Edge Impulse image classifier (healthy / early sick / critical)
- **Display**: OLED SSD1306 4-screen dashboard
- **Cloud**: openSenseMap live data logging
- **Browser**: WiFi dashboard with live sensor data + camera images

## Project Structure
- `data/` — training dataset (local only, not tracked by git)
- `models/` — Edge Impulse exported Arduino libraries
- `firmware/eye1/` — senseBox Eye Arduino sketch
- `firmware/mcu/` — senseBox MCU Arduino sketch
- `docs/` — wiring diagrams and notes

## Phases
- [x] Phase 1 — Dataset + Edge Impulse model
- [ ] Phase 2 — senseBox Eye firmware
- [ ] Phase 3 — senseBox MCU firmware
- [ ] Phase 4 — Integration + full system test
