unit bot;

interface

uses
  SysUtils, Classes, Windows;

type
  tSendMessage = procedure(params: PWideChar; message: PWideChar); stdcall;
  tRecvMessage = procedure(params: PWideChar; message: PWideChar); stdcall;

type
  TBotInit = record
    apiVersion:         DWORD;
    appName:            PCHAR;
    appVersion:         PCHAR;
    OnSendMessage:      tSendMessage;
    OnRecvMessage:      tRecvMessage;
    botId:              PCHAR;
    botVersion:         PCHAR;
  end;

var
  SendProc: tSendMessage;

procedure OnRecvMessage(params: PWideChar; message: PWideChar); stdcall;
function init(var _init: TBotInit): boolean; stdcall;
exports init;

implementation

procedure OnRecvMessage(params: PWideChar; message: PWideChar); stdcall;
begin
  //SendProc(params, 'oops! лажа');
  SendProc(params, message);
end;


function init(var _init: TBotInit): boolean; stdcall;
begin
  _init.botId := 'repeater';
  _init.botVersion := '1.0';
  _init.OnRecvMessage := OnRecvMessage;
  SendProc := _init.OnSendMessage;
  result := true;
end;


end.