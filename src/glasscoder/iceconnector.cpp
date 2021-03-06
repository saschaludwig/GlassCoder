// iceconnector.cpp
//
// Source connector class for IceCast2 servers
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

#include <QStringList>

#include "iceconnector.h"
#include "logging.h"

IceConnector::IceConnector(QObject *parent)
  : Connector(parent)
{
  ice_recv_buffer="";

  ice_socket=new QTcpSocket(this);
  connect(ice_socket,SIGNAL(connected()),this,SLOT(socketConnectedData()));
  connect(ice_socket,SIGNAL(disconnected()),
	  this,SLOT(socketDisconnectedData()));
  connect(ice_socket,SIGNAL(readyRead()),this,SLOT(socketReadyReadData()));
  connect(ice_socket,SIGNAL(error(QAbstractSocket::SocketError)),
	  this,SLOT(socketErrorData(QAbstractSocket::SocketError)));
}


IceConnector::~IceConnector()
{
  delete ice_socket;
}


IceConnector::ServerType IceConnector::serverType() const
{
  return Connector::Icecast2Server;
}


void IceConnector::connectToHostConnector(const QString &hostname,uint16_t port)
{
  ice_socket->connectToHost(hostname,port);
}


void IceConnector::disconnectFromHostConnector()
{
  ice_socket->disconnectFromHost();
}


int64_t IceConnector::writeDataConnector(int frames,const unsigned char *data,
					 int64_t len)
{
  return ice_socket->write((const char *)data,len);
}


void IceConnector::socketConnectedData()
{
  QString username=serverUsername();
  if(username.isEmpty()) {
    username="source";
  }
  WriteHeader("SOURCE "+serverMountpoint()+" HTTP/1.0");
  WriteHeader(QString("Authorization: Basic ")+
	      Connector::base64Encode(username+":"+serverPassword()));
  WriteHeader(QString("User-Agent: GlassCoder/")+VERSION);
  WriteHeader("Content-Type: "+contentType());
  WriteHeader("ice-name: "+streamName());
  WriteHeader("ice-description: "+streamDescription());
  WriteHeader("ice-genre: "+streamGenre());
  WriteHeader("ice-public: "+QString().sprintf("%d",streamPublic()));
  WriteHeader(QString("ice-audio-info: ")+
	      QString().sprintf("bitrate=%u;",audioBitrate())+
	      QString().sprintf("channels=%u;",audioChannels())+
	      QString().sprintf("samplerate=%u",audioSamplerate()));
  WriteHeader("");
}


void IceConnector::socketDisconnectedData()
{
  setConnected(false);
}


void IceConnector::socketReadyReadData()
{
  char data[1501];
  int64_t n;

  while((n=ice_socket->read(data,1500))>0) {
    data[n]=0;
    //printf("recvd %ld bytes: |%s|\n",n,data);
    for(int64_t i=0;i<n;i++) {
      switch(0xFF&data[i]) {
      case 10:
	if((ice_recv_buffer.length()>=2)&&
	   (ice_recv_buffer.toUtf8().at(ice_recv_buffer.length()-3)==13)) {
	  ice_recv_buffer=ice_recv_buffer.left(ice_recv_buffer.length()-1);
	  ProcessHeaders(ice_recv_buffer);
	  ice_recv_buffer="";
	}
	else {
	  ice_recv_buffer+=data[i];
	}
	break;

      default:
	ice_recv_buffer+=data[i];
	break;
      }
    }
  }
}


void IceConnector::socketErrorData(QAbstractSocket::SocketError err)
{
  setError(err);
}


void IceConnector::ProcessHeaders(const QString &hdrs)
{
  QStringList f0;
  QStringList f1;
  QString txt;

  f0=hdrs.split("\r\n");
  for(int i=0;i<f0.size();i++) {
    f1=f0[i].split(" ");
    if(f1[0].left(7)=="HTTP/1.") {
      for(int i=2;i<f1.size();i++) {
	txt+=f1[i]+" ";
      }
      txt=txt.left(txt.length()-1);
      if(f1[1].toInt()==200) {
	setConnected(true);
      }
      else {
	Log(LOG_ERR,
	    QString().sprintf("server \"%s:%u/%s\" returned \"%d %s\"",
			      (const char *)hostHostname().toUtf8(),
			      0xFFFF&hostPort(),
			      (const char *)serverMountpoint().toUtf8(),
			      f1[1].toInt(),(const char *)txt.toUtf8()));
	    setError(QAbstractSocket::UnknownSocketError);
      }
      return;
    }
  }
  Log(LOG_ERR,
      QString().sprintf("server \"%s:%u/%s\" returned unrecognized response",
			(const char *)hostHostname().toUtf8(),0xFFFF&hostPort(),
			(const char *)serverMountpoint().toUtf8()));
  setError(QAbstractSocket::UnknownSocketError);
}


void IceConnector::WriteHeader(const QString &str)
{
  ice_socket->write((str+"\r\n").toUtf8());
}
