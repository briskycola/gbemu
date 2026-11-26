#include "Frontend/SettingsDialog.hpp"
#include "Core/Memory/MMU.hpp"
#include <QAudioDevice>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <utility>

namespace settings_dialog
{
    static constexpr int CATEGORY_LIST_WIDTH = 150;
    static constexpr int MINIMUM_WIDTH       = 640;
    static constexpr int MINIMUM_HEIGHT      = 360;
    // Where each audio device item keeps the device's name
    static constexpr int DEVICE_NAME_ROLE    = Qt::UserRole + 1;
}

SettingsDialog::SettingsDialog(const Settings &settings, std::function<bool(const Settings&)> apply,
                               QWidget *parent)
    : QDialog(parent), apply(std::move(apply))
{
    setWindowTitle(tr("Settings"));
    setMinimumSize(settings_dialog::MINIMUM_WIDTH, settings_dialog::MINIMUM_HEIGHT);

    // Categories on the left, in the same order as the pages
    categories = new QListWidget(this);
    categories->addItem(tr("BIOS"));
    categories->addItem(tr("Game Boy"));
    categories->addItem(tr("Video"));
    categories->addItem(tr("Audio"));
    categories->setFixedWidth(settings_dialog::CATEGORY_LIST_WIDTH);

    pages = new QStackedWidget(this);
    pages->addWidget(createBIOSPage(settings));
    pages->addWidget(createGameBoyPage(settings));
    pages->addWidget(createVideoPage(settings));
    pages->addWidget(createAudioPage(settings));
    connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    categories->setCurrentRow(0);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { if (applyChanges()) accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] { applyChanges(); });

    QHBoxLayout *body = new QHBoxLayout;
    body->addWidget(categories);
    body->addWidget(pages, 1);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(body, 1);
    layout->addWidget(buttons);
}

void SettingsDialog::showPage(Page page)
{
    categories->setCurrentRow(static_cast<int>(page));
}

QWidget* SettingsDialog::createBIOSPage(const Settings &settings)
{
    QWidget *page = new QWidget(this);
    QGridLayout *grid = new QGridLayout(page);

    // One row per boot ROM: label, path, Browse button
    const auto addRow = [this, grid](int row, const QString &label, const QString &path, Console console)
    {
        QLineEdit *edit = new QLineEdit(QDir::toNativeSeparators(path));
        edit->setPlaceholderText(tr("No file chosen"));
        QPushButton *browse = new QPushButton(tr("Browse"));
        connect(browse, &QPushButton::clicked, this, [this, edit, console]
            {
                browseForBootROM(edit, console);
            });
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(edit, row, 1);
        grid->addWidget(browse, row, 2);
        return edit;
    };
    gameBoyBootROMEdit = addRow(0, tr("GB BIOS file:"), settings.gameBoyBootROM, Console::GameBoy);
    gameBoyColorBootROMEdit = addRow(1, tr("GBC BIOS file:"), settings.gameBoyColorBootROM, Console::GameBoyColor);
    superGameBoyBootROMEdit = addRow(2, tr("SGB BIOS file:"), settings.superGameBoyBootROM, Console::SuperGameBoy);

    QLabel *note = new QLabel(tr("gbemu does not include any BIOS. Every game starts by running the "
                                 "BIOS of the console it is emulated on, so choose your own dumps: "
                                 "a %1-byte Game Boy BIOS, a %2-byte Game Boy Color BIOS and a "
                                 "%1-byte Super Game Boy BIOS.")
                                  .arg(mmu::DMG_BOOT_ROM_SIZE).arg(mmu::CGB_BOOT_ROM_SIZE));
    note->setWordWrap(true);
    grid->addWidget(note, 3, 1, 1, 2);
    grid->setRowStretch(4, 1);
    return page;
}

QWidget* SettingsDialog::createGameBoyPage(const Settings &settings)
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);

    // Item order matches HardwareModel
    hardwareModelBox = new QComboBox;
    hardwareModelBox->addItem(tr("Automatic (Game Boy Color for color games, Super Game Boy for SGB games)"));
    hardwareModelBox->addItem(tr("Game Boy (DMG) for every game"));
    hardwareModelBox->addItem(tr("Game Boy Color for every game (Game Boy games in compatibility mode)"));
    hardwareModelBox->addItem(tr("Super Game Boy for every Game Boy game"));
    hardwareModelBox->setCurrentIndex(static_cast<int>(settings.hardwareModel));
    form->addRow(tr("Hardware:"), hardwareModelBox);

    QLabel *note = new QLabel(tr("Takes effect when a game starts. Some test ROMs, such as the OAM "
                                 "corruption tests, need the original Game Boy."));
    note->setWordWrap(true);
    form->addRow(QString(), note);
    return page;
}

QWidget* SettingsDialog::createVideoPage(const Settings &settings)
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);

    windowScaleBox = new QComboBox;
    for (int scale = video::MIN_WINDOW_SCALE; scale <= video::MAX_WINDOW_SCALE; ++scale)
    {
        windowScaleBox->addItem(tr("%1× (%2 × %3)").arg(scale)
            .arg(common::GAMEBOY_WIDTH * scale).arg(common::GAMEBOY_HEIGHT * scale), scale);
    }
    windowScaleBox->setCurrentIndex(windowScaleBox->findData(settings.windowScale));
    form->addRow(tr("Window size:"), windowScaleBox);

    // Item order matches ScreenScaling
    scalingBox = new QComboBox;
    scalingBox->addItem(tr("Fit the window, keeping the screen's shape"));
    scalingBox->addItem(tr("Whole multiples only (every pixel the same size)"));
    scalingBox->addItem(tr("Stretch to fill the window"));
    scalingBox->setCurrentIndex(static_cast<int>(settings.scaling));
    form->addRow(tr("Scaling:"), scalingBox);

    // Item order matches GameBoyColors
    gameBoyColorsBox = new QComboBox;
    gameBoyColorsBox->addItem(tr("Gray"));
    gameBoyColorsBox->addItem(tr("Green (original Game Boy screen)"));
    gameBoyColorsBox->addItem(tr("Pocket (Game Boy Pocket screen)"));
    gameBoyColorsBox->setCurrentIndex(static_cast<int>(settings.gameBoyColors));
    form->addRow(tr("Game Boy colors:"), gameBoyColorsBox);
    QLabel *colorsNote = new QLabel(tr("Used for games running on the original Game Boy. Game Boy Color "
                                       "and Super Game Boy games choose their own colors."));
    colorsNote->setWordWrap(true);
    form->addRow(QString(), colorsNote);

    superGameBoyBorderBox = new QCheckBox(tr("Show the Super Game Boy border"));
    superGameBoyBorderBox->setChecked(settings.showSuperGameBoyBorder);
    form->addRow(QString(), superGameBoyBorderBox);

    QLabel *note = new QLabel(tr("Video settings take effect straight away. View > Full Screen (F11) "
                                 "shows only the picture; Escape leaves it."));
    note->setWordWrap(true);
    form->addRow(QString(), note);
    return page;
}

QWidget* SettingsDialog::createAudioPage(const Settings &settings)
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);

    audioDeviceBox = new QComboBox;
    listAudioDevices(settings.audioDevice, settings.audioDeviceName);
    form->addRow(tr("Output device:"), audioDeviceBox);

    // Keep the list current as devices are plugged in and out
    QMediaDevices *devices = new QMediaDevices(page);
    connect(devices, &QMediaDevices::audioOutputsChanged, this, [this]
        {
            listAudioDevices(audioDeviceBox->currentData().toString(),
                             audioDeviceBox->currentData(settings_dialog::DEVICE_NAME_ROLE).toString());
        });

    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0, audio::MAX_VOLUME);
    volumeSlider->setValue(settings.volume);
    QLabel *volumeLabel = new QLabel;
    volumeLabel->setMinimumWidth(volumeLabel->fontMetrics().horizontalAdvance(QStringLiteral("100%")));
    const auto showVolume = [volumeLabel](int volume) { volumeLabel->setText(QStringLiteral("%1%").arg(volume)); };
    showVolume(settings.volume);
    connect(volumeSlider, &QSlider::valueChanged, volumeLabel, showVolume);
    QHBoxLayout *volumeRow = new QHBoxLayout;
    volumeRow->addWidget(volumeSlider, 1);
    volumeRow->addWidget(volumeLabel);
    form->addRow(tr("Volume:"), volumeRow);

    mutedBox = new QCheckBox(tr("Mute"));
    mutedBox->setChecked(settings.muted);
    form->addRow(QString(), mutedBox);

    QLabel *note = new QLabel(tr("Audio settings take effect straight away. If the chosen device is "
                                 "unplugged, the system default plays until it comes back."));
    note->setWordWrap(true);
    form->addRow(QString(), note);
    return page;
}

void SettingsDialog::listAudioDevices(const QString &selectedID, const QString &selectedName)
{
    audioDeviceBox->clear();
    audioDeviceBox->addItem(tr("System default"), QString());

    for (const QAudioDevice &device : QMediaDevices::audioOutputs())
    {
        audioDeviceBox->addItem(device.description(), QString::fromUtf8(device.id()));
        audioDeviceBox->setItemData(audioDeviceBox->count() - 1, device.description(),
                                    settings_dialog::DEVICE_NAME_ROLE);
    }

    int index = audioDeviceBox->findData(selectedID);
    if (index < 0)
    {
        audioDeviceBox->addItem(tr("%1 (not connected)").arg(selectedName.isEmpty() ? tr("Unknown device") : selectedName),
                                selectedID);
        index = audioDeviceBox->count() - 1;
        audioDeviceBox->setItemData(index, selectedName, settings_dialog::DEVICE_NAME_ROLE);
    }
    audioDeviceBox->setCurrentIndex(index);
}

void SettingsDialog::browseForBootROM(QLineEdit *edit, Console console)
{
    const QString current = edit->text().trimmed();
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Choose the %1 BIOS").arg(Settings::consoleName(console)),
        current.isEmpty() ? QDir::currentPath() : QFileInfo(current).absolutePath(),
        tr("BIOS files (*.bin *.rom *.gb *.gbc);;All files (*)"));
    if (!filename.isEmpty()) edit->setText(QDir::toNativeSeparators(filename));
}

Settings SettingsDialog::editedSettings() const
{
    Settings edited;
    edited.gameBoyBootROM = QDir::fromNativeSeparators(gameBoyBootROMEdit->text().trimmed());
    edited.gameBoyColorBootROM = QDir::fromNativeSeparators(gameBoyColorBootROMEdit->text().trimmed());
    edited.superGameBoyBootROM = QDir::fromNativeSeparators(superGameBoyBootROMEdit->text().trimmed());
    edited.hardwareModel = static_cast<HardwareModel>(hardwareModelBox->currentIndex());
    edited.windowScale = windowScaleBox->currentData().toInt();
    edited.scaling = static_cast<ScreenScaling>(scalingBox->currentIndex());
    edited.gameBoyColors = static_cast<GameBoyColors>(gameBoyColorsBox->currentIndex());
    edited.showSuperGameBoyBorder = superGameBoyBorderBox->isChecked();
    edited.audioDevice = audioDeviceBox->currentData().toString();
    edited.audioDeviceName = edited.audioDevice.isEmpty()
        ? QString() : audioDeviceBox->currentData(settings_dialog::DEVICE_NAME_ROLE).toString();
    edited.volume = volumeSlider->value();
    edited.muted = mutedBox->isChecked();
    return edited;
}

bool SettingsDialog::applyChanges()
{
    const Settings edited = editedSettings();

    // An empty path is fine (gbemu asks for it when a game needs it),
    // but a chosen file has to exist and be a BIOS of the right size
    const auto check = [this](const QString &path, const QString &name, size_t expectedSize)
    {
        if (path.isEmpty()) return true;
        const QFileInfo file(path);
        QString problem;
        if (!file.isFile()) problem = tr("The file does not exist.");
        else if (static_cast<size_t>(file.size()) != expectedSize)
            problem = tr("It should be %1 bytes, but it is %2 bytes.").arg(expectedSize).arg(file.size());
        if (problem.isEmpty()) return true;

        showPage(Page::BIOS);
        QMessageBox::warning(this, tr("Settings"), tr("%1 is not a usable %2 BIOS.\n\n%3")
            .arg(QDir::toNativeSeparators(path), name, problem));
        return false;
    };
    if (!check(edited.gameBoyBootROM, tr("Game Boy"), mmu::DMG_BOOT_ROM_SIZE)) return false;
    if (!check(edited.gameBoyColorBootROM, tr("Game Boy Color"), mmu::CGB_BOOT_ROM_SIZE)) return false;
    if (!check(edited.superGameBoyBootROM, tr("Super Game Boy"), mmu::DMG_BOOT_ROM_SIZE)) return false;

    return apply(edited);
}
