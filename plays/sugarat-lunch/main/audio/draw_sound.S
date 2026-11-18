/* SPDX-License-Identifier: Apache-2.0 */
/* The audio driver reads the WAV payload as aligned 16-bit PCM. */
.section .rodata.draw_sound,"a",@progbits
.balign 2
.global _binary_draw_wav_start
_binary_draw_wav_start:
.incbin LUNCH_DRAW_WAV
.global _binary_draw_wav_end
_binary_draw_wav_end:
