// connector.h
//
// Abstract base class for streaming server source connections.
//
//   (C) Copyright 2014-2015 Fred Gleason <fredg@paravelsystems.com>
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

#ifndef CONNECTOR_H
#define CONNECTOR_H

#include <stdint.h>

#include <vector>

#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>
#include <QProcess>

#define RINGBUFFER_SERVICE_INTERVAL 50

class Connector : public QObject
{
  Q_OBJECT;
 public:
  enum ServerType {HlsServer=0,Icecast2Server=1,
		   Shoutcast1Server=2,Shoutcast2Server=3,FileServer=4,
		   FileArchiveServer=5,LastServer=6};
  Connector(QObject *parent=0);
  ~Connector();
  virtual Connector::ServerType serverType() const=0;
  QString serverUsername() const;
  void setServerUsername(const QString &str);
  QString serverPassword() const;
  void setServerPassword(const QString &str);
  QString serverMountpoint() const;
  void setServerMountpoint(const QString &str);
  QString contentType() const;
  void setContentType(const QString &str);
  unsigned audioChannels() const;
  void setAudioChannels(unsigned chans);
  unsigned audioSamplerate() const;
  void setAudioSamplerate(unsigned rate);
  unsigned audioBitrate() const;
  void setAudioBitrate(unsigned rate);
  std::vector<unsigned> *audioBitrates();
  void setAudioBitrates(std::vector<unsigned> *rates);
  QString streamName() const;
  void setStreamName(const QString &str);
  QString streamDescription() const;
  void setStreamDescription(const QString &str);
  QString streamUrl() const;
  void setStreamUrl(const QString &str);
  QString streamIrc() const;
  void setStreamIrc(const QString &str);
  QString streamIcq() const;
  void setStreamIcq(const QString &str);
  QString streamAim() const;
  void setStreamAim(const QString &str);
  QString streamGenre() const;
  void setStreamGenre(const QString &str);
  bool streamPublic() const;
  void setStreamPublic(bool state);
  int streamTimestampOffset() const;
  void setStreamTimestampOffset(int msec);
  QString extension() const;
  void setExtension(const QString &str);
  QString formatIdentifier() const;
  void setFormatIdentifier(const QString &str);
  virtual void connectToServer(const QString &hostname,uint16_t port);
  virtual int64_t writeData(int frames,const unsigned char *data,int64_t len);
  void stop();
  QString scriptUp() const;
  void setScriptUp(const QString &cmd);
  QString scriptDown() const;
  void setScriptDown(const QString &cmd);
  static QString serverTypeText(Connector::ServerType);
  static QString optionKeyword(Connector::ServerType type);
  static Connector::ServerType serverType(const QString &key);
  static QString subMountpointName(const QString &mntpt,unsigned bitrate);
  static QString pathPart(const QString &fullpath);
  static QString basePart(const QString &fullpath);
  static QString urlEncode(const QString &str);
  static QString urlDecode(const QString &str);
  static QString base64Encode(const QString &str);
  static QString base64Decode(const QString &str,bool *ok=NULL);
  static QString curlStrError(int exit_code);
  static QString httpStrError(int status_code);
  static QString timezoneOffset();

 signals:
  void connected(bool state);
  void dataRequested(Connector *conn);
  void error(QAbstractSocket::SocketError err);
  void stopped();

 private slots:
  void dataTimeoutData();
  void watchdogTimeoutData();
  void stopTimeoutData();
  void scriptErrorData(QProcess::ProcessError err);
  void scriptUpFinishedData(int exit_code,QProcess::ExitStatus exit_status);
  void scriptUpCollectGarbageData();
  void scriptDownFinishedData(int exit_code,QProcess::ExitStatus exit_status);
  void scriptDownCollectGarbageData();

 protected:
  virtual void startStopping();
  void setConnected(bool state);
  void setError(QAbstractSocket::SocketError err);
  virtual void connectToHostConnector(const QString &hostname,uint16_t port)=0;
  virtual void disconnectFromHostConnector()=0;
  virtual int64_t writeDataConnector(int frames,const unsigned char *data,
				     int64_t len)=0;
  QString hostHostname() const;
  uint16_t hostPort() const;

 private:
  QString conn_server_username;
  QString conn_server_password;
  QString conn_server_mountpoint;
  QString conn_content_type;
  unsigned conn_audio_channels;
  unsigned conn_audio_samplerate;
  //unsigned conn_audio_bitrate;
  std::vector<unsigned> conn_audio_bitrates;
  QString conn_stream_name;
  QString conn_stream_description;
  QString conn_stream_url;
  QString conn_stream_irc;
  QString conn_stream_icq;
  QString conn_stream_aim;
  QString conn_stream_genre;
  bool conn_stream_public;
  int conn_stream_timestamp_offset;
  QString conn_extension;
  QString conn_format_identifier;
  QTimer *conn_data_timer;
  QTimer *conn_watchdog_timer;
  bool conn_watchdog_active;
  bool conn_connected;
  QString conn_host_hostname;
  uint16_t conn_host_port;
  QTimer *conn_stop_timer;
  QString conn_script_up;
  QProcess *conn_script_up_process;
  QStringList conn_script_up_args;
  QTimer *conn_script_up_garbage_timer;
  QString conn_script_down;
  QProcess *conn_script_down_process;
  QStringList conn_script_down_args;
  QTimer *conn_script_down_garbage_timer;
  bool conn_is_stopping;
};


#endif  // CONNECTOR_H
