unit session;

interface

uses
  SysUtils, Classes, Windows, Dictionary;

type
  TUserSession = class
  public
    cid: WideString;
    params: TStringList;
    function handleMessage(const params, msg: WideString): WideString;
    class function getCID(const params: WideString): WideString;
    constructor Create(cid: WideString);
    destructor Done;
  private
    hShutdown: THandle;
    replies: array of WideString;
    used: TBoolArray;
    function processVariables(const msg: WideString): WideString;
    function GetVariable(const v: WideString): WideString;
    procedure SendDelayedMsg(const msg: WideString; timeout: integer);
  end;

var
  sessions: array of TUserSession;

implementation

uses bot;

destructor TUserSession.done;
begin
  SetEvent(hShutdown);
  Params.Free;
  Sleep(200);
  CloseHandle(hShutdown);
end;

constructor TUserSession.Create(cid: WideString);
begin
  hShutdown := CreateEvent(nil, true, false, nil);
  Self.cid := cid;
  replies := nil;
  SetLength(used, dict.count);
  params := TStringList.Create;
end;

function TUserSession.GetVariable(const v: WideString): WideString;
var
  id, res: WideString;
begin
  id := AnsiUpperCase(v);
  if (id = 'LAST') then res := replies[length(replies)-1]
  else if (id = 'HISTORY') then res := replies[random(length(replies))]
  else res := params.Values[id];
  result := res;
end;

class function TUserSession.getCID(const params: WideString): WideString;
var
  i: integer;
begin
  i := pos('CID=', params);
  if (i > 0) and (i+39+4 < length(params)) then
    result := copy(params, i+4, 39)
  else
    result := '';
end;

function TUserSession.handleMessage(const params, msg: WideString): WideString;
var
  possible: TPhrases;
  current: TPhrase;
  i, timeout: integer;
begin
  SetLength(replies, length(replies)+1);
  replies[length(replies)-1] := msg;
  Self.Params.Text := StringReplace(params, '|', #13#10, [rfReplaceAll]);

  result := '';
  if (GetVariable('BOT') = '1') then exit;

  possible := dict.GetMatched(msg, used);
  if (length(possible) = 0) then begin
    for i := low(used) to high(used) do
      used[i] := false;
    possible := dict.GetMatched(msg, used);
  end;

  current := dict.GetRandom(possible);
  used[current.id] := true;

  result := processVariables(current.phrase);
  if (random(10) > 8) then exit;

  timeout := length(result)*100;
  if (random(5) = 0) then inc(timeout, 6000);
  SendDelayedMsg(result, timeout);
  result := '';
end;

function TUserSession.processVariables(const msg: WideString): WideString;
var
  i, ps: integer;
  varname: string;
begin
  result := '';
  i := 1;
  while (i <= length(msg)) do begin
    if (i > length(msg)-3) or (msg[i] <> '$') and (msg[i+1] <> '(') then
      result := result + msg[i]
    else begin
      ps := i+2;
      while (ps <= length(msg)) and (msg[ps] <> ')') do inc(ps);
      if (ps > length(msg)) then
        result := result + msg[i]
      else begin
        varname := copy(msg, i+2, ps-i-2);
        result := result + GetVariable(varname);
        i := ps;
      end;
    end;
    inc(i);
  end;
end;

type
  TMsgRecord = record
    msg, params: WideString;
    timeout: integer;
    waitHandle: THandle;
  end;

procedure DelayedMsgThread(param: pointer); stdcall;
var
  r: ^TMsgRecord;
begin
  r := param;
  if (WaitForSingleObject(r.waitHandle, r.timeout) = WAIT_TIMEOUT) and (Assigned(SendProc)) then 
    SendProc(pWideChar(r.params), pWideChar(r.msg));
  Dispose(r);
end;

procedure TUserSession.SendDelayedMsg(const msg: WideString; timeout: integer);
var
  r: ^TMsgRecord;
  tid: DWORD;
begin
  New(r);
  r.msg := msg;
  r.params := 'CID='+cid;
  r.timeout := timeout;
  r.waitHandle := hShutDown;
  CloseHandle(CreateThread(nil, 0, @DelayedMsgThread, r, 0, tid));
end;

procedure ShutDown;
var i: integer;
begin
  for i := 0 to length(sessions)-1 do
    sessions[i].Free;
  sessions := nil;
end;

initialization
  sessions := nil;
finalization
  ShutDown;
end.
