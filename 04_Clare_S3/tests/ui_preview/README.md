# Native LVGL UI regression and preview

Uses the **same `clare_ui.cpp` and fonts as the ESP32 firmware**, with a headless
480×480 RGB565 display and a no-op board lock. No hardware/network requests are
made. `fixture.json` is synthetic meeting data used only by this executable.

After the ESP-IDF build has populated `managed_components`, run on macOS/Linux:

```sh
cmake -S tests/ui_preview -B /tmp/clare-ui-build -DCJSON_DIR="$IDF_PATH/components/json/cJSON"
cmake --build /tmp/clare-ui-build -j8
mkdir -p /tmp/clare-ui-images
cd /tmp/clare-ui-images
/tmp/clare-ui-build/preview
```

The runner writes PPM screenshots and checks navigation, pause/resume callbacks,
Ask auto-entry and stop while listening/answering, persistent conversation controls,
end confirmation, ended-session gating, malformed snapshots,
complete snapshot replacement, topic callback lifetime and long UTF-8 transcripts.
Subtitle checks cover bottom-follow on updates and tab entry, larger white live
speech retained after finalization, and smaller grey history only when the next
sentence arrives. Simulated LVGL ticks check intermediate scroll positions and
arrival at the bottom. Screenshots `16-live-prominent.ppm`,
`17-final-stays-prominent.ppm` and `18-next-sentence.ppm` show that transition.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled by default. Hardware
locks, TLS, I2S and physical touch accuracy still require board verification.

Design: full 480×480 rounded-square layout with 16–20 px margins, dark AMOLED
background, mint primary controls, 24 px GB2312 body font and 20 px secondary
font/buttons. The complete 24 px font requires `LV_FONT_FMT_TXT_LARGE=1` in
both native and ESP-IDF builds. Four tabs share a scrollable reading
area; Ask has its own reading area. Large text buffers explicitly use PSRAM.
Network refresh uses the existing application action queue, not LVGL callbacks.
Snapshots are capped at 256 KiB; failures retain the previous valid content.

Topics and todos follow the bottom on tab entry and snapshot updates; topic detail preserves its reading position. The home preview renders the firmware mascot asset with alpha.

Mascot animation checks verify actual source-frame changes, pause while the home page is hidden, and restart/resume on returning home; screenshots 19/20 capture the blinking frames.
