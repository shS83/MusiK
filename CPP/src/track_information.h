#pragma once

#include <QDialog>
#include <QDir>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLabel>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QtConcurrentRun>
#include <algorithm>

class TrackInformation : public QDialog {
    struct Cover { QImage image; QString name; };
    QFileInfo track;
    QMediaPlayer reader;
    QTimer timeout;
    QLabel *cover, *source, *status;
    QFormLayout *fields;
    QImage embedded;
    Cover folder;
    bool searched=false;

    static Cover folderCover(const QFileInfo &track){
        const QStringList preferred={track.completeBaseName().toCaseFolded(),"cover","folder","front","albumart","album","artwork"};
        const QStringList extensions={"jpg","jpeg","png","webp","bmp"};
        auto files=track.dir().entryInfoList(QDir::Files,QDir::Name|QDir::IgnoreCase);
        auto priority=[&](const QFileInfo &f){
            auto index=preferred.indexOf(f.completeBaseName().toCaseFolded());
            return index<0?preferred.size():index;
        };
        std::stable_sort(files.begin(),files.end(),[&](const auto &a,const auto &b){return priority(a)<priority(b);});
        for(const auto &file:files){
            if(!extensions.contains(file.suffix().toLower()) || file.size()>32*1024*1024)continue;
            QImageReader image(file.absoluteFilePath());
            image.setAutoTransform(true);
            auto size=image.size();
            if(size.isValid() && (size.width()>600 || size.height()>600))
                image.setScaledSize(size.scaled(600,600,Qt::KeepAspectRatio));
            auto result=image.read();
            if(!result.isNull())return {result,file.fileName()};
        }
        return {};
    }
    void updateCover(){
        const auto &image=folder.image.isNull()?embedded:folder.image;
        if(image.isNull()){
            cover->setText("No album art");
            source->setText(searched?"No cover image in this folder":"Searching this folder…");
            return;
        }
        auto pixmap=QPixmap::fromImage(image).scaled(cover->size()*devicePixelRatioF(),
                                                    Qt::KeepAspectRatio,Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(devicePixelRatioF());
        cover->setPixmap(pixmap);
        source->setText(folder.image.isNull()?"Embedded album art":folder.name);
    }
    void addField(const QString &key,const QString &text){
        auto *value=new QLabel;
        value->setTextFormat(Qt::PlainText);value->setText(text);value->setWordWrap(true);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fields->addRow(key,value);
    }
    void refresh(){
        using M=QMediaMetaData;
        while(fields->rowCount())fields->removeRow(0);
        const auto meta=reader.metaData();
        const QList<M::Key> priority={M::Title,M::ContributingArtist,M::AlbumTitle,M::AlbumArtist,
                                      M::TrackNumber,M::Genre,M::Date,M::Composer};
        auto keys=meta.keys();
        std::sort(keys.begin(),keys.end(),[&](auto a,auto b){
            auto i=priority.indexOf(a),j=priority.indexOf(b);
            if(i<0 && j<0)return M::metaDataKeyToString(a)<M::metaDataKeyToString(b);
            return j<0 || (i>=0 && i<j);
        });
        for(auto key:keys){
            if(key==M::Duration || key==M::CoverArtImage || key==M::ThumbnailImage)continue;
            auto value=meta.stringValue(key);
            if(key==M::AudioBitRate)value=QString("%1 kb/s").arg(meta.value(key).toDouble()/1000);
            if(!value.isEmpty())addField(M::metaDataKeyToString(key),value);
        }
        if(reader.duration()>0){
            auto sec=reader.duration()/1000;
            addField("Duration",QString("%1:%2").arg(sec/60).arg(sec%60,2,10,QChar('0')));
        }
        addField("File",track.fileName());
        addField("Folder",QDir::toNativeSeparators(track.absolutePath()));
        addField("Format",track.suffix().toUpper());
        addField("Size",QString("%1 MiB").arg(track.size()/1048576.0,0,'f',2));
        embedded=meta.value(M::CoverArtImage).value<QImage>();
        if(embedded.isNull())embedded=meta.value(M::ThumbnailImage).value<QImage>();
        updateCover();
    }
public:
    TrackInformation(const QString &path,QWidget *parent,const QString &canvasStyle)
        :QDialog(parent),track(path){
        setAttribute(Qt::WA_DeleteOnClose);
        setWindowTitle("μSIK · Track information");
        setObjectName("canvas");setStyleSheet(canvasStyle);
        resize(760,520);setMinimumSize(560,360);
        auto *outer=new QVBoxLayout(this);
        auto *panel=new QFrame;panel->setObjectName("panel");outer->addWidget(panel);
        auto *layout=new QVBoxLayout(panel);layout->setContentsMargins(18,16,18,16);
        auto *header=new QHBoxLayout;layout->addLayout(header);
        auto *title=new QLabel;title->setObjectName("track");title->setTextFormat(Qt::PlainText);
        title->setText(track.completeBaseName());title->setWordWrap(true);header->addWidget(title,1);
        auto *close=new QPushButton("×");close->setAccessibleName("Close track information");
        close->setFixedWidth(44);header->addWidget(close,0,Qt::AlignTop);
        connect(close,&QPushButton::clicked,this,&QDialog::reject);
        auto *body=new QHBoxLayout;layout->addLayout(body,1);
        auto *art=new QVBoxLayout;body->addLayout(art);
        cover=new QLabel;cover->setFixedSize(240,240);cover->setAlignment(Qt::AlignCenter);
        cover->setStyleSheet("background:rgba(0,0,0,70);border-radius:10px;");art->addWidget(cover);
        source=new QLabel;source->setObjectName("coverSource");source->setTextFormat(Qt::PlainText);
        source->setWordWrap(true);source->setMaximumWidth(240);art->addWidget(source);art->addStretch();
        auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);
        scroll->setStyleSheet("QScrollArea{border:none;background:transparent;} QWidget#infoFields{background:transparent;}");
        auto *content=new QWidget;content->setObjectName("infoFields");
        fields=new QFormLayout(content);fields->setRowWrapPolicy(QFormLayout::WrapLongRows);
        scroll->setWidget(content);scroll->viewport()->setAutoFillBackground(false);
        content->setAutoFillBackground(false);body->addWidget(scroll,1);
        status=new QLabel("Reading metadata… · Esc closes this window");status->setWordWrap(true);layout->addWidget(status);
        connect(&reader,&QMediaPlayer::metaDataChanged,this,&TrackInformation::refresh);
        connect(&reader,&QMediaPlayer::durationChanged,this,&TrackInformation::refresh);
        connect(&reader,&QMediaPlayer::mediaStatusChanged,this,[this](auto state){
            if(state==QMediaPlayer::LoadedMedia || state==QMediaPlayer::BufferedMedia){
                timeout.stop();refresh();status->setText("Available metadata · Esc or × closes this window");
            }
        });
        connect(&reader,&QMediaPlayer::errorOccurred,this,[this]{
            timeout.stop();status->setText("Metadata unavailable for this file · Esc or × closes this window");
        });
        timeout.setSingleShot(true);
        connect(&timeout,&QTimer::timeout,this,[this]{status->setText("Some metadata could not be read · Esc or × closes this window");});
        refresh();
        auto *watcher=new QFutureWatcher<Cover>(this);
        connect(watcher,&QFutureWatcher<Cover>::finished,this,[this,watcher]{
            folder=watcher->result();searched=true;updateCover();watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run(&TrackInformation::folderCover,track));
        timeout.start(8000);
        reader.setSource(QUrl::fromLocalFile(track.absoluteFilePath()));
    }
};
