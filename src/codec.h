// codec.h
//
// Abstract base class for audio codecs.
//
//   (C) Copyright 2014 Fred Gleason <fredg@paravelsystems.com>
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License version 2 as
//   published by the Free Software Foundation.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public
//   License along with this program; if not, write to the Free Software
//   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef CODEC_H
#define CODEC_H

#include <queue>

#include <dlfcn.h>
#include <syslog.h>

#include <samplerate.h>

#include <QtCore/QObject>

#include "connector.h"
#include "ringbuffer.h"

#define MAX_AUDIO_CHANNELS 2
#define MAX_AUDIO_BUFFER 4096

class Codec : public QObject
{
  Q_OBJECT;
 public:
  enum Type {TypeMpegL2=0,TypeMpegL3=1,TypeAac=2,TypeLast=3};
  enum BitMode {BitModeConstant=0,BitModeVariable=1};
  Codec(Codec::Type type,Ringbuffer *ring,QObject *parent=0);
  ~Codec();
  Codec::BitMode bitmode() const;
  void setBitmode(Codec::BitMode mode);
  unsigned bitrate() const;
  void setBitrate(unsigned rate);
  unsigned channels() const;
  void setChannels(unsigned chans);
  double quality() const;
  void setQuality(double qual);
  unsigned sourceSamplerate() const;
  void setSourceSamplerate(unsigned rate);
  unsigned streamSamplerate() const;
  void setStreamSamplerate(unsigned rate);
  virtual QString contentType() const=0;
  virtual unsigned pcmFrames() const=0;
  virtual bool start();
  static QString codecTypeText(Codec::Type type);

 public slots:
  virtual void encode(Connector *conn);

 protected:
  virtual void encodeData(Connector *conn,const float *pcm,int len)=0;
  virtual bool startCodec()=0;
  Ringbuffer *ring();

 private:
  Ringbuffer *codec_ring1;
  Ringbuffer *codec_ring2;
  Codec::BitMode codec_bitmode;
  unsigned codec_bitrate;
  unsigned codec_channels;
  double codec_quality;
  unsigned codec_source_samplerate;
  unsigned codec_stream_samplerate;
  SRC_STATE *codec_src_state;
  SRC_DATA *codec_src_data;
  float *codec_pcm_in;
  float *codec_pcm_out;
  float *codec_pcm_buffer[2];
};


#endif  // CODEC_H