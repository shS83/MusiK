#define main musik_application_main
#include "../src/main.cpp"
#undef main
#include <QTemporaryDir>
#include <QDataStream>
#include <QThread>

static void require(bool ok,const char *what){if(!ok){std::fprintf(stderr,"FAILED %s\n",what);std::exit(1);}}
static void waitFor(int ms){
    QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();
}
static void wave(QString path){
    QFile file(path);require(file.open(QIODevice::WriteOnly),"create WAV");
    QDataStream out(&file);out.setByteOrder(QDataStream::LittleEndian);
    QByteArray silence(22050*2*3,0);
    out.writeRawData("RIFF",4);out<<quint32(36+silence.size());out.writeRawData("WAVEfmt ",8);
    out<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(22050)<<quint32(44100)<<quint16(2)<<quint16(16);
    out.writeRawData("data",4);out<<quint32(silence.size());file.write(silence);
}
static void testAudio(const QString &file){
    auto device=QMediaDevices::defaultAudioOutput();
    if(device.isNull()){qInfo("PCM hardware test skipped: no endpoint");return;}
    QMediaPlayer player;
    QAudioOutput volume;volume.setMuted(true);
    QAudioFormat received;
    PlaybackOutput route(&player,&volume,[&](const QAudioBuffer &b){if(b.isValid())received=b.format();},
                         [](QString message){qWarning("%s",qPrintable(message));});
    for(auto width:{"16","24"}){
        OutputChoice c{device.id(),width,44100};
        if(!device.isFormatSupported(c.format(device))){qInfo("PCM %s skipped: unsupported",width);continue;}
        route.configure(c);
        player.setSource(QUrl::fromLocalFile(file));player.play();
        auto *sink=route.findChild<QAudioSink*>();
        for(int n=0;n<100 && route.findChild<QAudioSink*>() && !sink->processedUSecs();++n)waitFor(25);
        require(route.findChild<QAudioSink*>() && sink->processedUSecs()>0,"PCM device did not consume samples");
        require(received==c.format(device) && sink->volume()==0,"wrong PCM format or volume");
        qInfo("PCM %s-bit at 44100 Hz verified",width);
        player.pause();waitFor(30);player.play();waitFor(30);player.stop();
    }
    route.configure({});require(player.audioOutput()==&volume,"restore automatic output");
}
static void testAudioTab(PlayerWindow &window){
    QTimer::singleShot(0,[]{
        auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        require(dialog,"settings dialog");
        auto *tabs=dialog->findChild<QTabWidget*>();
        require(tabs && tabs->tabText(0)=="Player" && tabs->tabText(1)=="Audio","settings tabs");
        tabs->setCurrentIndex(1);
        auto *form=qobject_cast<QFormLayout*>(tabs->currentWidget()->layout());
        require(form && form->rowCount()==3,"audio controls");
        auto *box=qobject_cast<QComboBox*>(form->itemAt(0,QFormLayout::FieldRole)->widget());
        require(box && box->itemText(0)=="System default","default output");
        for(const auto &device:QMediaDevices::audioOutputs()){
            int index=box->findData(device.id());
            require(index>=1 && box->itemText(index)==device.description(),"missing endpoint");
            box->setCurrentIndex(index);
            qInfo("Audio tab verified: %s",qPrintable(box->currentText()));
        }
        dialog->reject();
    });
    for(auto *button:window.findChildren<QPushButton*>()){
        if(button->text()=="Settings"){button->click();return;}
    }
    require(false,"settings button");
}
static void testSearch(PlayerWindow &window,const QString &root){
    auto *list=window.findChild<QListWidget*>("playlist");
    auto *search=window.findChild<QLineEdit*>("fzfSearch");
    list->clear();
    QDir(root).mkpath("Satoshi Tomiie/Disc 1");
    const auto hit=root+"/Satoshi Tomiie/Disc 1/Numb.wav";
    const auto miss=root+"/Stars At The Old House Shine Inside.wav";
    wave(hit);wave(miss);
    window.addFile(miss);
    search->setText("SATOSHI");
    window.addFile(hit);
    require(list->item(0)->isHidden() && !list->item(1)->isHidden(),"artist directory search");
    search->setText("tom  sat numb");
    require(!list->item(1)->isHidden(),"multi-term partial match");
    search->setText("not-in-playlist");
    require(list->item(0)->isHidden() && list->item(1)->isHidden(),"empty results");
    search->clear();
    require(!list->item(0)->isHidden() && !list->item(1)->isHidden(),"clear search");
}
static void testBulkPlaylist(PlayerWindow &window,const QString &root){
    // Regression: importing many entries must not repeatedly renumber/re-scan
    // the complete playlist. Also check that duplicate paths are ignored.
    auto *list=window.findChild<QListWidget*>("playlist");
    list->clear();
    const QString folder=root+"/bulk-playlist";
    require(QDir().mkpath(folder),"create bulk-playlist fixture directory");
    const QString m3u=folder+"/bulk.m3u8";
    QFile playlist(m3u);
    require(playlist.open(QIODevice::WriteOnly|QIODevice::Text),"create bulk playlist");
    playlist.write("#EXTM3U\n");
    constexpr int tracks=512;
    for(int i=0;i<tracks;++i){
        const QString name=QString("track-%1.flac").arg(i,4,10,QChar('0'));
        QFile audio(folder+"/"+name);
        require(audio.open(QIODevice::WriteOnly),"create bulk fixture track");
        audio.write("x");
        audio.close();
        playlist.write((name+"\n").toUtf8());
    }
    playlist.write("track-0000.flac\n"); // duplicate
    playlist.write("missing.flac\n");    // nonexistent
    playlist.close();

    window.loadList(m3u);
    require(list->count()==tracks,"bulk playlist count");
    require(list->item(0)->text().contains("track-0000"),"bulk playlist first item");
    require(list->item(tracks-1)->text().contains("track-0511"),"bulk playlist last item");
    window.loadList(m3u);
    require(list->count()==tracks,"duplicate import remains idempotent");
    list->clear();
}

static void testTrackInformation(PlayerWindow &window,const QString &file){
    auto *list=window.findChild<QListWidget*>("playlist");
    list->clear();
    window.addFile(file);
    window.show();
    waitFor(30);
    const auto player=window.findChild<QMediaPlayer*>();
    const auto original=player->source();
    auto openFromMenu=[&]{
        QTimer::singleShot(0,[]{
            auto *menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
            require(menu && menu->actions().first()->text()=="Track information","playlist context action");
            menu->setActiveAction(menu->actions().first());
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
            QApplication::sendEvent(menu,&enter);
        });
        list->customContextMenuRequested(list->visualItemRect(list->item(0)).center());
        auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        require(dialog && dialog->windowTitle().contains("Track information"),"track information window");
        return dialog;
    };
    auto verifyCover=[&](QDialog *dialog,const QString &expected){
        auto *source=dialog->findChild<QLabel*>("coverSource");
        require(source,"cover source");
        for(int i=0;i<100 && source->text()!=expected;++i)waitFor(25);
        require(source->text()==expected,"wrong cover source");
        bool image=false;
        for(auto *label:dialog->findChildren<QLabel*>())if(!label->pixmap().isNull())image=true;
        require(image==(expected!="No cover image in this folder"),"cover display");
        require(player->source()==original,"metadata changed playback source");
        dialog->reject();
        waitFor(10);
    };
    verifyCover(openFromMenu(),"No cover image in this folder");
    QImage art(25,25,QImage::Format_RGB32);art.fill(Qt::red);
    require(art.save(QFileInfo(file).dir().filePath("cover.png")),"folder cover fixture");
    verifyCover(openFromMenu(),"cover.png");
    window.hide();
}
static void testFireworks(){
    auto v=std::make_unique<Visual>();v->resize(480,320);v->setEffect(8);v->show();waitFor(100);
    auto key=[&](int code){QKeyEvent e(QEvent::KeyPress,code,Qt::NoModifier);QApplication::sendEvent(v.get(),&e);};
    key(Qt::Key_section);
    for(int n=1;n<=9;++n){key(Qt::Key_0+n);waitFor(50);}
    auto img=v->grabFramebuffer();int bright=0;
    for(int y=0;y<img.height();++y)for(int x=0;x<img.width();++x)if(img.pixelColor(x,y).value()>60)++bright;
    require(bright>100,"Fireworks GPU frame is empty");
}
int main(int argc,char **argv){
    QSurfaceFormat fmt;fmt.setVersion(3,3);fmt.setProfile(QSurfaceFormat::CoreProfile);QSurfaceFormat::setDefaultFormat(fmt);
    QApplication app(argc,argv);
    QTemporaryDir tmp;require(tmp.isValid(),"temporary directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());
    QStandardPaths::setTestModeEnabled(true);
    QSettings prefs(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
    require(prefs.fileName().startsWith(tmp.path()),"settings isolation");
    QStringList paths;
    for(auto album:{"A","B","C"}){
        QDir(tmp.path()).mkdir(album);
        for(auto track:{"1","2"}){QString p=tmp.path()+"/"+album+"/"+track+".wav";wave(p);paths<<p;}
    }
    for(int mode:{2,3,4}){
        prefs.setValue("order",mode);prefs.sync();
        auto window=std::make_unique<PlayerWindow>();
        if(mode==2){testTrackInformation(*window,paths[0]);testAudioTab(*window);testSearch(*window,tmp.path());testBulkPlaylist(*window,tmp.path());}
        auto *list=window->findChild<QListWidget*>("playlist");list->clear();
        require(window->findChild<QLabel*>("brand")->text()=="μSIK","brand");
        for(const auto &p:paths)window->addFile(p);
        QVector<int> played;
        for(int i=0;i<paths.size();++i){
            int idx=window->nextTrackIndex();require(idx>=0 && idx<paths.size(),"invalid next track");
            window->playRow(idx);played.append(idx);
        }
        if(mode!=4)require(QSet<int>(played.begin(),played.end()).size()==paths.size(),"repeated track");
        if(mode==3)for(int i=0;i<6;i+=2)require(played[i]%2==0 && played[i+1]==played[i]+1,"album order");
        int previous=window->nextTrackIndex(-1);require(previous==played[4],"Previous history");
        window->playRow(previous);require(window->nextTrackIndex()==played[5],"forward history");
        for(int i=0;i<paths.size();++i)require(list->item(i)->data(Qt::UserRole)==paths[i],"playlist reordered");
    }
    testAudio(paths[0]);testFireworks();
}
