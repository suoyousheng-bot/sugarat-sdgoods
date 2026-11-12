/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Deotaland
 *
 * The single worker that owns every slow or flash-mutating launcher operation:
 * loading the app list and icons at boot, looking up a play and its cover, downloading and installing
 * it, and uninstalling.
 * One job runs at a time. The LVGL thread starts jobs and polls launcher_job_status().
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "app_store.h"
#include "community.h"

typedef enum { JOB_NONE, JOB_LOOKUP, JOB_INSTALL, JOB_UNINSTALL } launcher_job_kind_t;
typedef enum { JOB_RUNNING, JOB_SUCCEEDED, JOB_FAILED } launcher_job_phase_t;
typedef enum { JOB_STEP_PREPARE, JOB_STEP_DOWNLOAD, JOB_STEP_VERIFY } launcher_job_step_t;

typedef struct {
    launcher_job_kind_t kind;          /**< Most recent job; JOB_NONE before the first. */
    launcher_job_phase_t phase;
    launcher_job_step_t step;          /**< Install progress stage. */
    uint32_t sequence;                 /**< Increments when a job starts. */
    community_status_t network;        /**< Network outcome of lookup/install. */
    app_problem_t problem;             /**< Storage outcome of install/uninstall. */
    community_play_t play;             /**< Lookup result, or the play being installed. */
    uint32_t received, total;          /**< Install download progress in bytes. */
    app_info_t installed;              /**< Installed app after a successful install. */
} launcher_job_status_t;

/** Start the worker; it loads the app store first. */
void launcher_job_start(void);

/** @returns False while another job runs. */
bool launcher_job_lookup(uint32_t play_id);

/** Download and install `play`. `replaces_slot` is the installed slot of the same play or -1.
 * @param release_first - Free the old slot (keeping its data) before allocating space.
 * @returns False while another job runs. */
bool launcher_job_install(const community_play_t *play, int replaces_slot, bool release_first);

/** Uninstall one slot and its data, or every app when `slot` is -1.
 * @returns False while another job runs. */
bool launcher_job_uninstall(int slot);

/** Ask a running install to stop; the partially written slot is released. */
void launcher_job_cancel(void);

/** Copy the current status. */
void launcher_job_status(launcher_job_status_t *out);
