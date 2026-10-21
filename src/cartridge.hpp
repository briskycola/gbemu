#pragma once

#include "cartridge_metadata.hpp"
#include "address.hpp"
#include "register.hpp"
#include <vector>
#include <memory>

// This file will be used to get
// the actual data from the cartridge,
// not the metadata.
//
// Not all Game Boy cartridges are
// the same. Some have Memory Bank
// Controllers (MBCs)
//
// Since the Game Boy had a very
// limited amount of RAM, some games
// took advantage of MBCs to expand
// the Game Boy's available RAM.
//
// The Game Boy will use bank switching
// to switch between main memory and
// the MBC.
//
// You can sort of think of it as
// swapping/paging in an OS,
// but more basic.
//
// https://gbdev.io/pandocs/MBCs.html

class Cartridge
{
    protected:
        std::vector<uint8_t> rom;
        std::vector<uint8_t> ram;
        std::unique_ptr<CartridgeInfo> cartridgeInfo;

    public:
        Cartridge(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
                  std::unique_ptr<CartridgeInfo> cartridgeInfo);
        virtual ~Cartridge() = default;

        virtual uint8_t read(const Address &address) const = 0;
        virtual void write(const Address &address, uint8_t value) = 0;
        virtual const std::vector<uint8_t>& getCartridgeRAM() const;
        bool supportsCGB() const;
        bool isCGBOnly() const;
        bool supportsSGB() const;
};

class MBC2 : public Cartridge
{
    private:
        uint8_t romBank = 1;
        bool ramEnabled = false;
        std::vector<uint8_t> nibbleRAM;

    public:
        MBC2(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo);
        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
        const std::vector<uint8_t>& getCartridgeRAM() const override;
};

// The ram parameter is optional since not all
// Game Boy cartidges have a MBC.
std::shared_ptr<Cartridge> getCartridge(const std::vector<uint8_t> &rom,
                                        const std::vector<uint8_t> &ram = {});

class NoMBC : public Cartridge
{
    public:
        NoMBC(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
              std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC1 : public Cartridge
{
    private:
        WordRegister romBank;
        WordRegister ramBank;

        bool isRAMEnabled = false;
        bool bankingMode = false;

    public:
        MBC1(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC3 : public Cartridge
{
    private:
        WordRegister romBank;
        WordRegister ramBank;

        bool isRAMEnabled = false;
        //bool romBankingMode = true;
        bool isRAMOverRTC = true;

    public:
        MBC3(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             const std::unique_ptr<CartridgeInfo> cartridgeInfo);

        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};

class MBC5 : public Cartridge
{
    private:
        uint16_t romBank = 1;
        uint8_t ramBank = 0;
        bool ramEnabled = false;

    public:
        MBC5(std::vector<uint8_t> rom, const std::vector<uint8_t> &ram,
             std::unique_ptr<CartridgeInfo> cartridgeInfo);
        uint8_t read(const Address &address) const override;
        void write(const Address &address, uint8_t value) override;
};
