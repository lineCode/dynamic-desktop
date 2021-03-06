#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include "settingsmanager.h"

#ifdef QT_HAS_WINEXTRAS
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif
#include <QFileDialog>
#include <QInputDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QIcon>
#include <QDragEnterEvent>
#include <QUrl>
#include <QMimeDatabase>
#include <QMimeData>
#include <QTextCodec>
#ifndef BUILD_DD_STATIC
#include <QLibraryInfo>
#endif

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    FramelessWindow(parent),
    ui(new Ui::PreferencesDialog)
{
    ui->setupUi(this);
    setContentsMargins(0, 0, 0, 0);
    setResizeable(true);
    setResizeableAreaWidth(5);
    setTitleBar(ui->widget_windowTitleBar);
    addIgnoreWidget(ui->label_windowTitle);
#ifdef QT_HAS_WINEXTRAS
    taskbarButton = new QWinTaskbarButton(this);
    taskbarButton->setWindow(windowHandle());
    taskbarProgress = taskbarButton->progress();
    taskbarProgress->setRange(0, 99);
    taskbarProgress->show();
    connect(ui->horizontalSlider_video_position, SIGNAL(valueChanged(int)), taskbarProgress, SLOT(setValue(int)));
    connect(ui->horizontalSlider_video_position, SIGNAL(rangeChanged(int, int)), taskbarProgress, SLOT(setRange(int, int)));
#endif
    ui->comboBox_video_track->setEnabled(false);
    ui->comboBox_audio_track->setEnabled(false);
    ui->comboBox_subtitle_track->setEnabled(false);
    ui->comboBox_image_quality->addItem(tr("Best"), QStringLiteral("best"));
    ui->comboBox_image_quality->addItem(tr("Fastest"), QStringLiteral("fastest"));
    ui->comboBox_image_quality->addItem(tr("Default"), QStringLiteral("default"));
    ui->comboBox_video_renderer->addItem(QStringLiteral("OpenGLWidget"), QtAV::VideoRendererId_OpenGLWidget);
    ui->comboBox_video_renderer->addItem(QStringLiteral("QGLWidget2"), QtAV::VideoRendererId_GLWidget2);
    ui->comboBox_video_renderer->addItem(QStringLiteral("Widget"), QtAV::VideoRendererId_Widget);
    ui->comboBox_video_renderer->addItem(QStringLiteral("GDI"), QtAV::VideoRendererId_GDI);
    ui->comboBox_video_renderer->addItem(QStringLiteral("Direct2D"), QtAV::VideoRendererId_Direct2D);
    ui->comboBox_skin->addItem(tr("<None>"), QStringLiteral("none"));
#ifndef BUILD_DD_STATIC
    QString skinDirPath = QApplication::applicationDirPath() + QDir::separator() + QStringLiteral("skins");
#else
    QString skinDirPath = QStringLiteral(":/skins");
#endif
    QDir skinDir(skinDirPath);
    QFileInfoList skinFileList = skinDir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    if (skinFileList.count() > 0)
        for (auto& skinFile : skinFileList)
            ui->comboBox_skin->addItem(skinFile.completeBaseName(), skinFile.completeBaseName());
    if (ui->comboBox_skin->count() > 1)
        ui->comboBox_skin->setEnabled(true);
    else
        ui->comboBox_skin->setEnabled(false);
#ifndef BUILD_DD_STATIC
    QString languageDirPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#else
    QString languageDirPath = QStringLiteral(":/i18n");
#endif
    QDir languageDir(languageDirPath);
    QFileInfoList languageFileList = languageDir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    if (languageFileList.count() > 0)
        for (auto& languageFile : languageFileList)
        {
            QString fileName = languageFile.completeBaseName();
            if (fileName.startsWith(QStringLiteral("qt"), Qt::CaseInsensitive))
                continue;
            QString lang = fileName.mid(fileName.indexOf(QLatin1Char('_')) + 1);
            lang = lang.replace('-', '_');
            QLocale locale(lang);
            ui->comboBox_language->addItem(locale.nativeLanguageName(), lang);
        }
    if (ui->comboBox_language->count() > 0)
    {
        ui->comboBox_language->insertItem(0, tr("Auto"), QStringLiteral("auto"));
        ui->comboBox_language->setEnabled(true);
    }
    else
    {
        ui->comboBox_language->addItem(tr("<None>"), QStringLiteral("en"));
        ui->comboBox_language->setEnabled(false);
    }
    connect(ui->pushButton_audio_open, &QPushButton::clicked,
        [=]
        {
            QString audioPath = QFileDialog::getOpenFileName(nullptr, tr("Please select an audio file"), SettingsManager::getInstance()->lastDir(), tr("Audios (*.mka *.aac *.flac *.mp3 *.wav);;All files (*)"));
            if (!audioPath.isEmpty())
                emit this->audioOpened(audioPath);
        });
    connect(ui->pushButton_subtitle_open, &QPushButton::clicked,
        [=]
        {
            QString subtitlePath = QFileDialog::getOpenFileName(nullptr, tr("Please select a subtitle file"), SettingsManager::getInstance()->lastDir(), tr("Subtitles (*.ass *.ssa *.srt *.sub);;All files (*)"));
            if (!subtitlePath.isEmpty())
                emit this->subtitleOpened(subtitlePath);
        });
    connect(this, &PreferencesDialog::clearAllTracks,
        [=]
        {
            ui->comboBox_video_track->clear();
            ui->comboBox_audio_track->clear();
            ui->comboBox_subtitle_track->clear();
        });
    connect(this, &PreferencesDialog::updateVideoTracks,
        [=](const QVariantList &videoTracks)
        {
            if (!videoTracks.isEmpty())
            {
                ui->comboBox_video_track->clear();
                for (auto& track : videoTracks)
                {
                    QVariantMap trackData = track.toMap();
                    unsigned int id = trackData[QStringLiteral("id")].toUInt();
                    QString lang = trackData[QStringLiteral("language")].toString();
                    QString title = trackData[QStringLiteral("title")].toString();
                    QString txt = tr("ID: %0 | Title: %1 | Language: %2")
                            .arg(id).arg(title).arg(lang);
                    ui->comboBox_video_track->addItem(txt, id);
                }
            }
            if (ui->comboBox_video_track->count() > 0)
                ui->comboBox_video_track->setEnabled(true);
            else
                ui->comboBox_video_track->setEnabled(false);
        });
    connect(this, &PreferencesDialog::updateAudioTracks,
        [=](const QVariantList &audioTracks, bool add)
        {
            if (!audioTracks.isEmpty())
            {
                if (!add)
                    ui->comboBox_audio_track->clear();
                for (auto& track : audioTracks)
                {
                    QVariantMap trackData = track.toMap();
                    unsigned int id = trackData[QStringLiteral("id")].toUInt();
                    QString lang = trackData[QStringLiteral("language")].toString();
                    QString title = trackData[QStringLiteral("title")].toString();
                    QString txt = tr("ID: %0 | Title: %1 | Language: %2")
                            .arg(id).arg(title).arg(lang);
                    ui->comboBox_audio_track->addItem(txt, id);
                }
            }
            if (ui->comboBox_audio_track->count() > 0)
                ui->comboBox_audio_track->setEnabled(true);
            else
                ui->comboBox_audio_track->setEnabled(false);
        });
    connect(this, &PreferencesDialog::updateSubtitleTracks,
        [=](const QVariantList &subtitleTracks, bool add)
        {
            if (!subtitleTracks.isEmpty())
            {
                if (!add)
                    ui->comboBox_subtitle_track->clear();
                for (auto& track : subtitleTracks)
                {
                    QVariantMap trackData = track.toMap();
                    if (!add)
                    {
                        unsigned int id = trackData[QStringLiteral("id")].toUInt();
                        QString lang = trackData[QStringLiteral("language")].toString();
                        QString title = trackData[QStringLiteral("title")].toString();
                        QString txt = tr("ID: %0 | Title: %1 | Language: %2")
                                .arg(id).arg(title).arg(lang);
                        ui->comboBox_subtitle_track->addItem(txt, id);
                    }
                    else
                    {
                        QString file = trackData[QStringLiteral("file")].toString();
                        QString txt = tr("File: %0").arg(QFileInfo(file).fileName());
                        ui->comboBox_subtitle_track->addItem(txt, file);
                    }
                }
            }
            if (ui->comboBox_subtitle_track->count() > 0)
                ui->comboBox_subtitle_track->setEnabled(true);
            else
                ui->comboBox_subtitle_track->setEnabled(false);
        });
    connect(this, &PreferencesDialog::updateVolumeArea,
        [=]
        {
            if (audioAvailable)
            {
                ui->checkBox_volume->setChecked(!SettingsManager::getInstance()->getMute());
                ui->horizontalSlider_volume->setEnabled(ui->checkBox_volume->isChecked());
                ui->horizontalSlider_volume->setValue(SettingsManager::getInstance()->getVolume());
            }
        });
    connect(this, &PreferencesDialog::setVolumeAreaEnabled,
        [=](bool enabled)
        {
            ui->checkBox_volume->setEnabled(audioAvailable && enabled);
            ui->horizontalSlider_volume->setEnabled(audioAvailable && enabled && ui->checkBox_volume->isChecked());
        });
    connect(ui->horizontalSlider_volume, &QSlider::sliderMoved,
        [=](int value)
        {
            int vol = value;
            if (vol > 99)
                vol = 99;
            if (vol < 0)
                vol = 0;
            if (static_cast<unsigned int>(vol) != SettingsManager::getInstance()->getVolume())
            {
                SettingsManager::getInstance()->setVolume(static_cast<unsigned int>(vol));
                emit this->volumeChanged(SettingsManager::getInstance()->getVolume());
            }
        });
    connect(this, &PreferencesDialog::setAudioAreaEnabled,
        [=](bool audioAvailable)
        {
            this->audioAvailable = audioAvailable;
            ui->groupBox_audio->setEnabled(audioAvailable);
        });
    connect(this, &PreferencesDialog::setSeekAreaEnabled,
        [=](bool enabled)
        {
            ui->horizontalSlider_video_position->setEnabled(enabled);
        });
    connect(ui->horizontalSlider_video_position, &QSlider::sliderMoved,
        [=](int value)
        {
            emit this->seekBySlider(static_cast<qint64>(value * sliderUnit));
        });
    connect(this, &PreferencesDialog::updateVideoSliderRange,
        [=](qint64 duration)
        {
            ui->horizontalSlider_video_position->setRange(0, static_cast<int>(duration / sliderUnit));
        });
    connect(this, &PreferencesDialog::updateVideoSliderUnit,
        [=](int unit)
        {
            sliderUnit = unit;
        });
    connect(this, &PreferencesDialog::updateVideoSlider,
        [=](qint64 position)
        {
            ui->horizontalSlider_video_position->setValue(static_cast<int>(position / sliderUnit));
        });
    connect(ui->pushButton_about, SIGNAL(clicked()), this, SIGNAL(about()));
    connect(ui->pushButton_minimize, SIGNAL(clicked()), this, SLOT(showMinimized()));
    connect(ui->pushButton_close, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->pushButton_maximize, &QPushButton::clicked,
        [=]
        {
            if (this->isMaximized())
                this->showNormal();
            else
                this->showMaximized();
        });
    connect(ui->checkBox_volume, &QCheckBox::stateChanged,
        [=]
        {
            ui->horizontalSlider_volume->setEnabled(ui->checkBox_volume->isChecked());
            if (ui->checkBox_volume->isChecked() != !SettingsManager::getInstance()->getMute())
            {
                SettingsManager::getInstance()->setMute(!ui->checkBox_volume->isChecked());
                emit this->muteChanged(SettingsManager::getInstance()->getMute());
            }
        });
    connect(ui->pushButton_play, &QPushButton::clicked,
        [=]
        {
            if (ui->lineEdit_url->text() != SettingsManager::getInstance()->getUrl())
                emit this->urlChanged(ui->lineEdit_url->text());
            else
                emit this->urlChanged(QString());
#ifdef QT_HAS_WINEXTRAS
            if (!taskbarProgress->isVisible())
                taskbarProgress->show();
            taskbarProgress->resume();
#endif
        });
    connect(ui->pushButton_pause, &QPushButton::clicked,
        [=]
        {
#ifdef QT_HAS_WINEXTRAS
            taskbarProgress->pause();
#endif
            emit this->pause();
        });
    connect(ui->pushButton_cancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->pushButton_url_browse, &QPushButton::clicked,
        [=]
        {
            QString path = QFileDialog::getOpenFileName(nullptr, tr("Please select a media file"), SettingsManager::getInstance()->lastDir(), tr("Videos (*.avi *.mp4 *.mkv *.flv);;Audios (*.mp3 *.flac *.ape *.wav);;Pictures (*.bmp *.jpg *.jpeg *.png *.gif);;All files (*)"));
            if (!path.isEmpty())
                ui->lineEdit_url->setText(QDir::toNativeSeparators(path));
        });
    connect(ui->pushButton_url_input, &QPushButton::clicked,
        [=]
        {
            bool ok = false;
            QString input = QInputDialog::getText(nullptr, tr("Please input a valid URL"), tr("URL"), QLineEdit::Normal, QStringLiteral("https://"), &ok);
            if (ok && !input.isEmpty())
            {
                QUrl url(input);
                if (url.isValid())
                {
                    if (url.isLocalFile())
                        ui->lineEdit_url->setText(url.toLocalFile());
                    else
                        ui->lineEdit_url->setText(url.url());
                }
                else
                    QMessageBox::warning(nullptr, QStringLiteral("Dynamic Desktop"), tr("\"%0\" is not a valid URL.").arg(input));
            }
        });
    connect(ui->checkBox_hwdec, &QCheckBox::stateChanged,
        [=]
        {
            bool hwdecEnabled = ui->checkBox_hwdec->isChecked();
            ui->checkBox_hwdec_cuda->setEnabled(hwdecEnabled);
            ui->checkBox_hwdec_d3d11->setEnabled(hwdecEnabled);
            ui->checkBox_hwdec_dxva->setEnabled(hwdecEnabled);
            this->setDecoders();
        });
    connect(ui->checkBox_hwdec_cuda, &QCheckBox::stateChanged,
        [=]
        {
            this->setDecoders();
        });
    connect(ui->checkBox_hwdec_d3d11, &QCheckBox::stateChanged,
        [=]
        {
            this->setDecoders();
        });
    connect(ui->checkBox_hwdec_dxva, &QCheckBox::stateChanged,
        [=]
        {
            this->setDecoders();
        });
    ui->comboBox_subtitle_charset->addItem(tr("Auto detect"), QStringLiteral("AutoDetect"));
    ui->comboBox_subtitle_charset->addItem(tr("System"), QStringLiteral("System"));
    for (auto& codec : QTextCodec::availableCodecs())
        ui->comboBox_subtitle_charset->addItem(QString::fromLatin1(codec), QString::fromLatin1(codec));
    connect(ui->comboBox_skin, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            if (ui->comboBox_skin->currentData().toString() != SettingsManager::getInstance()->getSkin())
            {
                SettingsManager::getInstance()->setSkin(ui->comboBox_skin->currentData().toString());
                emit this->skinChanged(SettingsManager::getInstance()->getSkin());
            }
        });
    connect(ui->comboBox_language, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            if (ui->comboBox_language->currentData().toString() != SettingsManager::getInstance()->getLanguage())
            {
                SettingsManager::getInstance()->setLanguage(ui->comboBox_language->currentData().toString());
                QMessageBox::information(nullptr, QStringLiteral("Dynamic Desktop"), tr("You have changed the UI translation. Application restart is needed."));
            }
        });
    connect(ui->comboBox_video_track, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            emit this->videoTrackChanged(ui->comboBox_video_track->currentData().toUInt());
        });
    connect(ui->comboBox_audio_track, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            emit this->audioTrackChanged(ui->comboBox_audio_track->currentData().toUInt());
        });
    connect(ui->comboBox_subtitle_track, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            emit this->subtitleTrackChanged(ui->comboBox_subtitle_track->currentData());
        });
    connect(ui->comboBox_video_renderer, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            if (ui->comboBox_video_renderer->currentData().toInt() != SettingsManager::getInstance()->getRenderer())
            {
                SettingsManager::getInstance()->setRenderer(ui->comboBox_video_renderer->currentData().toInt());
                emit this->rendererChanged(SettingsManager::getInstance()->getRenderer());
            }
        });
    connect(ui->comboBox_image_quality, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            if (ui->comboBox_image_quality->currentData().toString() != SettingsManager::getInstance()->getImageQuality())
            {
                SettingsManager::getInstance()->setImageQuality(ui->comboBox_image_quality->currentData().toString());
                emit this->imageQualityChanged(SettingsManager::getInstance()->getImageQuality());
            }
        });
    connect(ui->lineEdit_url, &QLineEdit::textChanged,
        [=](const QString &text)
        {
            if (text != SettingsManager::getInstance()->getUrl())
            {
                SettingsManager::getInstance()->setUrl(text);
                emit this->urlChanged(SettingsManager::getInstance()->getUrl());
            }
        });
    connect(ui->checkBox_autoStart, &QCheckBox::stateChanged,
        [=]
        {
            if (ui->checkBox_autoStart->isChecked() && !SettingsManager::getInstance()->isAutoStart())
                SettingsManager::getInstance()->setAutoStart(true);
            else if (!ui->checkBox_autoStart->isChecked() && SettingsManager::getInstance()->isAutoStart())
                SettingsManager::getInstance()->setAutoStart(false);
        });
    connect(ui->radioButton_ratio_fitDesktop, &QRadioButton::clicked,
        [=]
        {
            this->setRatio();
        });
    connect(ui->radioButton_ratio_videoAspectRatio, &QRadioButton::clicked,
        [=]
        {
            this->setRatio();
        });
    connect(ui->comboBox_subtitle_charset, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        [=](int index)
        {
            Q_UNUSED(index)
            if (ui->comboBox_subtitle_charset->currentData().toString() != SettingsManager::getInstance()->getCharset())
            {
                SettingsManager::getInstance()->setCharset(ui->comboBox_subtitle_charset->currentData().toString());
                emit this->charsetChanged(SettingsManager::getInstance()->getCharset());
            }
        });
    connect(ui->checkBox_subtitle_autoLoadExternal, &QCheckBox::stateChanged,
        [=]
        {
            if (ui->checkBox_subtitle_autoLoadExternal->isChecked() != SettingsManager::getInstance()->getSubtitleAutoLoad())
            {
                SettingsManager::getInstance()->setSubtitleAutoLoad(ui->checkBox_subtitle_autoLoadExternal->isChecked());
                emit this->subtitleAutoLoadChanged(SettingsManager::getInstance()->getSubtitleAutoLoad());
            }
        });
    connect(ui->checkBox_displaySubtitle, &QCheckBox::stateChanged,
        [=]
        {
            if (ui->checkBox_displaySubtitle->isChecked() != SettingsManager::getInstance()->getSubtitle())
            {
                SettingsManager::getInstance()->setSubtitle(ui->checkBox_displaySubtitle->isChecked());
                emit this->subtitleEnabled(SettingsManager::getInstance()->getSubtitle());
            }
        });
    connect(ui->checkBox_audio_autoLoadExternal, &QCheckBox::stateChanged,
        [=]
        {
            if (ui->checkBox_audio_autoLoadExternal->isChecked() != SettingsManager::getInstance()->getAudioAutoLoad())
                SettingsManager::getInstance()->setAudioAutoLoad(ui->checkBox_audio_autoLoadExternal->isChecked());
        });
    connect(this, &PreferencesDialog::setVolumeToolTip,
        [=](const QString &text)
        {
            if (!text.isEmpty())
            {
                ui->checkBox_volume->setToolTip(text);
                ui->horizontalSlider_volume->setToolTip(text);
            }
        });
    connect(this, &PreferencesDialog::setVideoPositionText,
        [=](const QString &text)
        {
            if (!text.isEmpty())
                ui->label_video_position->setText(text);
        });
    connect(this, &PreferencesDialog::setVideoDurationText,
        [=](const QString &text)
        {
            if (!text.isEmpty())
                ui->label_video_duration->setText(text);
        });
    refreshUI();
}

PreferencesDialog::~PreferencesDialog()
{
    delete ui;
}

void PreferencesDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
        if (windowState() == Qt::WindowMaximized)
            ui->pushButton_maximize->setIcon(QIcon(QStringLiteral(":/restore.ico")));
        else
            ui->pushButton_maximize->setIcon(QIcon(QStringLiteral(":/maximize.ico")));
    FramelessWindow::changeEvent(event);
}

static bool canHandleDrop(const QDragEnterEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.empty()) return false;
    QMimeDatabase mimeDatabase;
    return SettingsManager::getInstance()->supportedMimeTypes().
        contains(mimeDatabase.mimeTypeForUrl(urls.constFirst()).name());
}

void PreferencesDialog::dragEnterEvent(QDragEnterEvent *event)
{
    event->setAccepted(canHandleDrop(event));
    FramelessWindow::dragEnterEvent(event);
}

void PreferencesDialog::dropEvent(QDropEvent *event)
{
    event->accept();
    QUrl url = event->mimeData()->urls().constFirst();
    QString path;
    if (url.isLocalFile())
        path = QDir::toNativeSeparators(url.toLocalFile());
    else
        path = url.url();
    ui->lineEdit_url->setText(path);
    FramelessWindow::dropEvent(event);
}

void PreferencesDialog::refreshUI()
{
    ui->lineEdit_url->setText(SettingsManager::getInstance()->getUrl());
    if (audioAvailable)
    {
        ui->checkBox_volume->setChecked(!SettingsManager::getInstance()->getMute());
        ui->horizontalSlider_volume->setEnabled(ui->checkBox_volume->isChecked());
        ui->horizontalSlider_volume->setValue(SettingsManager::getInstance()->getVolume());
    }
    ui->checkBox_autoStart->setChecked(SettingsManager::getInstance()->isAutoStart());
    QStringList decoders = SettingsManager::getInstance()->getDecoders();
    ui->checkBox_hwdec_cuda->setChecked(decoders.contains(QStringLiteral("CUDA")));
    ui->checkBox_hwdec_d3d11->setChecked(decoders.contains(QStringLiteral("D3D11")));
    ui->checkBox_hwdec_dxva->setChecked(decoders.contains(QStringLiteral("DXVA")));
    bool hwdecEnabled = (ui->checkBox_hwdec_cuda->isChecked()
            || ui->checkBox_hwdec_d3d11->isChecked()
            || ui->checkBox_hwdec_dxva->isChecked())
            && SettingsManager::getInstance()->getHwdec();
    ui->checkBox_hwdec->setChecked(hwdecEnabled);
    ui->checkBox_hwdec_cuda->setEnabled(hwdecEnabled);
    ui->checkBox_hwdec_d3d11->setEnabled(hwdecEnabled);
    ui->checkBox_hwdec_dxva->setEnabled(hwdecEnabled);
    ui->radioButton_ratio_fitDesktop->setChecked(SettingsManager::getInstance()->getFitDesktop());
    ui->radioButton_ratio_videoAspectRatio->setChecked(!ui->radioButton_ratio_fitDesktop->isChecked());
    int i = ui->comboBox_subtitle_charset->findData(SettingsManager::getInstance()->getCharset());
    ui->comboBox_subtitle_charset->setCurrentIndex(i > -1 ? i : 0);
    ui->checkBox_subtitle_autoLoadExternal->setChecked(SettingsManager::getInstance()->getSubtitleAutoLoad());
    ui->checkBox_displaySubtitle->setChecked(SettingsManager::getInstance()->getSubtitle());
    ui->checkBox_audio_autoLoadExternal->setChecked(SettingsManager::getInstance()->getAudioAutoLoad());
    i = ui->comboBox_skin->findData(SettingsManager::getInstance()->getSkin());
    ui->comboBox_skin->setCurrentIndex(i > -1 ? i : 0);
    i = ui->comboBox_language->findData(SettingsManager::getInstance()->getLanguage());
    ui->comboBox_language->setCurrentIndex(i > -1 ? i : 0);
    i = ui->comboBox_video_renderer->findData(SettingsManager::getInstance()->getRenderer());
    ui->comboBox_video_renderer->setCurrentIndex(i > -1 ? i : 0);
    i = ui->comboBox_image_quality->findData(SettingsManager::getInstance()->getImageQuality());
    ui->comboBox_image_quality->setCurrentIndex(i > -1 ? i : 0);
}

void PreferencesDialog::setDecoders()
{
    QStringList decoders;
    if (ui->checkBox_hwdec_cuda->isChecked())
        decoders << QStringLiteral("CUDA");
    if (ui->checkBox_hwdec_d3d11->isChecked())
        decoders << QStringLiteral("D3D11");
    if (ui->checkBox_hwdec_dxva->isChecked())
        decoders << QStringLiteral("DXVA");
    if (decoders.isEmpty())
        ui->checkBox_hwdec->setChecked(false);
    if (ui->checkBox_hwdec->isChecked() != SettingsManager::getInstance()->getHwdec())
        SettingsManager::getInstance()->setHwdec(ui->checkBox_hwdec->isChecked());
    decoders << QStringLiteral("FFmpeg");
    if (decoders != SettingsManager::getInstance()->getDecoders())
        SettingsManager::getInstance()->setDecoders(decoders);
    if (ui->checkBox_hwdec->isChecked() != SettingsManager::getInstance()->getHwdec())
        SettingsManager::getInstance()->setHwdec(ui->checkBox_hwdec->isChecked());
    if (isVisible() && isActiveWindow())
        QMessageBox::information(nullptr, QStringLiteral("Dynamic Desktop"), tr("Reopen this video or play another video to experience it.\nMake sure this application runs in your GPU's Optimus mode."));
}

void PreferencesDialog::setRatio()
{
    if (ui->radioButton_ratio_fitDesktop->isChecked() != SettingsManager::getInstance()->getFitDesktop())
    {
        SettingsManager::getInstance()->setFitDesktop(ui->radioButton_ratio_fitDesktop->isChecked());
        emit pictureRatioChanged(SettingsManager::getInstance()->getFitDesktop());
    }
}
