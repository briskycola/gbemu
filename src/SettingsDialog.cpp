#include "SettingsDialog.hpp"
#include "MMU.hpp"
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
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <utility>

namespace settings_dialog
{
    static constexpr int CATEGORY_LIST_WIDTH = 150;
    static constexpr int MINIMUM_WIDTH       = 640;
    static constexpr int MINIMUM_HEIGHT      = 360;
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
    categories->setFixedWidth(settings_dialog::CATEGORY_LIST_WIDTH);

    pages = new QStackedWidget(this);
    pages->addWidget(createBIOSPage(settings));
    pages->addWidget(createGameBoyPage(settings));
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
    const auto addRow = [this, grid](int row, const QString &label, const QString &path, bool gameBoyColor)
    {
        QLineEdit *edit = new QLineEdit(QDir::toNativeSeparators(path));
        edit->setPlaceholderText(tr("No file chosen"));
        QPushButton *browse = new QPushButton(tr("Browse"));
        connect(browse, &QPushButton::clicked, this, [this, edit, gameBoyColor]
            {
                browseForBootROM(edit, gameBoyColor);
            });
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(edit, row, 1);
        grid->addWidget(browse, row, 2);
        return edit;
    };
    gameBoyBootROMEdit = addRow(0, tr("GB BIOS file:"), settings.gameBoyBootROM, false);
    gameBoyColorBootROMEdit = addRow(1, tr("GBC BIOS file:"), settings.gameBoyColorBootROM, true);

    QLabel *note = new QLabel(tr("gbemu does not include any BIOS. Every game starts by running the "
                                 "BIOS of the console it is emulated on, so choose your own dumps: "
                                 "a %1-byte Game Boy BIOS and a %2-byte Game Boy Color BIOS.")
                                  .arg(mmu::DMG_BOOT_ROM_SIZE).arg(mmu::CGB_BOOT_ROM_SIZE));
    note->setWordWrap(true);
    grid->addWidget(note, 2, 1, 1, 2);
    grid->setRowStretch(3, 1);
    return page;
}

QWidget* SettingsDialog::createGameBoyPage(const Settings &settings)
{
    QWidget *page = new QWidget(this);
    QFormLayout *form = new QFormLayout(page);

    // Item order matches HardwareModel
    hardwareModelBox = new QComboBox;
    hardwareModelBox->addItem(tr("Automatic (Game Boy Color for color games)"));
    hardwareModelBox->addItem(tr("Game Boy (DMG) for every game"));
    hardwareModelBox->setCurrentIndex(static_cast<int>(settings.hardwareModel));
    form->addRow(tr("Hardware:"), hardwareModelBox);

    QLabel *note = new QLabel(tr("Takes effect when a game starts. Some test ROMs, such as the OAM "
                                 "corruption tests, need the original Game Boy."));
    note->setWordWrap(true);
    form->addRow(QString(), note);
    return page;
}

void SettingsDialog::browseForBootROM(QLineEdit *edit, bool gameBoyColor)
{
    const QString current = edit->text().trimmed();
    const QString filename = QFileDialog::getOpenFileName(
        this, gameBoyColor ? tr("Choose the Game Boy Color BIOS") : tr("Choose the Game Boy BIOS"),
        current.isEmpty() ? QDir::currentPath() : QFileInfo(current).absolutePath(),
        tr("BIOS files (*.bin *.rom *.gb *.gbc);;All files (*)"));
    if (!filename.isEmpty()) edit->setText(QDir::toNativeSeparators(filename));
}

Settings SettingsDialog::editedSettings() const
{
    Settings edited;
    edited.gameBoyBootROM = QDir::fromNativeSeparators(gameBoyBootROMEdit->text().trimmed());
    edited.gameBoyColorBootROM = QDir::fromNativeSeparators(gameBoyColorBootROMEdit->text().trimmed());
    edited.hardwareModel = static_cast<HardwareModel>(hardwareModelBox->currentIndex());
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

    return apply(edited);
}
