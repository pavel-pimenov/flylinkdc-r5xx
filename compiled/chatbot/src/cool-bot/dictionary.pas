unit dictionary;

interface

uses
  SysUtils, Classes, Windows;

type
  TPhrase = record
    id, basePrio: integer;
    match: string;
    phrase: string;
  end;

  TPhrases = array of TPhrase;
  TBoolArray = array of boolean;

type
  TDict = class
  private
    function getPhraseCount: integer;
  public
    allphrases: TPhrases;
    property count: integer read getPhraseCount;
    function getMatched(const msg: WideString; const used: TBoolArray): TPhrases;
    class function GetRandom(const  from: TPhrases): TPhrase;
    constructor init(const ini: string);
  end;

var
  dict: TDict;

implementation

function TDict.getPhraseCount: integer;
begin
  result := length(allphrases);
end;

function TDict.getMatched(const msg: WideString; const used: TBoolArray): TPhrases;
var
  i,dst: integer;
  t: WideString;
begin
  t := AnsiLowerCase(msg);
  result := nil; dst := 0;
  for i := 0 to count-1 do begin
    if (used[i]) then continue;
    if (allphrases[i].match = '') or (pos(allphrases[i].match,t) > 0) then begin
      SetLength(result, dst+1);
      result[dst] := allphrases[i];
      inc(dst);
    end;
  end;
end;

class function TDict.GetRandom(const from: TPhrases): TPhrase;
var
  max, msg, i: integer;
begin
  max := 0;
  for i := low(from) to high(from) do
    max := max + from[i].basePrio;
  msg := random(max);
  if (msg >= max) then msg := max-1;
  i := 0; max := 0;
  while (true) do begin
    max := max + from[i].basePrio;
    if (msg < max) then break
    else inc(i);
  end;
  result := from[i];
end;

constructor TDict.init(const ini: string);
var
  f: TextFile;
  s, match: string;
  dst, ps, prio: integer;
begin
  allphrases := nil;
  AssignFile(f, ini);
  Reset(f);
  try
    dst := 0;
    while not EOF(f) do begin
      Readln(f, s);
      if (length(s) < 3) or (s[1] = ';') then continue;
      ps := pos('/', s);
      if (ps <= 0) then continue;
      prio := StrToInt(copy(s, 1, ps-1));
      delete(s, 1, ps);
      ps := pos('/', s);
      if (ps <= 0) then continue;
      match := copy(s, 1, ps-1);
      delete(s, 1, ps);
      SetLength(allphrases, dst+1);
      allphrases[dst].id := dst;
      allphrases[dst].basePrio := prio;
      allphrases[dst].match := AnsiLowerCase(match);
      allphrases[dst].phrase := s;
      inc(dst);
    end;
  finally
    CloseFile(f);
  end;
end;

end.
