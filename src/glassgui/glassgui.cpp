// glassgui.cpp
//
// glassgui(1) Audio Encoder front end
//
//   (C) Copyright 2015 Fred Gleason <fredg@paravelsystems.com>
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

#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QUrl>

#include "audiodevice.h"
#include "cmdswitch.h"
#include "codec.h"
#include "connector.h"
#include "logging.h"
#include "profile.h"

#include "glassgui.h"

#include "../../icons/glasscoder-16x16.xpm"

MainWidget::MainWidget(QWidget *parent)
  : QMainWindow(parent)
{
  instance_name="";
  QString delete_instance="";
  bool list_instances=false;
  gui_autostart=false;

  CmdSwitch *cmd=
    new CmdSwitch(qApp->argc(),qApp->argv(),"glassgui",GLASSGUI_USAGE);
  for(unsigned i=0;i<cmd->keys();i++) {
    if(cmd->key(i)=="--autostart") {
      gui_autostart=true;
      cmd->setProcessed(i,true);
    }
    if(cmd->key(i)=="--delete-instance") {
      delete_instance=cmd->value(i);
      cmd->setProcessed(i,true);
    }
    if(cmd->key(i)=="--instance-name") {
      instance_name=cmd->value(i);
      cmd->setProcessed(i,true);
    }
    if(cmd->key(i)=="--list-instances") {
      list_instances=true;
      cmd->setProcessed(i,true);
    }
    if(!cmd->processed(i)) {
      QMessageBox::critical(this,"GlassGui - "+tr("Error"),
			    tr("Unknown argument")+" \""+cmd->key(i)+"\".");
      exit(256);
    }
  }
  if(list_instances&&(!delete_instance.isEmpty())) {
    fprintf(stderr,"glassgui: --list-instances and --delete-instance are mutually exclusive\n");
    exit(256);
  }
  if(list_instances) {
    ListInstances();
    exit(0);
  }
  if(!delete_instance.isEmpty()) {
    DeleteInstance(delete_instance);
    exit(0);
  }

  setWindowIcon(QPixmap(glasscoder_16x16_xpm));
  if(instance_name.isEmpty()) {
    setWindowTitle(QString("GlassGui v")+VERSION);
  }
  else {
    setWindowTitle(QString("GlassGui v")+VERSION+" - "+instance_name);
  }
  gui_settings_dir=NULL;

  //
  // Fonts
  //
  QFont big_font("helvetica",20,QFont::Bold);
  big_font.setPixelSize(20);
  QFont section_font("helvetica",16,QFont::Bold);
  section_font.setPixelSize(16);
  QFont label_font("helvetica",14,QFont::Bold);
  label_font.setPixelSize(14);
  QFont message_font("helvetica",14,QFont::Normal);
  message_font.setPixelSize(14);

  //
  // Set Size
  //
  setMinimumSize(sizeHint());
  setMaximumHeight(sizeHint().height());

  //
  // Dialogs
  //
  gui_server_dialog=new ServerDialog(this);
  gui_codec_dialog=new CodecDialog(this);
  gui_codeviewer_dialog=new CodeViewer(this);
  gui_source_dialog=new SourceDialog(this);
  connect(gui_source_dialog,SIGNAL(updated()),this,SLOT(checkArgs()));
  gui_stream_dialog=new StreamDialog(this);
  connect(gui_server_dialog,SIGNAL(typeChanged(Connector::ServerType,bool)),
	  this,SLOT(serverTypeChangedData(Connector::ServerType,bool)));
  connect(gui_server_dialog,SIGNAL(settingsChanged()),this,SLOT(checkArgs()));

  //
  // Status Bar
  //
  gui_message_widget=new MessageWidget(this);
  gui_message_widget->setFont(message_font);
  gui_message_widget->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
  gui_message_widget->setFrameStyle(QFrame::Box|QFrame::Raised);

  gui_status_frame_widget=new QLabel(this);
  gui_status_frame_widget->setFrameStyle(QFrame::Box|QFrame::Raised);
  gui_status_widget=new StatusWidget(this);
  gui_status_widget->setFont(label_font);

  //
  // Meter Section
  //
  gui_meter=new StereoMeter(this);
  gui_meter->setReference(800);
  gui_meter->setMode(SegMeter::Peak);
  gui_start_button=new QPushButton(tr("Start"),this);
  gui_start_button->setFont(big_font);
  connect(gui_start_button,SIGNAL(clicked()),this,SLOT(startEncodingData()));
  gui_start_button->setDisabled(true);
  gui_code_button=new QPushButton(tr("Show")+"\n"+tr("Code"),this);
  gui_code_button->setFont(section_font);
  connect(gui_code_button,SIGNAL(clicked()),this,SLOT(showCodeData()));
  gui_code_button->setDisabled(true);

  //
  // Server Settings
  //
  gui_server_button=new QPushButton(tr("Server\nSettings"),this);
  gui_server_button->setFont(section_font);
  connect(gui_server_button,SIGNAL(clicked()),this,SLOT(serverData()));

  //
  // Codec Settings
  //
  gui_codec_button=new QPushButton(tr("Codec\nSettings"),this);
  gui_codec_button->setFont(section_font);
  connect(gui_codec_button,SIGNAL(clicked()),this,SLOT(codecData()));

  //
  // Stream Settings
  //
  gui_stream_button=new QPushButton(tr("Stream\nSettings"),this);
  gui_stream_button->setFont(section_font);
  connect(gui_stream_button,SIGNAL(clicked()),this,SLOT(streamData()));

  //
  // Source Settings
  //
  gui_source_button=new QPushButton(tr("Source\nSettings"),this);
  gui_source_button->setFont(section_font);
  connect(gui_source_button,SIGNAL(clicked()),this,SLOT(sourceData()));

  //
  // Process Timers
  //
  gui_process_cleanup_timer=new QTimer(this);
  gui_process_cleanup_timer->setSingleShot(true);
  connect(gui_process_cleanup_timer,SIGNAL(timeout()),
	  this,SLOT(processCollectGarbageData()));

  gui_process_kill_timer=new QTimer(this);
  gui_process_kill_timer->setSingleShot(true);
  connect(gui_process_kill_timer,SIGNAL(timeout()),
	  this,SLOT(processKillData()));

  //
  // Autostart Timer
  //
  gui_autostart_timer=new QTimer(this);
  gui_autostart_timer->setSingleShot(true);
  connect(gui_autostart_timer,SIGNAL(timeout()),this,SLOT(startEncodingData()));

  //
  // Get Codec List
  //
  gui_process=new QProcess(this);
  connect(gui_process,SIGNAL(error(QProcess::ProcessError)),
	  this,SLOT(processErrorData(QProcess::ProcessError)));
  connect(gui_process,SIGNAL(finished(int,QProcess::ExitStatus)),
	  this,SLOT(codecFinishedData(int,QProcess::ExitStatus)));

  QStringList args;
  args.push_back("--list-codecs");
  gui_process->start("glasscoder",args);
}


QSize MainWidget::sizeHint() const
{
  return QSize(500,175);
}


void MainWidget::closeEvent(QCloseEvent *e)
{
  if(gui_process!=NULL) {
    if(QMessageBox::question(this,"GlassGui - "+tr("Exiting"),
			     tr("The encoder is currently running.")+" "+
			     tr("Close will shut it down.")+"\n\n"+
			     tr("Proceed?"),QMessageBox::Yes,QMessageBox::No)==
       QMessageBox::No) {
      e->ignore();
      return;
    }
    gui_process->terminate();
    gui_process_kill_timer->start(GLASSGUI_TERMINATE_TIMEOUT);
    SaveSettings();
    e->ignore();
    return;
  }
  if(!SaveSettings()) {
    QMessageBox::warning(this,"GlassGui - "+tr("Save Error"),
			 tr("Unable to save configuration!"));
  }
  e->accept();
}


void MainWidget::resizeEvent(QResizeEvent *e)
{
  int ypos=10;

  gui_meter->setGeometry(10,ypos,gui_meter->sizeHint().width(),
			 gui_meter->sizeHint().height());
  gui_start_button->setGeometry(gui_meter->sizeHint().width()+20,ypos,
				size().width()-gui_meter->sizeHint().width()-30,
				gui_meter->sizeHint().height());
  ypos+=(gui_meter->sizeHint().height()+10);

  int w_edge=(size().width()-60)/5;
  gui_server_button->setGeometry(10,ypos,w_edge,55);
  gui_codec_button->setGeometry(20+w_edge,ypos,w_edge,55);
  gui_stream_button->setGeometry(30+2*w_edge,ypos,w_edge,55);
  gui_source_button->setGeometry(40+3*w_edge,ypos,w_edge,55);
  gui_code_button->setGeometry(50+4*w_edge,ypos,w_edge,55);

  ypos+=70;

  //
  // Status Bar
  //
  gui_message_widget->setGeometry(0,size().height()-30,size().width()-140,30);
  gui_status_frame_widget->
    setGeometry(size().width()-140,size().height()-30,140,30);
  gui_status_widget->setGeometry(size().width()-137,size().height()-27,134,24);
}


void MainWidget::startEncodingData()
{
  QStringList args;

  if(gui_process!=NULL) {
    QMessageBox::warning(this,"GlassGui - "+tr("Process Error"),
			 tr("Process is not in ready state!"));
    return;
  }
  gui_process=new QProcess(this);
  gui_process->setReadChannel(QProcess::StandardOutput);
  connect(gui_process,SIGNAL(readyRead()),
	  this,SLOT(processReadyReadStandardOutputData()));
  connect(gui_process,SIGNAL(error(QProcess::ProcessError)),
	  this,SLOT(processErrorData(QProcess::ProcessError)));
  connect(gui_process,SIGNAL(finished(int,QProcess::ExitStatus)),
	  this,SLOT(processFinishedData(int,QProcess::ExitStatus)));
  gui_server_dialog->makeArgs(&args,false);
  gui_codec_dialog->makeArgs(&args);
  gui_stream_dialog->makeArgs(&args,false);
  gui_source_dialog->makeArgs(&args,false);
  args.push_back("--meter-data");
  args.push_back("--errors-to=STDOUT");
  gui_process->start("glasscoder",args);
  gui_start_button->disconnect();
  connect(gui_start_button,SIGNAL(clicked()),this,SLOT(stopEncodingData()));
  gui_start_button->setText(tr("Stop"));
  LockControls(true);
}


void MainWidget::stopEncodingData()
{
  gui_process->terminate();
}


void MainWidget::showCodeData()
{
  QStringList args;

  args.push_back("glasscoder");
  gui_server_dialog->makeArgs(&args,true);
  gui_codec_dialog->makeArgs(&args);
  gui_stream_dialog->makeArgs(&args,true);
  gui_source_dialog->makeArgs(&args,true);

  gui_codeviewer_dialog->exec(args);
}


void MainWidget::serverTypeChangedData(Connector::ServerType type,
				       bool multirate)
{
  gui_stream_dialog->setServerType(type);
  gui_codec_dialog->setMultirate(multirate);
}


void MainWidget::serverData()
{
  if(gui_server_dialog->isVisible()) {
    gui_server_dialog->hide();
  }
  else {
    gui_server_dialog->show();
  }
}


void MainWidget::codecData()
{
  if(gui_codec_dialog->isVisible()) {
    gui_codec_dialog->hide();
  }
  else {
    gui_codec_dialog->show();
  }
}


void MainWidget::streamData()
{
  if(gui_stream_dialog->isVisible()) {
    gui_stream_dialog->hide();
  }
  else {
    gui_stream_dialog->show();
  }
}


void MainWidget::sourceData()
{
  if(gui_source_dialog->isVisible()) {
    gui_source_dialog->hide();
  }
  else {
    gui_source_dialog->show();
  }
}


void MainWidget::codecFinishedData(int exit_code,
				   QProcess::ExitStatus exit_status)
{
  QStringList f0;

  if(exit_code==0) {
    //
    // Populate Codec Types
    //
    gui_codec_dialog->addCodecTypes(gui_process->readAllStandardOutput());

    //
    // Get Device List
    //
    gui_process=new QProcess(this);
    connect(gui_process,SIGNAL(error(QProcess::ProcessError)),
	    this,SLOT(processErrorData(QProcess::ProcessError)));
    connect(gui_process,SIGNAL(finished(int,QProcess::ExitStatus)),
	    this,SLOT(deviceFinishedData(int,QProcess::ExitStatus)));

    QStringList args;
    args.push_back("--list-devices");
    gui_process->start("glasscoder",args);
  }
  else {
    ProcessError(exit_code,exit_status);
  }
}


void MainWidget::checkArgs()
{
  QStringList args;
  bool state;

  state=gui_server_dialog->makeArgs(&args,false);
  state=state&&gui_source_dialog->makeArgs(&args,false);
  gui_start_button->setEnabled(state);
  gui_code_button->setEnabled(state);
}


void MainWidget::checkArgs(const QString &str)
{
  checkArgs();
}


void MainWidget::deviceFinishedData(int exit_code,
				    QProcess::ExitStatus exit_status)
{
  QStringList f0;

  if(exit_code==0) {
    //
    // Populate Device Types
    //
    gui_source_dialog->addSourceTypes(gui_process->readAllStandardOutput());
    gui_process=NULL;
    LoadSettings();
    if(gui_autostart) {
      gui_autostart_timer->start(0);
    }
  }
  else {
    ProcessError(exit_code,exit_status);
  }
}


void MainWidget::processReadyReadStandardOutputData()
{
  char data[1500];
  int n=0;

  if((n=gui_process->read(data,1500))>0) {
    data[n]=0;
    for(int i=0;i<n;i++) {
      switch(0xFF&data[i]) {
      case 13:
	break;

      case 10:
	ProcessFeedback(gui_process_accum);
	gui_process_accum="";
	break;

      default:
	gui_process_accum+=data[i];
      }
    }
  }
}


void MainWidget::processFinishedData(int exit_code,
				     QProcess::ExitStatus exit_status)
{
  if(exit_code==0) {
    if(gui_process_kill_timer->isActive()) {
      exit(0);
    }
    gui_meter->setLeftPeakBar(-10000);
    gui_meter->setRightPeakBar(-10000);
    gui_start_button->disconnect();
    connect(gui_start_button,SIGNAL(clicked()),this,SLOT(startEncodingData()));
    gui_start_button->setText(tr("Start"));
    LockControls(false);
  }
  else {
    ProcessError(exit_code,exit_status);
  }
  gui_status_widget->setStatus(CONNECTION_IDLE);
  gui_process_cleanup_timer->start(0);
}


void MainWidget::processErrorData(QProcess::ProcessError err)
{
  QMessageBox::warning(this,"GlassGui - "+tr("Process Error"),
		       tr("Received QProcess error")+
		       QString().sprintf(": %d",err));
  exit(256);
}


void MainWidget::processCollectGarbageData()
{
  delete gui_process;
  gui_process=NULL;
}


void MainWidget::processKillData()
{
  gui_process->kill();
  qApp->processEvents();
  exit(0);
}


void MainWidget::LockControls(bool state)
{

  gui_codec_dialog->setControlsLocked(state);

  gui_stream_dialog->setControlsLocked(state);

  gui_source_dialog->setControlsLocked(state);
}


void MainWidget::ProcessFeedback(const QString &str)
{
  QStringList f0;
  bool ok=false;
  int level;
  int prio;
  int status;
  QString msg;

  f0=str.split(" ");

  if((f0[0]=="CS")&&(f0.size()==2)) {  // Connection Status
    status=f0[1].toInt(&ok);
    if(ok) {
      if(!gui_status_widget->setStatus(status)) {
	gui_message_widget->addMessage(tr("Unknown status code")+
				       QString().sprintf(" \"%d\" ",status)+
				       tr("received."));
      }
    }
  }

  if((f0[0]=="ER")&&(f0.size()>=2)) {  // Error Message
    prio=f0[1].toInt();
    f0.erase(f0.begin());
    f0.erase(f0.begin());
    msg=f0.join(" ");

    switch(prio) {
    case LOG_EMERG:
    case LOG_ALERT:
    case LOG_CRIT:
    case LOG_ERR:
      gui_message_widget->addMessage(msg);
      break;

    case LOG_WARNING:
    case LOG_NOTICE:
    case LOG_INFO:
      gui_message_widget->addMessage(msg);
      break;
    }
    return;
  }

  if(f0[0]=="ME") {  // Meter Levels
    if((f0.size()==2)&&(f0[1].length()==8)) {
      level=f0[1].left(4).toInt(&ok,16);
      if(ok) {
	gui_meter->setLeftPeakBar(-level);
      }
      level=f0[1].right(4).toInt(&ok,16);
      if(ok) {
	gui_meter->setRightPeakBar(-level);
      }
    }
  }
}


void MainWidget::ProcessError(int exit_code,QProcess::ExitStatus exit_status)
{
  if(exit_status==QProcess::CrashExit) {
    QMessageBox::warning(this,"GlassGui - "+tr("GlassCoder Error"),
			 tr("GlassCoder crashed!"));
  }
  else {
    QString msg=gui_process->readAllStandardError();
    QMessageBox::warning(this,"GlassGui - "+tr("GlassCoder Error"),
			 tr("GlassCoder returned a non-zero exit code")+
			 "\n\""+msg+"\".");
  }
  exit(256);
}


void MainWidget::LoadSettings()
{
  if(CheckSettingsDirectory()) {
    Profile *p=new Profile();
    p->setSource(GetSettingsFilename());
    gui_server_dialog->load(p);
    gui_codec_dialog->load(p);
    gui_source_dialog->load(p);
    gui_stream_dialog->load(p);
    delete p;
  }
  checkArgs();
}


bool MainWidget::SaveSettings()
{
  FILE *f;

  if(CheckSettingsDirectory()) {
    QString basepath=GetSettingsFilename();
    if((f=fopen((basepath+".tmp").toUtf8(),"w"))==NULL) {
      return false;
    }
    fprintf(f,"[GlassGui]\n");
    gui_server_dialog->save(f);
    gui_codec_dialog->save(f);
    gui_source_dialog->save(f);
    gui_stream_dialog->save(f);
    fclose(f);
    rename((basepath+".tmp").toUtf8(),basepath.toUtf8());
  }

  return true;
}


bool MainWidget::CheckSettingsDirectory()
{
  QString path=QString("/")+GLASSGUI_SETTINGS_DIR;

  if(getenv("HOME")!=NULL) {
    path=QString(getenv("HOME"))+"/"+GLASSGUI_SETTINGS_DIR;
  }
  gui_settings_dir=new QDir(path);
  if(!gui_settings_dir->exists()) {
    mkdir(path.toUtf8(),
	  S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
    if(!gui_settings_dir->exists()) {
      return false;
    }
  }
  return true;
}


QString MainWidget::GetSettingsFilename()
{
  if(!CheckSettingsDirectory()) {
    QMessageBox::critical(this,"GlassGui - "+tr("Error"),
			  tr("Unable to create settings directory!"));
    exit(256);
  }
  QString ret=gui_settings_dir->path()+"/"+GLASSGUI_SETTINGS_FILE;
  if(!instance_name.isEmpty()) {
    ret+="-"+instance_name;
  }

  return ret;
}


void MainWidget::DeleteInstance(const QString &name)
{
  if(CheckSettingsDirectory()) {
    unlink((gui_settings_dir->path()+"/"+GLASSGUI_SETTINGS_FILE+"-"+name).toUtf8());
  }
}


void MainWidget::ListInstances()
{
  if(CheckSettingsDirectory()) {
    QStringList files=gui_settings_dir->
      entryList(QStringList(QString(GLASSGUI_SETTINGS_FILE)+"-*"),
		QDir::Files,QDir::Name);
    for(int i=0;i<files.size();i++) {
      printf("%s\n",
	     (const char *)files[i].right(files[i].length()-
	                   sizeof(GLASSGUI_SETTINGS_FILE)-1+1).toUtf8());
    }
  }
}


int main(int argc,char *argv[])
{
  QApplication a(argc,argv);
  MainWidget *w=new MainWidget();
  w->show();
  return a.exec();
}
