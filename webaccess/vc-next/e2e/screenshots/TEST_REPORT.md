# 🎛️ DMX Web Control — Test Report

> Generated: **2026-04-25 12:38:37**  
> Duration: **31.1s** | Status: **passed**  
> Tests: **14** passed, **0** failed, **0** skipped — **14** total

## Summary

| Status | Test | Duration |
|--------|------|----------|
| ✅ | T1+T3: DMX Control loads directly — no VC tab 📸×1 | 1598ms |
| ✅ | T5: Fixtures sorted by DMX address 📸×1 | 1441ms |
| ✅ | T9: Model-based filter badges 📸×2 | 2007ms |
| ✅ | T6: Collapsible universe groups 📸×2 | 2456ms |
| ✅ | Cross-tab color sync animation 📸×6 | 8998ms |
| ✅ | T4: Controls sorted by channel index 📸×1 | 1886ms |
| ✅ | T7: Auto-open raw + star for raw-only fixtures 📸×1 | 1868ms |
| ✅ | T8: Raw channels = ALL channels with read-only monitors 📸×1 | 2123ms |
| ✅ | Cross-tab position sync animation 📸×6 | 8222ms |
| ✅ | Cross-tab sync: color picker change propagates 📸×3 | 5944ms |
| ✅ | Cross-tab sync: position pad change propagates 📸×3 | 5573ms |
| ✅ | Cross-tab fader sync animation 📸×6 | 8179ms |
| ✅ | Feature walkthrough animation 📸×7 | 4826ms |
| ✅ | Cross-tab sync: fader change propagates 📸×3 | 5584ms |

---

## chromium › screenshot-docs.spec.ts › Feature Screenshots

### ✅ T1+T3: DMX Control loads directly — no VC tab

- **File:** `e2e/screenshot-docs.spec.ts:43`
- **Duration:** 1598ms

**01-dmx-only**

![01-dmx-only](01-dmx-only.png)

---

### ✅ T5: Fixtures sorted by DMX address

- **File:** `e2e/screenshot-docs.spec.ts:49`
- **Duration:** 1441ms

**02-fixture-order**

![02-fixture-order](02-fixture-order.png)

---

### ✅ T9: Model-based filter badges

- **File:** `e2e/screenshot-docs.spec.ts:54`
- **Duration:** 2007ms

**03-model-filter-all**

![03-model-filter-all](03-model-filter-all.png)

**03-model-filter-active**

![03-model-filter-active](03-model-filter-active.png)

---

### ✅ T6: Collapsible universe groups

- **File:** `e2e/screenshot-docs.spec.ts:66`
- **Duration:** 2456ms

**04-universe-expanded**

![04-universe-expanded](04-universe-expanded.png)

**04-universe-collapsed**

![04-universe-collapsed](04-universe-collapsed.png)

---

### ✅ T4: Controls sorted by channel index

- **File:** `e2e/screenshot-docs.spec.ts:80`
- **Duration:** 1886ms

**05-channel-order**

![05-channel-order](05-channel-order.png)

---

### ✅ T7: Auto-open raw + star for raw-only fixtures

- **File:** `e2e/screenshot-docs.spec.ts:88`
- **Duration:** 1868ms

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
- **Duration:** 5944ms

**08-sync-color-before-tabB**

![08-sync-color-before-tabB](08-sync-color-before-tabB.png)

**08-sync-color-tabA-changed**

![08-sync-color-tabA-changed](08-sync-color-tabA-changed.png)

**08-sync-color-after-tabB**

![08-sync-color-after-tabB](08-sync-color-after-tabB.png)

---

### ✅ Cross-tab sync: position pad change propagates

- **File:** `e2e/screenshot-docs.spec.ts:152`
- **Duration:** 5573ms

**09-sync-pos-before-tabB**

![09-sync-pos-before-tabB](09-sync-pos-before-tabB.png)

**09-sync-pos-tabA-changed**

![09-sync-pos-tabA-changed](09-sync-pos-tabA-changed.png)

**09-sync-pos-after-tabB**

![09-sync-pos-after-tabB](09-sync-pos-after-tabB.png)

---

### ✅ Cross-tab sync: fader change propagates

- **File:** `e2e/screenshot-docs.spec.ts:184`
- **Duration:** 5584ms

**10-sync-fader-before-tabB**

![10-sync-fader-before-tabB](10-sync-fader-before-tabB.png)

**10-sync-fader-tabA-changed**

![10-sync-fader-tabA-changed](10-sync-fader-tabA-changed.png)

**10-sync-fader-after-tabB**

![10-sync-fader-after-tabB](10-sync-fader-after-tabB.png)

---

## chromium › animated-docs.spec.ts › Animated Documentation

### ✅ Cross-tab color sync animation

- **File:** `e2e/animated-docs.spec.ts:52`
- **Duration:** 8998ms

**color-sync-01-before**

![color-sync-01-before](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/color-sync-01-before-bcffb0085f6ba30244203f4dcea7697868cab208.png)

**color-sync-02-tabA-set**

![color-sync-02-tabA-set](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/color-sync-02-tabA-set-3c5e23ae8fc7b6acb6f6c62a74fbe4d890c1aff3.png)

**color-sync-03-tabB-synced**

![color-sync-03-tabB-synced](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/color-sync-03-tabB-synced-8357d2f84bf0aabd6848c3e6b8e4e74649f7cd98.png)

**color-sync-04-tabA-changed**

![color-sync-04-tabA-changed](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/color-sync-04-tabA-changed-725bfb801506f230a206a7c301f139c53bcee4aa.png)

**color-sync-05-tabB-updated**

![color-sync-05-tabB-updated](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/color-sync-05-tabB-updated-670e3186667d84b8638e4c1d4322cb2c98176e0b.png)

**cross-tab-color-sync**

![cross-tab-color-sync](../../test-results/animated-docs-Animated-Doc-501d2-ss-tab-color-sync-animation-chromium/attachments/cross-tab-color-sync-75d1034778a2a905aada19508f44719b19138e09.gif)

---

### ✅ Cross-tab position sync animation

- **File:** `e2e/animated-docs.spec.ts:114`
- **Duration:** 8222ms

**pos-sync-01-before**

![pos-sync-01-before](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/pos-sync-01-before-9c395264d0eabf1be1afce25eed3a2dad261aeff.png)

**pos-sync-02-tabA-moved**

![pos-sync-02-tabA-moved](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/pos-sync-02-tabA-moved-98b12151d67ab603f2dae505c6bc6fbe6002ab58.png)

**pos-sync-03-tabB-synced**

![pos-sync-03-tabB-synced](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/pos-sync-03-tabB-synced-66032ba0be8d98f6aed1289cc7efc625577ab7ff.png)

**pos-sync-04-tabA-moved2**

![pos-sync-04-tabA-moved2](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/pos-sync-04-tabA-moved2-0f2849d3110be856879169cda49832978739928e.png)

**pos-sync-05-tabB-synced2**

![pos-sync-05-tabB-synced2](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/pos-sync-05-tabB-synced2-3b3358c7e7729a5cf15fde906eca343ebef3ebd3.png)

**cross-tab-position-sync**

![cross-tab-position-sync](../../test-results/animated-docs-Animated-Doc-5a711-tab-position-sync-animation-chromium/attachments/cross-tab-position-sync-34c5c31a18bb1c09c8a7b1f7214fff84c63703ce.gif)

---

### ✅ Cross-tab fader sync animation

- **File:** `e2e/animated-docs.spec.ts:164`
- **Duration:** 8179ms

**fader-sync-01-before**

![fader-sync-01-before](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/fader-sync-01-before-5aadcf59ad663217da00dca781e3cb668a8d9ef1.png)

**fader-sync-02-tabA-high**

![fader-sync-02-tabA-high](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/fader-sync-02-tabA-high-1d7c92f8d96325e8ca8286839515678522a0fec9.png)

**fader-sync-03-tabB-synced**

![fader-sync-03-tabB-synced](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/fader-sync-03-tabB-synced-8c90cd8cd05634c12c7e11fb3ea6d78e57eb6869.png)

**fader-sync-04-tabA-mid**

![fader-sync-04-tabA-mid](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/fader-sync-04-tabA-mid-9cf8e5bdad36d276a20175203222fdbb5bc46b21.png)

**fader-sync-05-tabB-synced2**

![fader-sync-05-tabB-synced2](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/fader-sync-05-tabB-synced2-eeae1ff3ec7b9be25f026c70353ace9f4bd6a6cd.png)

**cross-tab-fader-sync**

![cross-tab-fader-sync](../../test-results/animated-docs-Animated-Doc-5af2d-ss-tab-fader-sync-animation-chromium/attachments/cross-tab-fader-sync-c56bad50560582f23ce055b422520aad40ff7acb.gif)

---

### ✅ Feature walkthrough animation

- **File:** `e2e/animated-docs.spec.ts:214`
- **Duration:** 4826ms

**walkthrough-01-initial**

![walkthrough-01-initial](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-01-initial-a02c1a63e79b48890f05db975ade7791fb2963fb.png)

**walkthrough-02-filtered**

![walkthrough-02-filtered](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-02-filtered-4a94c3e49fb67585fbae8767839cfbff40f4ae7e.png)

**walkthrough-03-grouped**

![walkthrough-03-grouped](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-03-grouped-5ea85000d5af5d2cedaa7b2868d4a34e52a48376.png)

**walkthrough-04-collapsed**

![walkthrough-04-collapsed](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-04-collapsed-7c0bfd1d0de4210adcb4b22d30b8238f89eca177.png)

**walkthrough-05-hero-controls**

![walkthrough-05-hero-controls](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-05-hero-controls-a7f54d7dc6a89c2de4fd45368e45e397aa8aaa2f.png)

**walkthrough-06-raw-open**

![walkthrough-06-raw-open](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/walkthrough-06-raw-open-cdfcc05b4190e286dd09af7d69bea29435be0d14.png)

**feature-walkthrough**

![feature-walkthrough](../../test-results/animated-docs-Animated-Doc-099d7-ature-walkthrough-animation-chromium/attachments/feature-walkthrough-767083194131d83c881a8084b7f3557ea6c47885.gif)

---
