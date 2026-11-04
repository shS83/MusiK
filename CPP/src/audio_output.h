#pragma once
#include <cstring>
#include <QAudioBuffer>
#include <QAudioSink>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QMediaDevices>
#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QSettings>
#include <QTimer>
#include <algorithm>
#include <functional>
#include <set>

struct OutputChoice {
    QByteArray id;
    QString width="auto";
    int rate=0;
    bool operator==(const OutputChoice &) const = default;

    QAudioDevice device() const {
        for(const auto &d:QMediaDevices::audioOutputs())if(d.id()==id)return d;
        return QMediaDevices::defaultAudioOutput();
    }
    QAudioFormat format(const QAudioDevice &d) const {
        auto fmt=d.preferredFormat();
        if(width=="16")fmt.setSampleFormat(QAudioFormat::Int16);
        if(width=="24" || width=="32")fmt.setSampleFormat(QAudioFormat::Int32);
        if(width=="float")fmt.setSampleFormat(QAudioFormat::Float);
        if(rate)fmt.setSampleRate(rate);
        return fmt;
    }
    static OutputChoice load(){
        QSettings s(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
        return {s.value("outputDevice").toByteArray(),s.value("outputWidth","auto").toString(),s.value("outputRate",0).toInt()};
    }
    void save() const {
        QSettings s(QSettings::defaultFormat(),QSettings::UserScope,"MusiK","MusiKCpp");
        s.setValue("outputDevice",id);s.setValue("outputWidth",width);s.setValue("outputRate",rate);
    }
};

class OutputControls : public QObject {
    QComboBox *devices=new QComboBox,*widths=new QComboBox,*rates=new QComboBox;
    QMediaDevices monitor;
    void populateDevices(QByteArray id){
        QSignalBlocker block(devices);
        devices->clear();devices->addItem("System default",QByteArray{});
        for(const auto &d:QMediaDevices::audioOutputs())devices->addItem(d.description(),d.id());
        if(devices->findData(id)<0)devices->addItem("Disconnected device (using system default)",id);
        devices->setCurrentIndex(devices->findData(id));
    }
    void populateFormats(OutputChoice choice){
        QSignalBlocker block(widths);
        auto device=choice.device();
        std::set<int> hz={22050,32000,44100,48000,88200,96000,176400,192000,device.preferredFormat().sampleRate()};
        widths->clear();widths->addItem("Automatic","auto");
        for(auto width:{"16","24","32","float"}){
            OutputChoice trial=choice;trial.width=width;
            if(std::any_of(hz.begin(),hz.end(),[&](int n){trial.rate=n;return n>0 && device.isFormatSupported(trial.format(device));})){
                QString label=QString(width)+"-bit integer";
                if(trial.width=="24")label+=" (32-bit container)";
                if(trial.width=="float")label="32-bit float";
                widths->addItem(label,width);
            }
        }
        widths->setCurrentIndex(std::max(0,widths->findData(choice.width)));
        choice.width=widths->currentData().toString();
        rates->clear();rates->addItem("Automatic",0);
        const auto selectedRate=choice.rate;
        for(int n:hz){
            choice.rate=n;
            auto format=choice.format(device);
            if(n>0 && device.isFormatSupported(format)){
                double kbps=n*format.bytesPerFrame()*8/1000.0;
                rates->addItem(QString("%1 kb/s  ·  %2 kHz").arg(kbps).arg(n/1000.0),n);
            }
        }
        rates->setCurrentIndex(std::max(0,rates->findData(selectedRate)));
    }
public:
    OutputChoice value() const {return {devices->currentData().toByteArray(),widths->currentData().toString(),rates->currentData().toInt()};}
    OutputControls(QFormLayout *form,const OutputChoice &initial){
        form->addRow("Output device",devices);form->addRow("Bit width",widths);form->addRow("Bitrate (PCM)",rates);
        populateDevices(initial.id);populateFormats(initial);
        connect(devices,&QComboBox::currentIndexChanged,this,[this]{populateFormats(value());});
        connect(widths,&QComboBox::currentIndexChanged,this,[this]{populateFormats(value());});
        connect(&monitor,&QMediaDevices::audioOutputsChanged,this,[this]{auto c=value();populateDevices(c.id);populateFormats(c);});
    }
};

class PlaybackOutput : public QObject {
    QMediaPlayer *player;
    QAudioOutput *volume;
    std::function<void(const QAudioBuffer &)> analyze;
    std::function<void(QString)> warn;
    QMediaDevices monitor;
    QTimer pump;
    QAudioBufferOutput *tap=nullptr;
    QAudioSink *sink=nullptr;
    QIODevice *writer=nullptr;
    QByteArray pending;
    OutputChoice selected;
    QAudioDevice active;
    bool failed=false;

    void syncVolume(){if(sink)sink->setVolume(volume->isMuted()?0:volume->volume());}
    void fallback(){
        if(failed)return;
        failed=true;flush();
        QTimer::singleShot(0,tap,[this]{
            warn("Device could not play the selected PCM format. Using automatic playback.");
            install(true);
        });
    }
    void drain(){
        if(!writer)return;
        if((sink->error()!=QtAudio::NoError)
           || pending.size()>sink->format().bytesForDuration(1000000)){fallback();return;}
        const auto count=std::min(pending.size(),sink->bytesFree());
        if(count>0){
            const auto written=writer->write(pending.constData(),count);
            if(written<0){fallback();return;}
            pending.remove(0,written);
        }
        if(pending.isEmpty())pump.stop();else if(!pump.isActive())pump.start();
    }
    void receive(const QAudioBuffer &b){
        analyze(b);
        if(!sink || failed || !b.isValid() || player->playbackState()!=QMediaPlayer::PlayingState)return;
        if(b.format()!=sink->format()){fallback();return;}
        if(!writer)writer=sink->start();
        if(!writer){fallback();return;}
        if(selected.width=="24"){
            auto samples=b.constData<qint32>();
            QByteArray pcm(b.byteCount(),Qt::Uninitialized);
            for(int i=0;i<b.sampleCount();++i){
                qint32 sample=samples[i]&~255;
                std::memcpy(pcm.data()+i*sizeof(sample),&sample,sizeof(sample));
            }
            pending+=pcm;
        }else pending.append(b.constData<char>(),b.byteCount());
        drain();
    }
    void install(bool automatic=false){
        flush();
        player->setAudioBufferOutput(nullptr);delete tap;tap=nullptr;
        delete sink;sink=nullptr;failed=false;
        active=selected.device();
        volume->setDevice(selected.id.isEmpty()?QAudioDevice{}:active);
        auto fmt=selected.format(active);
        const bool explicitFormat=!automatic && (selected.width!="auto" || selected.rate);
        if(explicitFormat && !active.isNull() && active.isFormatSupported(fmt)){
            player->setAudioOutput(nullptr);
            sink=new QAudioSink(active,fmt,this);
            sink->setBufferSize(fmt.bytesForDuration(100000));syncVolume();
            tap=new QAudioBufferOutput(fmt,this);
        }else{
            if(explicitFormat)warn("Selected PCM format unavailable. Using automatic playback.");
            player->setAudioOutput(volume);tap=new QAudioBufferOutput(this);
        }
        connect(tap,&QAudioBufferOutput::audioBufferReceived,tap,[this](const auto &b){receive(b);});
        player->setAudioBufferOutput(tap);
    }
public:
    const OutputChoice &choice() const {return selected;}
    void configure(OutputChoice c){if(c!=selected){selected=c;install();}}
    void flush(){pump.stop();pending.clear();writer=nullptr;if(sink)sink->reset();}
    PlaybackOutput(QMediaPlayer *p,QAudioOutput *v,std::function<void(const QAudioBuffer &)> analysis,
                   std::function<void(QString)> warning)
        :player(p),volume(v),analyze(std::move(analysis)),warn(std::move(warning)),selected(OutputChoice::load()){
        pump.setInterval(8);
        connect(&pump,&QTimer::timeout,this,[this]{drain();});
        connect(volume,&QAudioOutput::volumeChanged,this,[this]{syncVolume();});
        connect(volume,&QAudioOutput::mutedChanged,this,[this]{syncVolume();});
        connect(&monitor,&QMediaDevices::audioOutputsChanged,this,[this]{if(selected.device()!=active)install();});
        connect(player,&QMediaPlayer::sourceChanged,this,[this]{flush();});
        connect(player,&QMediaPlayer::playbackStateChanged,this,[this](auto state){
            if(!sink)return;
            if(state==QMediaPlayer::StoppedState)flush();
            else if(state==QMediaPlayer::PausedState){pump.stop();if(writer)sink->suspend();}
            else if(writer){sink->resume();drain();}
        });
        install();
    }
    ~PlaybackOutput(){flush();player->setAudioBufferOutput(nullptr);player->setAudioOutput(nullptr);}
};
