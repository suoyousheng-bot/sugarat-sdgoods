# Sugarat Cover validation

Recorded results for the community release. They describe the recorded build and devices only; a rebuilt or modified image needs its own verification.

## Community release notes

v0.1.4: ESP-IDF 5.5.4 build and application-image inspection passed; the image was written with hash verification and booted with working storage, and on the badge cover swipes slide smoothly without the earlier pause. Since v0.1.3, on a badge with an erased application-data partition, the firmware created the filesystem and saved the starter photo, and phone upload through the scanned management page worked. Host ASan/UBSan checks cover storage, verified photo loading, gestures, decode-free preloaded swipes, paired transfer with loss/corruption/duplicates and failed saves, plus 24 bilingual LVGL snapshots. Browser checks exercise the actual C HTTP handlers for crop/upload/select/delete, language persistence, mobile layouts and the Android picker notice. Not yet verified on hardware: the Android sign-in-window notice, two-device radio transfer, power-loss recovery and multi-app launcher installation/Exit; range, throughput and battery life are unmeasured.
