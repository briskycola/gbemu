#pragma once

#include <cstdint>

// Addresses of the memory-mapped hardware
// registers in the 0xFF00-0xFF7F I/O range.
// The pandocs name of each register is in
// the comment next to it.
//
// https://gbdev.io/pandocs/Hardware_Reg_List.html

// Joypad, serial, and timer
static constexpr uint16_t JOYPAD_REGISTER        = 0xFF00; // P1/JOYP
static constexpr uint16_t SERIAL_DATA_REGISTER    = 0xFF01; // SB
static constexpr uint16_t SERIAL_CONTROL_REGISTER = 0xFF02; // SC
static constexpr uint16_t DIVIDER_REGISTER       = 0xFF04; // DIV
static constexpr uint16_t TIMER_COUNTER_REGISTER  = 0xFF05; // TIMA
static constexpr uint16_t TIMER_MODULO_REGISTER   = 0xFF06; // TMA
static constexpr uint16_t TIMER_CONTROL_REGISTER  = 0xFF07; // TAC
static constexpr uint16_t INTERRUPT_FLAG_REGISTER = 0xFF0F; // IF

// Audio
static constexpr uint16_t CHANNEL1_SWEEP_REGISTER       = 0xFF10; // NR10
static constexpr uint16_t CHANNEL1_LENGTH_DUTY_REGISTER  = 0xFF11; // NR11
static constexpr uint16_t CHANNEL1_ENVELOPE_REGISTER    = 0xFF12; // NR12
static constexpr uint16_t CHANNEL1_PERIOD_LOW_REGISTER   = 0xFF13; // NR13
static constexpr uint16_t CHANNEL1_CONTROL_REGISTER     = 0xFF14; // NR14
static constexpr uint16_t CHANNEL2_LENGTH_DUTY_REGISTER  = 0xFF16; // NR21
static constexpr uint16_t CHANNEL2_ENVELOPE_REGISTER    = 0xFF17; // NR22
static constexpr uint16_t CHANNEL2_PERIOD_LOW_REGISTER   = 0xFF18; // NR23
static constexpr uint16_t CHANNEL2_CONTROL_REGISTER     = 0xFF19; // NR24
static constexpr uint16_t CHANNEL3_DAC_ENABLE_REGISTER   = 0xFF1A; // NR30
static constexpr uint16_t CHANNEL3_LENGTH_REGISTER      = 0xFF1B; // NR31
static constexpr uint16_t CHANNEL3_OUTPUT_LEVEL_REGISTER = 0xFF1C; // NR32
static constexpr uint16_t CHANNEL3_PERIOD_LOW_REGISTER   = 0xFF1D; // NR33
static constexpr uint16_t CHANNEL3_CONTROL_REGISTER     = 0xFF1E; // NR34
static constexpr uint16_t CHANNEL4_LENGTH_REGISTER      = 0xFF20; // NR41
static constexpr uint16_t CHANNEL4_ENVELOPE_REGISTER    = 0xFF21; // NR42
static constexpr uint16_t CHANNEL4_FREQUENCY_REGISTER   = 0xFF22; // NR43
static constexpr uint16_t CHANNEL4_CONTROL_REGISTER     = 0xFF23; // NR44
static constexpr uint16_t MASTER_VOLUME_REGISTER        = 0xFF24; // NR50
static constexpr uint16_t SOUND_PANNING_REGISTER        = 0xFF25; // NR51
static constexpr uint16_t SOUND_CONTROL_REGISTER        = 0xFF26; // NR52
static constexpr uint16_t WAVE_RAM_START                = 0xFF30;
static constexpr uint16_t WAVE_RAM_END                  = 0xFF3F;

// Everything the APU handles, including the
// unused addresses between its registers
static constexpr uint16_t AUDIO_ADDRESS_START = CHANNEL1_SWEEP_REGISTER;
static constexpr uint16_t AUDIO_ADDRESS_END   = WAVE_RAM_END;

// LCD
static constexpr uint16_t LCD_CONTROL_REGISTER  = 0xFF40; // LCDC
static constexpr uint16_t LCD_STATUS_REGISTER   = 0xFF41; // STAT
static constexpr uint16_t SCROLL_Y_REGISTER     = 0xFF42; // SCY
static constexpr uint16_t SCROLL_X_REGISTER     = 0xFF43; // SCX
static constexpr uint16_t LCD_Y_REGISTER        = 0xFF44; // LY
static constexpr uint16_t LCD_Y_COMPARE_REGISTER = 0xFF45; // LYC
static constexpr uint16_t OAM_DMA_REGISTER      = 0xFF46; // DMA
static constexpr uint16_t BG_PALETTE_REGISTER   = 0xFF47; // BGP
static constexpr uint16_t OBJ_PALETTE0_REGISTER = 0xFF48; // OBP0
static constexpr uint16_t OBJ_PALETTE1_REGISTER = 0xFF49; // OBP1
static constexpr uint16_t WINDOW_Y_REGISTER     = 0xFF4A; // WY
static constexpr uint16_t WINDOW_X_REGISTER     = 0xFF4B; // WX

// Writing a nonzero value unmaps the boot ROM
static constexpr uint16_t BOOT_ROM_DISABLE_REGISTER = 0xFF50; // BANK

// Game Boy Color
static constexpr uint16_t CGB_MODE_REGISTER             = 0xFF4C; // KEY0
static constexpr uint16_t SPEED_SWITCH_REGISTER         = 0xFF4D; // KEY1
static constexpr uint16_t VRAM_BANK_REGISTER            = 0xFF4F; // VBK
static constexpr uint16_t HDMA_SOURCE_HIGH_REGISTER      = 0xFF51; // HDMA1
static constexpr uint16_t HDMA_SOURCE_LOW_REGISTER       = 0xFF52; // HDMA2
static constexpr uint16_t HDMA_DESTINATION_HIGH_REGISTER = 0xFF53; // HDMA3
static constexpr uint16_t HDMA_DESTINATION_LOW_REGISTER  = 0xFF54; // HDMA4
static constexpr uint16_t HDMA_CONTROL_REGISTER         = 0xFF55; // HDMA5
static constexpr uint16_t BG_PALETTE_INDEX_REGISTER      = 0xFF68; // BCPS
static constexpr uint16_t BG_PALETTE_DATA_REGISTER       = 0xFF69; // BCPD
static constexpr uint16_t OBJ_PALETTE_INDEX_REGISTER     = 0xFF6A; // OCPS
static constexpr uint16_t OBJ_PALETTE_DATA_REGISTER      = 0xFF6B; // OCPD
static constexpr uint16_t OBJ_PRIORITY_MODE_REGISTER     = 0xFF6C; // OPRI
static constexpr uint16_t WRAM_BANK_REGISTER            = 0xFF70; // SVBK

// Reads from unmapped memory and unused
// registers return all bits set
static constexpr uint8_t UNMAPPED_READ_VALUE = 0xFF;
