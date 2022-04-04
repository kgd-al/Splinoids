#ifndef KGD_SOUND_VISUALIZER_H
#define KGD_SOUND_VISUALIZER_H

#include <QWidget>
#include <QTimer>

#include "generator.h"

namespace kgd::watchmaker::sound {

class Visualizer : public QWidget {
  Q_OBJECT

  const uchar _channel;

  const StaticData::NoteSheet *_notes;
  int _prevNote, _currNote;

  QImage _data;
  bool _highlight;

  QTimer _timer;
  uint _currentMS;
  StaticData::PlaybackType _playback;

  float _sliderPos;

public:
  Visualizer(uchar channel, QWidget *parent = nullptr);
  ~Visualizer (void);

  QSize minimumSizeHint(void) const override {
    return QSize(100,100);
  }

  bool hasHeightForWidth(void) const override {
    return true;
  }

  int heightForWidth(int w) const override {
    return w;
  }

  void setNoteSheet(const StaticData::NoteSheet &notes);
  void vocaliseToAudioOut (StaticData::PlaybackType t);
  void stopVocalisationToAudioOut (void);

  void start (void);
  void pause (void);
  void resume (void);
  void stop (void);

  void setHighlighted (bool s);

  void paintEvent(QPaintEvent *e) override;

  void render (const std::string &filename) const {
    render(QString::fromStdString(filename));
  }
  void render (const QString &filename) const;

signals:
  void notifyNote (void);

private:
  void timeout(void);
  void updateSlider (void);
  void nextNote (bool spontaneous);
};

} // end of namespace kgd::watchmaker::sound

#endif // KGD_SOUND_VISUALIZER_H
