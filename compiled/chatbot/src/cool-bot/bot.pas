unit bot;

interface

uses
  SysUtils, Classes, Windows, Session;

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

uses
  dictionary, tray;

procedure OnRecvMessage(params: PWideChar; message: PWideChar); stdcall;
var
  cid, answer: WideString;
  i,sid: integer;
begin
  if (TrayForm <> nil) and (not TrayForm.State) then exit;

  try
    cid := TUserSession.getCID(params);
    sid := -1;
    for i := 0 to length(sessions)-1 do
      if (sessions[i].cid = cid) then sid := i;
    if (sid < 0) then begin
      sid := length(Sessions);
      SetLength(Sessions, sid+1);
      Sessions[sid] := TUserSession.Create(cid);
    end;
    answer := Sessions[sid].handleMessage(params, message);
    if (answer <> '') then SendProc(params, pWideChar(answer));
  except
    on e: exception do begin
      SendProc(params, pWideChar(WideString('ChatBot.dll failed: '+e.message)));
      Windows.Beep(800, 50);
    end;
  end;
end;

function init(var _init: TBotInit): boolean; stdcall;
var
  exe: array [0..512] of char;
begin
  Randomize;
  result := true;
  _init.botId := 'simple bot';
  _init.botVersion := '1.0';
  _init.OnRecvMessage := OnRecvMessage;
  SendProc := _init.OnSendMessage;
  try
    GetModuleFilename(0, exe, 512);
    dict := TDict.init(ExtractFilePath(string(exe)) + 'ChatBot.ini');
  except
    on e: exception do
    begin
      Windows.Beep(400, 50);
      Windows.MessageBox(0, pchar('can''t init ChatBot.dll:'#13#10+e.message), 'error', MB_OK or MB_ICONERROR);
      result := false;
    end;
  end;
end;

end.