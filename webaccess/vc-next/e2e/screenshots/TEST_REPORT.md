# 🎛️ DMX Web Control — Test Report

> Generated: **2026-04-25 12:23:03**  
> Duration: **31.0s** | Status: **passed**  
> Tests: **10** passed, **0** failed, **0** skipped — **10** total

## Summary

| Status | Test | Duration |
|--------|------|----------|
| ✅ | T1+T3: DMX Control loads directly — no VC tab 📸×1 | 1523ms |
| ✅ | T5: Fixtures sorted by DMX address 📸×1 | 1416ms |
| ✅ | T9: Model-based filter badges 📸×2 | 2036ms |
| ✅ | T6: Collapsible universe groups 📸×2 | 2453ms |
| ✅ | T4: Controls sorted by channel index 📸×1 | 1891ms |
| ✅ | T7: Auto-open raw + star for raw-only fixtures 📸×1 | 1863ms |
| ✅ | T8: Raw channels = ALL channels with read-only monitors 📸×1 | 2123ms |
| ✅ | Cross-tab sync: color picker change propagates 📸×3 | 6089ms |
| ✅ | Cross-tab sync: position pad change propagates 📸×3 | 5520ms |
| ✅ | Cross-tab sync: fader change propagates 📸×3 | 5502ms |

---

## chromium › screenshot-docs.spec.ts › Feature Screenshots

### ✅ T1+T3: DMX Control loads directly — no VC tab

- **File:** `e2e/screenshot-docs.spec.ts:43`
- **Duration:** 1523ms

**01-dmx-only**

![01-dmx-only](01-dmx-only.png)

---

### ✅ T5: Fixtures sorted by DMX address

- **File:** `e2e/screenshot-docs.spec.ts:49`
- **Duration:** 1416ms

**02-fixture-order**

![02-fixture-order](02-fixture-order.png)

---

### ✅ T9: Model-based filter badges

- **File:** `e2e/screenshot-docs.spec.ts:54`
- **Duration:** 2036ms

**03-model-filter-all**

![03-model-filter-all](03-model-filter-all.png)

**03-model-filter-active**

![03-model-filter-active](03-model-filter-active.png)

---

### ✅ T6: Collapsible universe groups

- **File:** `e2e/screenshot-docs.spec.ts:66`
- **Duration:** 2453ms

**04-universe-expanded**

![04-universe-expanded](04-universe-expanded.png)

**04-universe-collapsed**

![04-universe-collapsed](04-universe-collapsed.png)

---

### ✅ T4: Controls sorted by channel index

- **File:** `e2e/screenshot-docs.spec.ts:80`
- **Duration:** 1891ms

**05-channel-order**

![05-channel-order](05-channel-order.png)

---

### ✅ T7: Auto-open raw + star for raw-only fixtures

- **File:** `e2e/screenshot-docs.spec.ts:88`
- **Duration:** 1863ms

**06-raw-auto-open**

![06-raw-auto-open](06-raw-auto-open.png)

---

### ✅ T8: Raw channels = ALL channels with read-only monitors

- **File:** `e2e/screenshot-docs.spec.ts:96`
- **Duration:** 2123ms

**07-raw-all-channels**

![07-raw-all-channels](07-raw-all-channels.png)

---

### ✅ Cross-tab sync: color picker change propagates

- **File:** `e2e/screenshot-docs.spec.ts:113`
- **Duration:** 6089ms

**08-sync-color-before-tabB**

![08-sync-color-before-tabB](08-sync-color-before-tabB.png)

**08-sync-color-tabA-changed**

![08-sync-color-tabA-changed](08-sync-color-tabA-changed.png)

**08-sync-color-after-tabB**

![08-sync-color-after-tabB](08-sync-color-after-tabB.png)

---

### ✅ Cross-tab sync: position pad change propagates

- **File:** `e2e/screenshot-docs.spec.ts:152`
- **Duration:** 5520ms

**09-sync-pos-before-tabB**

![09-sync-pos-before-tabB](09-sync-pos-before-tabB.png)

**09-sync-pos-tabA-changed**

![09-sync-pos-tabA-changed](09-sync-pos-tabA-changed.png)

**09-sync-pos-after-tabB**

![09-sync-pos-after-tabB](09-sync-pos-after-tabB.png)

---

### ✅ Cross-tab sync: fader change propagates

- **File:** `e2e/screenshot-docs.spec.ts:184`
- **Duration:** 5502ms

**10-sync-fader-before-tabB**

![10-sync-fader-before-tabB](10-sync-fader-before-tabB.png)

**10-sync-fader-tabA-changed**

![10-sync-fader-tabA-changed](10-sync-fader-tabA-changed.png)

**10-sync-fader-after-tabB**

![10-sync-fader-after-tabB](10-sync-fader-after-tabB.png)

---
